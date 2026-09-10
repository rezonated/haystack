// Copyright (c) 2026 Vanan Andreas.

#include "Testing/HayDigBotComponent.h"
#include "HayPile.h"
#include "Haystack.h"
#include "HayPile/HayLayoutComponent.h"
#include "HayPile/HayPickComponent.h"
#include "HayPile/HayPieceStateComponent.h"
#include "Player/HayInteractionComponent.h"

#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayDigBotComponent)

static FAutoConsoleCommandWithWorldAndArgs HayDigBotCommand(TEXT("Hay.DigBot"), TEXT("Digging bot on the local player. No argument toggles, \"on\" or \"needle\" makes sure it runs, \"off\" removes it."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UHayDigBotComponent::ToggleOnLocalPlayer));

UHayDigBotComponent::UHayDigBotComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHayDigBotComponent::ToggleOnLocalPlayer(const TArray<FString>& Args, UWorld* World)
{
	const bool	   bTowardNeedle = Args.Contains(TEXT("needle"));
	const ERequest Request = Args.Contains(TEXT("off")) ? ERequest::Off : (bTowardNeedle || Args.Contains(TEXT("on"))) ? ERequest::On : ERequest::Toggle;
	if (Apply(World, Request, bTowardNeedle))
	{
		return;
	}

	// From -ExecCmds on a client the connection is still pending and no controller exists yet. Poll until one does.
	UE_LOG(LogHay, Log, TEXT("Hay.DigBot: no local player controller yet, waiting"));
	const double GiveUpSeconds = FPlatformTime::Seconds() + WaitForControllerSeconds;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Request, bTowardNeedle, GiveUpSeconds](float)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World() && Apply(Context.World(), Request, bTowardNeedle))
			{
				return false;
			}
		}
		if (FPlatformTime::Seconds() > GiveUpSeconds)
		{
			UE_LOG(LogHay, Error, TEXT("Hay.DigBot: no local player controller after %.0f s, giving up"), WaitForControllerSeconds);
			return false;
		}
		return true;
	}), WaitForControllerPollSeconds);
}

bool UHayDigBotComponent::Apply(UWorld* World, const ERequest Request, const bool bTowardNeedle)
{
	// A controller without a pawn is the transition map's placeholder on a connecting client. It is destroyed on travel.
	APlayerController* Controller = World->GetFirstPlayerController();
	if (!Controller || !Controller->IsLocalController() || !Controller->GetPawn())
	{
		return false;
	}

	UHayDigBotComponent* Existing = Controller->FindComponentByClass<UHayDigBotComponent>();
	if (Existing && Request != ERequest::On)
	{
		Existing->DestroyComponent();
		UE_LOG(LogHay, Log, TEXT("DigBot off on %s"), *Controller->GetName());
	}
	else if (!Existing && Request != ERequest::Off)
	{
		UHayDigBotComponent* Bot = NewObject<UHayDigBotComponent>(Controller, TEXT("HayDigBot"));
		Bot->bDigTowardNeedle = bTowardNeedle;
		Bot->RegisterComponent();
		UE_LOG(LogHay, Log, TEXT("DigBot on %s, %s"), *Controller->GetName(), bTowardNeedle ? TEXT("toward the needle") : TEXT("random spots"));
	}
	return true;
}

void UHayDigBotComponent::BeginPlay()
{
	Super::BeginPlay();

	for (AHayPile* WorldPile : TActorRange<AHayPile>(GetWorld()))
	{
		Pile = WorldPile;
		break;
	}
	if (!Pile)
	{
		UE_LOG(LogHay, Error, TEXT("%s: DigBot found no HayPile"), *GetOwner()->GetName());
		SetComponentTickEnabled(false);
		return;
	}

	StartSeconds = FPlatformTime::Seconds();

	UHayPieceStateComponent* PieceState = Pile->GetPieceState();
	PieceState->OnNeedleFound.AddUObject(this, &UHayDigBotComponent::OnNeedleFound);
	if (PieceState->IsNeedleFound())
	{
		OnNeedleFound(PieceState->GetNeedleFoundBy());
	}
}

