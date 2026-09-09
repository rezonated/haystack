// Copyright (c) 2026 Vanan Andreas.

#include "Player/HayInteractionComponent.h"
#include "HayPile.h"
#include "Haystack.h"
#include "HayPile/HayPickComponent.h"
#include "HayPile/HayPieceStateComponent.h"
#include "HayPile/HayRenderComponent.h"

#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayInteractionComponent)

UHayInteractionComponent::UHayInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UHayInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHayInteractionComponent, HeldPiece);
}

void UHayInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<AHayPile> It(GetWorld()); It; ++It)
	{
		Pile = *It;
		break;
	}
	if (!Pile)
	{
		UE_LOG(LogHay, Error, TEXT("%s: no HayPile in the level"), *GetOwner()->GetName());
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		UE_LOG(LogHay, Error, TEXT("%s: HayInteraction must sit on a Pawn"), *GetOwner()->GetName());
		return;
	}

	// Every pawn gets a held mesh in its hand.
	// The local pawn moves it to the camera once it knows it is local.
	HeldMesh = NewObject<UStaticMeshComponent>(Pawn, TEXT("HayHeldMesh"));
	HeldMesh->SetStaticMesh(Pile->GetRender()->HayMesh);
	HeldMesh->SetCastShadow(false);
	HeldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (USkeletalMeshComponent* Body = Pawn->FindComponentByClass<USkeletalMeshComponent>())
	{
		HeldMesh->SetupAttachment(Body, HeldSocket);
		HeldMesh->SetRelativeTransform(HeldSocketOffset);
	}
	else
	{
		HeldMesh->SetupAttachment(Pawn->GetRootComponent());
	}
	HeldMesh->RegisterComponent();
	OnRep_HeldPiece();

	Pawn->ReceiveRestartedDelegate.AddDynamic(this, &UHayInteractionComponent::OnPawnRestarted);
	if (Pawn->IsLocallyControlled())
	{
		OnPawnRestarted(Pawn);
	}
}

void UHayInteractionComponent::OnPawnRestarted(APawn* Pawn)
{
	if (!Pawn->IsLocallyControlled() || !Pile)
	{
		return;
	}

	BindInput();

	if (Camera)
	{
		return;
	}

	Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (!Camera)
	{
		UE_LOG(LogHay, Error, TEXT("%s: HayInteraction needs a CameraComponent on the pawn"), *Pawn->GetName());
		return;
	}

	if (OutlineMaterial)
	{
		Camera->PostProcessSettings.AddBlendable(OutlineMaterial, 1.f);
	}
	else
	{
		UE_LOG(LogHay, Warning, TEXT("%s: no OutlineMaterial set, hover has no outline"), *Pawn->GetName());
	}

	HeldMesh->AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);
	HeldMesh->SetRelativeLocationAndRotation(HeldOffset, HeldRotation);

	// The proxy writes custom depth only.
	// It bypasses Nanite so the main pass skip applies.
	HoverProxy = NewObject<UStaticMeshComponent>(Pawn, TEXT("HayHoverProxy"));
	HoverProxy->SetStaticMesh(Pile->GetRender()->HayMesh);
	HoverProxy->bDisallowNanite = true;
	HoverProxy->SetRenderCustomDepth(true);
	HoverProxy->SetRenderInMainPass(false);
	HoverProxy->SetRenderInDepthPass(false);
	HoverProxy->SetCastShadow(false);
	HoverProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HoverProxy->SetVisibility(false);
	HoverProxy->SetAbsolute(true, true, true);
	HoverProxy->SetupAttachment(Pawn->GetRootComponent());
	HoverProxy->RegisterComponent();
}

void UHayInteractionComponent::BindInput()
{
	if (bInputBound)
	{
		return;
	}

	APawn*					 Pawn = Cast<APawn>(GetOwner());
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!Input)
	{
		return;
	}

	if (!InteractAction)
	{
		UE_LOG(LogHay, Error, TEXT("%s: no InteractAction set on HayInteraction"), *Pawn->GetName());
		return;
	}

	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &UHayInteractionComponent::Interact);
	bInputBound = true;
}

bool UHayInteractionComponent::GetViewRay(FVector& OutOrigin, FVector& OutDirection) const
{
	const APawn*			 Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller)
	{
		return false;
	}

	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(OutOrigin, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}

void UHayInteractionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (HoverProxy)
	{
		UpdateHover();
	}
}

