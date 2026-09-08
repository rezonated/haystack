// Copyright (c) 2026 Vanan Andreas.

#include "HayInteractionComponent.h"
#include "HayPickComponent.h"
#include "HayPieceStateComponent.h"
#include "HayPile.h"
#include "HayRenderComponent.h"
#include "Haystack.h"

#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayInteractionComponent)

UHayInteractionComponent::UHayInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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

	UStaticMesh* HayMesh = Pile->GetRender()->HayMesh;

	// The proxy writes custom depth only.
	// It bypasses Nanite so the main pass skip applies.
	HoverProxy = NewObject<UStaticMeshComponent>(Pawn, TEXT("HayHoverProxy"));
	HoverProxy->SetStaticMesh(HayMesh);
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

	HeldMesh = NewObject<UStaticMeshComponent>(Pawn, TEXT("HayHeldMesh"));
	HeldMesh->SetStaticMesh(HayMesh);
	HeldMesh->SetCastShadow(false);
	HeldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldMesh->SetVisibility(false);
	HeldMesh->SetupAttachment(Camera);
	HeldMesh->SetRelativeLocationAndRotation(HeldOffset, HeldRotation);
	HeldMesh->RegisterComponent();
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

	UHayPieceStateComponent* PieceState = Pile->GetPieceState();
	if (HeldPiece != INDEX_NONE)
	{
		FTransform DropTransform;
		if (FindDropTransform(DropTransform) && PieceState->PlacePiece(HeldPiece, DropTransform))
		{
			HeldPiece = INDEX_NONE;
			HeldMesh->SetVisibility(false);
		}
		return;
	}

	if (HoveredPiece != INDEX_NONE && PieceState->TakePiece(HoveredPiece))
	{
		HeldPiece = HoveredPiece;
		HoveredPiece = INDEX_NONE;
		HoverProxy->SetVisibility(false);
		HeldMesh->SetVisibility(true);
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