void UHayDigBotComponent::OnNeedleFound(const FUniqueNetIdRepl& Player)
{
	UE_LOG(LogHay, Log, TEXT("DigBot: needle found after %d takes at %d spots, %.1f s"), Takes, Spots, FPlatformTime::Seconds() - StartSeconds);
	SetComponentTickEnabled(false);
}

void UHayDigBotComponent::ChooseSpot(const APawn* Pawn, const float Reach)
{
	const FTransform& PileToWorld = Pile->GetActorTransform();
	const float		  Radius = Pile->GetLayout()->DomeRadius;
	const FVector	  EyeOffset = Pawn->GetPawnViewLocation() - Pawn->GetActorLocation();

	// Random spots are rerolled a few times until the eyes at the standing point can reach them. The needle is taken as is.
	constexpr int32 Rerolls = 16;
	for (int32 Attempt = 0; Attempt < Rerolls; ++Attempt)
	{
		FVector Direction;
		if (bDigTowardNeedle)
		{
			const FVector NeedleLocal = Pile->GetLayout()->GetPieceLocalTransform(Pile->GetPieceState()->GetNeedlePiece()).GetLocation();
			DigSpot = PileToWorld.TransformPosition(NeedleLocal);
			Direction = NeedleLocal.GetSafeNormal();
		}
		else
		{
			Direction = FMath::VRand();
			Direction.Z = FMath::Abs(Direction.Z);
			DigSpot = PileToWorld.TransformPosition(Direction * Radius);
		}

		StandPoint = PileToWorld.TransformPosition(Direction.GetSafeNormal2D() * (Radius + StandDistance));
		StandPoint.Z = Pawn->GetActorLocation().Z;

		if (bDigTowardNeedle || FVector::Dist(StandPoint + EyeOffset, DigSpot) <= Reach)
		{
			break;
		}
	}

	LastAimPoint = DigSpot;
	bAtStandPoint = false;
	bMoveRequested = false;
	SecondsWithoutProgress = 0.f;
	BestDistanceToStand = TNumericLimits<float>::Max();
	TakesAtSpot = 0;
	MissesAtSpot = 0;
	++Spots;

	UE_LOG(LogHay, Log, TEXT("DigBot: spot %d at %s, standing at %s, %.0f cm from the eyes"), Spots, *DigSpot.ToCompactString(), *StandPoint.ToCompactString(), FVector::Dist(StandPoint + EyeOffset, DigSpot));
}

bool UHayDigBotComponent::WalkToStandPoint(APawn* Pawn, const float DeltaTime)
{
	const FVector ToStand = StandPoint - Pawn->GetActorLocation();
	const float	  Distance = ToStand.Size2D();
	if (Distance <= ArriveRadius)
	{
		return true;
	}

	// Progress check, so a wall or the dome between here and there does not stall the run.
	if (Distance < BestDistanceToStand - 1.f)
	{
		BestDistanceToStand = Distance;
		SecondsWithoutProgress = 0.f;
	}
	else
	{
		SecondsWithoutProgress += DeltaTime;
	}

	APlayerController*	   Controller = Cast<APlayerController>(GetOwner());
	UNavigationSystemV1*   NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool			   bHasNavMesh = NavSystem && NavSystem->GetDefaultNavDataInstance();
	if (bHasNavMesh)
	{
		if (!bMoveRequested)
		{
			UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, StandPoint);
			bMoveRequested = true;
		}
	}
	else
	{
		Pawn->AddMovementInput(ToStand.GetSafeNormal2D());
	}

	// Look where the feet go.
	const FVector Velocity = Pawn->GetVelocity();
	Controller->SetControlRotation((Velocity.SizeSquared2D() > 1.f ? Velocity : ToStand).GetSafeNormal2D().Rotation());

	return false;
}

FVector UHayDigBotComponent::NextAimPoint()
{
	if (AimSpreadRadius <= 0.f)
	{
		return DigSpot;
	}

	// A handful of draws is enough to land at least half the radius away from the last aim.
	constexpr int32 Draws = 8;
	FVector			Point = DigSpot;
	for (int32 Draw = 0; Draw < Draws; ++Draw)
	{
		Point = DigSpot + FMath::VRand() * FMath::FRandRange(0.f, AimSpreadRadius);
		if (FVector::Dist(Point, LastAimPoint) >= AimSpreadRadius * 0.5f)
		{
			break;
		}
	}

	LastAimPoint = Point;
	return Point;
}

