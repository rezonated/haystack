// Copyright (c) 2026 Vanan Andreas.

#include "HayPile/HayFallingPiece.h"
#include "HayPile.h"
#include "HayPile/HayPieceStateComponent.h"

#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayFallingPiece)

AHayFallingPiece::AHayFallingPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	// The box is the physics body. Clients simulate too and get corrected by movement replication.
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetSimulatePhysics(true);
	Body->BodyInstance.bGenerateWakeEvents = true;
	// A 1 cm slab at throw speed crosses more than its thickness per step. Continuous collision keeps it from tunneling.
	Body->BodyInstance.bUseCCD = true;
	Body->SetNotifyRigidBodyCollision(true);
	RootComponent = Body;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	Visual->SetupAttachment(Body);
}

void AHayFallingPiece::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHayFallingPiece, Mesh);
	DOREPLIFETIME(AHayFallingPiece, HalfExtents);
}

void AHayFallingPiece::Launch(AHayPile* InPile, const int32 InPieceIndex, UStaticMesh* InMesh, const FVector& InHalfExtents, const FVector& InitialVelocity)
{
	Pile = InPile;
	PieceIndex = InPieceIndex;
	Mesh = InMesh;
	HalfExtents = InHalfExtents;
	OnRep_Shape();

	Body->OnComponentSleep.AddDynamic(this, &AHayFallingPiece::OnBodySleep);
	Body->OnComponentHit.AddDynamic(this, &AHayFallingPiece::OnBodyHit);
	Body->SetPhysicsLinearVelocity(InitialVelocity);
	GetWorldTimerManager().SetTimer(LandTimer, this, &AHayFallingPiece::Land, MaxFallSeconds, false);
}

void AHayFallingPiece::OnRep_Shape()
{
	Visual->SetStaticMesh(Mesh);
	Body->SetBoxExtent(HalfExtents);
}

void AHayFallingPiece::OnBodySleep(UPrimitiveComponent* SleepingComponent, FName BoneName)
{
	Land();
}

void AHayFallingPiece::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor == Pile)
	{
		Land();
	}
}

void AHayFallingPiece::FellOutOfWorld(const UDamageType& DamageType)
{
	Land();
}

void AHayFallingPiece::Land()
{
	if (bLanded || !HasAuthority())
	{
		return;
	}
	bLanded = true;

	if (Pile)
	{
		Pile->GetPieceState()->PlacePiece(PieceIndex, Body->GetComponentTransform());
	}
	Destroy();
}
