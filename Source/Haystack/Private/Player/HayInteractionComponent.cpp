// Copyright (c) 2026 Vanan Andreas.

#include "Player/HayInteractionComponent.h"
#include "HayPile.h"
#include "Haystack.h"
#include "HayPile/HayFallingPiece.h"
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
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayInteractionComponent)

UHayInteractionComponent::UHayInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	FallingPieceClass = AHayFallingPiece::StaticClass();
}

void UHayInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHayInteractionComponent, HeldPiece, Params);
}

void UHayInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	for (AHayPile* WorldPile : TActorRange<AHayPile>(GetWorld()))
	{
		Pile = WorldPile;
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
		RequestDrop();
	}
	else if (HoveredPiece != INDEX_NONE)
	{
		RequestTake(HoveredPiece);
	}
}

void UHayInteractionComponent::RequestTake(const int32 PieceIndex)
{
	if (Pile && HeldPiece == INDEX_NONE && PieceIndex != INDEX_NONE)
	{
		Server_Take(PieceIndex);
	}
}

void UHayInteractionComponent::RequestDrop()
{
	if (Pile && HeldPiece != INDEX_NONE)
	{
		Server_Drop(HeldPiece);
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
	MARK_PROPERTY_DIRTY_FROM_NAME(UHayInteractionComponent, HeldPiece, this);
	OnRep_HeldPiece();

	if (const APlayerState* PlayerState = Cast<APawn>(GetOwner())->GetPlayerState())
	{
		PieceState->NotifyPieceTaken(PieceIndex, PlayerState->GetUniqueId());
	}
}

void UHayInteractionComponent::Server_Drop_Implementation(const int32 PieceIndex)
{
	if (!Pile || HeldPiece != PieceIndex)
	{
		return;
	}
	if (!FallingPieceClass)
	{
		UE_LOG(LogHay, Error, TEXT("%s: no FallingPieceClass set on HayInteraction, cannot drop"), *GetOwner()->GetName());
		return;
	}

	// Release from where the held mesh sits in front of the eyes, thrown along the view on top of the pawn's velocity.
	APawn*			 Pawn = Cast<APawn>(GetOwner());
	const FQuat		 View = Pawn->GetControlRotation().Quaternion();
	const FTransform Release(View * HeldRotation.Quaternion(), Pawn->GetPawnViewLocation() + View.RotateVector(HeldOffset));
	const FVector	 Velocity = Pawn->GetVelocity() + View.GetForwardVector() * ThrowSpeed;

	const UHayRenderComponent* Render = Pile->GetRender();
	const bool				   bNeedle = PieceIndex == Pile->GetPieceState()->GetNeedlePiece();
	UStaticMesh*			   Mesh = bNeedle && Render->NeedleMesh ? Render->NeedleMesh : Render->HayMesh;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Pawn;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHayFallingPiece* Falling = GetWorld()->SpawnActor<AHayFallingPiece>(FallingPieceClass, Release, SpawnParameters);
	Falling->Launch(Pile, PieceIndex, Mesh, FVector(Render->GetPieceHalfExtents()), Velocity);

	HeldPiece = INDEX_NONE;
	MARK_PROPERTY_DIRTY_FROM_NAME(UHayInteractionComponent, HeldPiece, this);
	OnRep_HeldPiece();
}

bool UHayInteractionComponent::IsWithinServerReach(const FVector& WorldLocation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return FVector::Dist(Pawn->GetPawnViewLocation(), WorldLocation) <= Reach + ServerReachSlack;
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