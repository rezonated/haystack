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
 * Pieces are stored in bucket order over a coarse grid of the cell's bounds, so a ray only touches the buckets it passes.
 */
struct FHayCellPick
{
	/**
	 * Piece centers grown by the piece bound radius. The cell level ray test.
	 */
	FBox3f Bounds = FBox3f(ForceInit);

	/**
	 * Corner of bucket (0, 0, 0). Pieces bucket by floor((Location - GridOrigin) / BucketSize).
	 */
	FVector3f GridOrigin = FVector3f::ZeroVector;

	FIntVector GridSize = FIntVector::ZeroValue;

	/**
	 * Slot range of bucket b is [BucketStart[b], BucketStart[b + 1]). GridSize.X * Y * Z + 1 entries.
	 */
	TArray<int32> BucketStart = {};

	/**
	 * Per slot, in bucket order.
	 */
	TArray<FVector3f> Locations = {};

	TArray<FQuat4f> Rotations = {};

	/**
	 * Piece offset within the cell per slot, to get back to the piece index.
	 */
	TArray<int32> PieceOffsets = {};
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
UCLASS(ClassGroup = Hay, MinimalAPI, meta = (BlueprintSpawnableComponent))
class UHayPickComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * Edge length of a pick grid bucket, cm. Smaller buckets mean fewer pieces per ray but more buckets to walk.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 10))
	float PickBucketSize = 50.f;

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

	/**
	 * Ray against one piece in pile space.
	 * A sphere test first, since almost every piece fails it, then the exact slab test in the piece's own axes.
	 * On a hit, InOutBestDistance shrinks to the entry distance.
	 */
	static HAYSTACK_API bool RayHitsPiece(const FVector3f& Origin, const FVector3f& Direction, const FVector3f& PieceLocation, const FQuat4f& PieceRotation, const FVector3f& HalfExtents, const float BoundRadius, float& InOutBestDistance);

private:
	/**
	 * Builds the cell's bucket grid with a counting sort over its pieces.
	 */
	void OnCellSpawned(const int32 CellIndex, TArrayView<const FTransform> LocalTransforms);

	/**
	 * Bucket index of a location, clamped to the grid.
	 */
	static FIntVector BucketOf(const FHayCellPick& Pick, const FVector3f& Location, const float InverseBucketSize);

	static int32 FlatBucket(const FIntVector& GridSize, const FIntVector& Bucket) { return (Bucket.Z * GridSize.Y + Bucket.Y) * GridSize.X + Bucket.X; }

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