// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HayFallingPiece.generated.h"

class AHayPile;
class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A dropped piece while it falls. Exists only between the hand and the ground.
 * The server owns the body, replicates its movement, and once it sleeps writes the rest transform back into the pile and
 * destroys this actor.
 */
UCLASS(Blueprintable, HideCategories = (Input, Collision, Rendering, HLOD, Physics, Networking, LevelInstance, Cooking, DataLayers, WorldPartition))
class AHayFallingPiece : public AActor
{
	GENERATED_BODY()

public:
	AHayFallingPiece();

	/**
	 * A piece still moving after this many seconds is placed where it is.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 1))
	float MaxFallSeconds = 10.f;

	/**
	 * Server only, right after spawning.
	 */
	void Launch(AHayPile* InPile, const int32 InPieceIndex, UStaticMesh* InMesh, const FVector& InHalfExtents, const FVector& InitialVelocity);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

private:
	UFUNCTION()
	void OnRep_Shape();

	UFUNCTION()
	void OnBodySleep(UPrimitiveComponent* SleepingComponent, FName BoneName);

	/**
	 * Hay sticks to the haystack instead of sliding off it, so a hit against the pile lands the piece at once.
	 */
	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	/**
	 * Writes the current transform into the pile and destroys the actor. Server only, once.
	 */
	void Land();

	UPROPERTY(VisibleAnywhere, Category = Hay)
	TObjectPtr<UBoxComponent> Body = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Hay)
	TObjectPtr<UStaticMeshComponent> Visual = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_Shape)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_Shape)
	FVector HalfExtents = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<AHayPile> Pile = nullptr;

	int32 PieceIndex = INDEX_NONE;

	FTimerHandle LandTimer = {};

	bool bLanded = false;
};
