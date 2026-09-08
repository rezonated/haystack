// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HayPickComponent.generated.h"

class UHayLayoutComponent;
class UHayPieceStateComponent;
class UHayRenderComponent;

/**
 * Ray pick data for one spawned cell.
 */
struct FHayCellPick
{
	FBox3f Bounds = FBox3f(ForceInit);

	TArray<FVector3f> Locations = {};

	TArray<FQuat4f> Rotations = {};
};

struct FHayPickResult
{
	int32 PieceIndex = INDEX_NONE;

	float Distance = 0.f;

	FTransform WorldTransform = FTransform::Identity;
};

/**
 * Finds the piece under a ray. Keeps its own float copy of every spawned
 * piece transform, so picking never touches the renderer or regenerates
 * transforms.
 */
UCLASS(ClassGroup = Hay, meta = (BlueprintSpawnableComponent))
class UHayPickComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * Needs layout, render and piece state on the owner.
	 * Subscribes to cell spawns, so call it before the first cell spawns.
	 */
	void Initialize();

	/**
	 * Nearest piece along a world ray, among spawned pieces still in the pile and loose pieces on the ground.
	 * Held pieces are never hit.
	 */
	bool RayPick(const FVector& WorldOrigin, const FVector& WorldDirection, const float MaxDistance, FHayPickResult& OutResult);

private:
	void OnCellSpawned(const int32 CellIndex, TArrayView<const FTransform> LocalTransforms);

	/**
	 * Ray against one piece in pile space.
	 * A sphere test first, since almost every piece fails it, then the exact slab test in the piece's own axes.
	 * On a hit, InOutBestDistance shrinks to the entry distance.
	 */
	static bool RayHitsPiece(const FVector3f& Origin, const FVector3f& Direction, const FVector3f& PieceLocation, const FQuat4f& PieceRotation, const FVector3f& HalfExtents, const float BoundRadius, float& InOutBestDistance);

	UPROPERTY(Transient)
	TObjectPtr<UHayLayoutComponent> Layout = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHayRenderComponent> Render = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHayPieceStateComponent> State = nullptr;

	/**
	 * One entry per cell, empty until spawned.
	 */
	TArray<FHayCellPick> CellPick = {};

	/**
	 * Cells the current ray enters, reused every call.
	 */
	TArray<TPair<float, int32>> PickCandidates = {};

	/**
	 * Radius of the sphere around a piece center that encloses the whole piece.
	 */
	float PieceBoundRadius = 0.f;
};