void UHayInteractionComponent::UpdateHover()
{
	HoveredPiece = INDEX_NONE;

	FVector Origin, Direction;
	if (HeldPiece == INDEX_NONE && GetViewRay(Origin, Direction))
	{
		FHayPickResult Hit;
		if (Pile->GetPick()->RayPick(Origin, Direction, Reach, Hit))
		{
			HoveredPiece = Hit.PieceIndex;
			HoverProxy->SetWorldTransform(Hit.WorldTransform);
		}
	}

	HoverProxy->SetVisibility(HoveredPiece != INDEX_NONE);
}

void UHayInteractionComponent::Interact()
{
	if (!Pile)
	{
		return;
	}

	if (HeldPiece != INDEX_NONE)
	{
		FTransform DropTransform;
		if (FindDropTransform(DropTransform))
		{
			Server_Place(HeldPiece, DropTransform);
		}

		return;
	}

	if (HoveredPiece != INDEX_NONE)
	{
		Server_Take(HoveredPiece);
	}
}

void UHayInteractionComponent::Server_Take_Implementation(const int32 PieceIndex)
{
	if (!Pile || HeldPiece != INDEX_NONE)
	{
		return;
	}

	UHayPieceStateComponent* PieceState = Pile->GetPieceState();
	if (!IsWithinServerReach(PieceState->GetPieceWorldTransform(PieceIndex).GetLocation()) || !PieceState->TakePiece(PieceIndex))
	{
		return;
	}

	HeldPiece = PieceIndex;
	OnRep_HeldPiece();

	if (const APlayerState* PlayerState = Cast<APawn>(GetOwner())->GetPlayerState())
	{
		PieceState->NotifyPieceTaken(PieceIndex, PlayerState->GetUniqueId());
	}
}

void UHayInteractionComponent::Server_Place_Implementation(const int32 PieceIndex, const FTransform& WorldTransform)
{
	if (!Pile || HeldPiece != PieceIndex || !IsWithinServerReach(WorldTransform.GetLocation()) || !Pile->GetPieceState()->PlacePiece(PieceIndex, WorldTransform))
	{
		return;
	}

	HeldPiece = INDEX_NONE;
	OnRep_HeldPiece();
}

bool UHayInteractionComponent::IsWithinServerReach(const FVector& WorldLocation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return FVector::Dist(Pawn->GetPawnViewLocation(), WorldLocation) <= Reach * ServerReachTolerance;
}

void UHayInteractionComponent::OnRep_HeldPiece()
{
	if (HeldMesh && Pile)
	{
		const UHayRenderComponent* Render = Pile->GetRender();
		const bool				   bHoldingNeedle = HeldPiece != INDEX_NONE && HeldPiece == Pile->GetPieceState()->GetNeedlePiece();
		HeldMesh->SetStaticMesh(bHoldingNeedle && Render->NeedleMesh ? Render->NeedleMesh : Render->HayMesh);
		HeldMesh->SetVisibility(HeldPiece != INDEX_NONE);
	}

	if (HoverProxy && HeldPiece != INDEX_NONE)
	{
		HoveredPiece = INDEX_NONE;
		HoverProxy->SetVisibility(false);
	}
}

bool UHayInteractionComponent::FindDropTransform(FTransform& OutWorldTransform) const
{
	FVector Origin, Direction;
	if (!GetViewRay(Origin, Direction))
	{
		return false;
	}

	// Hay has no collision and the dome sphere ignores visibility, so this finds floor, walls and props.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HayDrop), /*bTraceComplex*/ false, GetOwner());
	FHitResult			  Hit;
	bool				  bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * Reach, ECC_Visibility, Params);
	if (!bHit)
	{
		// Looking at nothing within reach: drop straight down from a point in front of the pawn.
		const FVector Ahead = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 100.f;
		bHit = GetWorld()->LineTraceSingleByChannel(Hit, Ahead, Ahead - FVector(0.f, 0.f, 500.f), ECC_Visibility, Params);
	}
	if (!bHit)
	{
		return false;
	}

	// Lie flat on the surface with a random heading, thickness resting on it.
	const FVector Up = Hit.ImpactNormal;
	const FVector Heading = FRotationMatrix(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f)).GetUnitAxis(EAxis::X);
	const FQuat	  Rotation = FRotationMatrix::MakeFromZX(Up, Heading).ToQuat();
	const FVector Location = Hit.ImpactPoint + Up * Pile->GetRender()->GetPieceHalfExtents().Z;
	OutWorldTransform = FTransform(Rotation, Location);
	return true;
}