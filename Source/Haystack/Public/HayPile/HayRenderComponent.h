// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HayRenderComponent.generated.h"

class UHayLayoutComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Fired once per cell right after its instances exist, with the pile-local
 * transforms used to build them.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHayCellSpawned, int32 /*CellIndex*/, TArrayView<const FTransform> /*LocalTransforms*/);

/**
 * Puts the pile on screen. One instanced static mesh component per cell,
 * spawned a few per frame under a time budget. Only the outer shells exist at
 * start, deeper cells spawn when digging exposes them.
 */
UCLASS(ClassGroup = Hay, meta = (BlueprintSpawnableComponent))
class UHayRenderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHayRenderComponent();

	UPROPERTY(EditAnywhere, Category = Hay)
	TObjectPtr<UStaticMesh> HayMesh = nullptr;

	/**
	 * Drawn in place of the hay slab at the needle piece.
	 * Falls back to HayMesh when unset.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	TObjectPtr<UStaticMesh> NeedleMesh = nullptr;

	/**
	 * Shells spawned from the surface inward at start, and kept spawned below
	 * any dug spot.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 1))
	int32 VisibleShells = 2;

	/**
	 * Game thread milliseconds spent adding cells per frame.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0.5))
	float SpawnBudgetMs = 6.f;

	UPROPERTY(EditAnywhere, Category = Hay)
	bool bCastShadow = true;

	/**
	 * Whether pieces feed the Lumen scene. Off keeps the pile out of the
	 * surface cache.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	bool bAffectIndirectLighting = false;

#if WITH_EDITORONLY_DATA
	/**
	 * Log averaged frame, thread and GPU times to LogHay while spawning and
	 * shortly after.
	 */
	UPROPERTY(EditAnywhere, Category = "Hay|Debug")
	bool bLogFrameStats = true;

	/**
	 * Run profilegpu and memreport once the initial shells are complete.
	 */
	UPROPERTY(EditAnywhere, Category = "Hay|Debug")
	bool bDumpDiagnosticsAfterSpawn = false;

	/**
	 * Seconds between spawn completion and the diagnostics dump.
	 */
	UPROPERTY(EditAnywhere, Category = "Hay|Debug")
	float DiagnosticsDelay = 5.f;
#endif

	FOnHayCellSpawned OnCellSpawned;

	/**
	 * Reads the mesh, queues the outer shells. Needs a built layout on the owner.
	 */
	void Initialize();

	/**
	 * Spawns the cells directly beneath a piece so removing it never shows a
	 * hole into nothing.
	 */
	void RevealBelow(const int32 PieceIndex);

	/**
	 * Writes one instance transform into the chunk that owns the piece.
	 */
	void SetInstanceTransform(const int32 PieceIndex, const FTransform& LocalTransform);

	/**
	 * Shrinks an instance to nothing while keeping its index and location.
	 */
	void HideInstance(const int32 PieceIndex, const FTransform& RestLocalTransform);

	void ShowNeedle(const FTransform& LocalTransform);

	void HideNeedle();

	/**
	 * Half extents of one piece along its own axes, from the mesh bounds.
	 */
	FVector3f GetPieceHalfExtents() const { return PieceHalfExtents; }

	virtual void TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void QueueCell(const int32 CellIndex);

	void SpawnCell(const int32 CellIndex);

	void LogInitialSpawnDone();

#if WITH_EDITORONLY_DATA
	void LogFrameStats(const float DeltaTime);

	void DumpDiagnostics() const;

	FTimerHandle DiagnosticsTimer = {};

	int32 StatReportsAfterSpawn = 6;

	float StatAccumulationSeconds = 0.f;

	int32 StatAccumulationFrames = 0;
#endif

	/**
	 * Scale of a hidden instance. Not zero, so the instance matrix stays
	 * invertible for the renderer, but far below one pixel at any distance.
	 */
	static constexpr float HiddenInstanceScale = 0.0001f;

	UPROPERTY(Transient)
	TObjectPtr<UHayLayoutComponent> Layout = nullptr;

	/**
	 * One entry per cell, null until spawned.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> CellChunks = {};

	/**
	 * The one piece that is not hay.
	 * Hidden until the piece state places it.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Needle = nullptr;

	/**
	 * Cells waiting to spawn, processed under SpawnBudgetMs per frame.
	 */
	TArray<int32> SpawnQueue = {};

	int32 SpawnQueueHead = 0;

	TBitArray<> CellQueuedOrSpawned = {};

	/**
	 * Reused per cell while spawning.
	 */
	TArray<FTransform> ScratchTransforms = {};

	FVector3f PieceHalfExtents = FVector3f::ZeroVector;

	int32 PiecesSpawned = 0;

	int32 InitialPieces = 0;

	bool bInitialSpawnLogged = false;

	double SpawnStartSeconds = 0.0;

	uint64 SpawnStartUsedPhysical = 0;
};