void UHayDigBotComponent::Dig(APawn* Pawn, UHayInteractionComponent* Interaction, const double Now)
{
	const FVector  Eye = Pawn->GetPawnViewLocation();
	const FRotator Aim = (NextAimPoint() - Eye).Rotation();
	Cast<APlayerController>(GetOwner())->SetControlRotation(Aim);

	FHayPickResult Hit;
	if (!Pile->GetPick()->RayPick(Eye, Aim.Vector(), Interaction->Reach, Hit))
	{
		if (++MissesAtSpot >= MissesPerSpot)
		{
			ChooseSpot(Pawn, Interaction->Reach * ReachFraction);
		}
		return;
	}

	Interaction->RequestTake(Hit.PieceIndex);
	PendingTake = Hit.PieceIndex;
	PendingSinceSeconds = Now;
}

void UHayDigBotComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn*					  Pawn = Cast<APlayerController>(GetOwner())->GetPawn();
	UHayInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UHayInteractionComponent>() : nullptr;
	if (!Pile || !Interaction || Pile->GetPieceState()->GetNeedlePiece() == INDEX_NONE)
	{
		return;
	}

	if (Spots == 0)
	{
		ChooseSpot(Pawn, Interaction->Reach * ReachFraction);
	}

	if (!bAtStandPoint)
	{
		bAtStandPoint = WalkToStandPoint(Pawn, DeltaTime);
		if (bAtStandPoint)
		{
			DiggingSinceSeconds = FPlatformTime::Seconds();
			UE_LOG(LogHay, Log, TEXT("DigBot: arrived at spot %d, digging"), Spots);
		}
		else if (SecondsWithoutProgress > StuckSeconds)
		{
			UE_LOG(LogHay, Warning, TEXT("DigBot: no progress toward the standing point, choosing a new spot"));
			ChooseSpot(Pawn, Interaction->Reach * ReachFraction);
		}
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const int32	 HeldPiece = Interaction->GetHeldPiece();

	if (bPendingDrop)
	{
		if (HeldPiece == INDEX_NONE)
		{
			bPendingDrop = false;
			NextActionSeconds = Now + TakeInterval;
		}
		else if (Now - PendingSinceSeconds > RequestTimeout)
		{
			UE_LOG(LogHay, Warning, TEXT("DigBot: server did not confirm the throw of piece %d, asking again"), HeldPiece);
			bPendingDrop = false;
		}
		return;
	}

	if (PendingTake != INDEX_NONE)
	{
		if (HeldPiece == PendingTake)
		{
			PendingTake = INDEX_NONE;
			++Takes;
			++TakesAtSpot;
			NextActionSeconds = Now + HoldSeconds;
			if (Takes % ProgressLogEveryTakes == 0)
			{
				UE_LOG(LogHay, Log, TEXT("DigBot: %d takes, %d misses at spot %d, %.0f s"), Takes, MissesAtSpot, Spots, Now - StartSeconds);
			}
		}
		else if (Now - PendingSinceSeconds > RequestTimeout)
		{
			UE_LOG(LogHay, Warning, TEXT("DigBot: server did not confirm take of piece %d"), PendingTake);
			PendingTake = INDEX_NONE;
			++MissesAtSpot;
		}
		return;
	}

	if (Now < NextActionSeconds)
	{
		return;
	}

	if (HeldPiece != INDEX_NONE)
	{
		// Throw away from the pile so the piece does not land back in the hole.
		const FVector Away = (Pawn->GetActorLocation() - Pile->GetActorLocation()).GetSafeNormal2D();
		Cast<APlayerController>(GetOwner())->SetControlRotation(Away.Rotation());
		Interaction->RequestDrop();
		bPendingDrop = true;
		PendingSinceSeconds = Now;

		if (!bDigTowardNeedle && Now - DiggingSinceSeconds >= SecondsPerSpot)
		{
			ChooseSpot(Pawn, Interaction->Reach * ReachFraction);
		}
		return;
	}

	Dig(Pawn, Interaction, Now);
}
