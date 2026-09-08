// Copyright (c) 2026 Vanan Andreas.

#include "HayPile/HayRenderComponent.h"
#include "Haystack.h"
#include "HayPile/HayLayoutComponent.h"

#include "PrimitiveSceneProxy.h"
#include "Async/ParallelFor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"

#if WITH_EDITORONLY_DATA
	#include "RenderTimer.h"
	#include "Scalability.h"
	#include "TimerManager.h"
	#include "Engine/Engine.h"
	#include "Engine/World.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayRenderComponent)

UHayRenderComponent::UHayRenderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHayRenderComponent::Initialize()
{
	Layout = GetOwner()->FindComponentByClass<UHayLayoutComponent>();
	if (!Layout || !Layout->IsBuilt())
	{
		UE_LOG(LogHay, Error, TEXT("%s: HayRender needs a built HayLayout on the same actor"), *GetOwner()->GetName());
		return;
	}
	if (!HayMesh)
	{
		UE_LOG(LogHay, Error, TEXT("%s: HayRender has no HayMesh set"), *GetOwner()->GetName());
		return;
	}

	PieceHalfExtents = FVector3f(HayMesh->GetBounds().BoxExtent);

	const TArray<FHayCell>& Cells = Layout->GetCells();
	CellChunks.SetNum(Cells.Num());
	CellQueuedOrSpawned.Init(false, Cells.Num());

	InitialPieces = 0;
	const int32 LastInitialShell = FMath::Min(VisibleShells, Layout->GetNumShells());
	for (int32 CellIndex = 0; CellIndex < Layout->GetShellStart(LastInitialShell); ++CellIndex)
	{
		QueueCell(CellIndex);
		InitialPieces += Cells[CellIndex].PieceCount;
	}

	SpawnStartSeconds = FPlatformTime::Seconds();
	SpawnStartUsedPhysical = FPlatformMemory::GetStats().UsedPhysical;
	SetComponentTickEnabled(true);

	UE_LOG(LogHay, Log, TEXT("Pile %d pieces, radius %.0f, %d shells of %.0f cm in %d cells, mesh %s nanite %d. Spawning outer %d shells: %d cells, %d pieces. Shadow %d, indirect %d"), Layout->NumPieces, Layout->DomeRadius, Layout->GetNumShells(), Layout->ShellThickness, Cells.Num(), *HayMesh->GetName(), HayMesh->HasValidNaniteData() ? 1 : 0, LastInitialShell, SpawnQueue.Num(), InitialPieces, bCastShadow ? 1 : 0, bAffectIndirectLighting ? 1 : 0);
}

void UHayRenderComponent::QueueCell(const int32 CellIndex)
{
	if (CellQueuedOrSpawned[CellIndex])
	{
		return;
	}

	CellQueuedOrSpawned[CellIndex] = true;
	SpawnQueue.Add(CellIndex);
	SetComponentTickEnabled(true);
}

void UHayRenderComponent::RevealBelow(const int32 PieceIndex)
{
	const int32		 Shell = Layout->GetCells()[Layout->GetCellOfPiece(PieceIndex)].Shell;
	const FTransform Piece = Layout->GetPieceLocalTransform(PieceIndex);
	const FVector	 Center = Piece.GetLocation();
	const FVector	 HalfAlong = Piece.GetUnitAxis(EAxis::X) * PieceHalfExtents.X;
	const int32		 LastShell = FMath::Min(Shell + VisibleShells, Layout->GetNumShells() - 1);

	for (int32 Below = Shell + 1; Below <= LastShell; ++Below)
	{
		QueueCell(Layout->FindCell(Below, Center));
		QueueCell(Layout->FindCell(Below, Center + HalfAlong));
		QueueCell(Layout->FindCell(Below, Center - HalfAlong));
	}
}

void UHayRenderComponent::SetInstanceTransform(const int32 PieceIndex, const FTransform& LocalTransform)
{
	const int32		CellIndex = Layout->GetCellOfPiece(PieceIndex);
	const FHayCell& Cell = Layout->GetCells()[CellIndex];
	if (UInstancedStaticMeshComponent* Chunk = CellChunks[CellIndex])
	{
		Chunk->UpdateInstanceTransform(PieceIndex - Cell.FirstPiece, LocalTransform, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
	}
}

void UHayRenderComponent::HideInstance(const int32 PieceIndex, const FTransform& RestLocalTransform)
{
	SetInstanceTransform(PieceIndex, FTransform(RestLocalTransform.GetRotation(), RestLocalTransform.GetLocation(), FVector(HiddenInstanceScale)));
}

void UHayRenderComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (SpawnQueueHead < SpawnQueue.Num())
	{
		const double Deadline = FPlatformTime::Seconds() + SpawnBudgetMs * 0.001;
		do
		{
			SpawnCell(SpawnQueue[SpawnQueueHead++]);
		}
		while (SpawnQueueHead < SpawnQueue.Num() && FPlatformTime::Seconds() < Deadline);

		if (SpawnQueueHead == SpawnQueue.Num())
		{
			SpawnQueue.Reset();
			SpawnQueueHead = 0;
		}

		if (!bInitialSpawnLogged && PiecesSpawned >= InitialPieces)
		{
			LogInitialSpawnDone();
		}
	}

#if WITH_EDITORONLY_DATA
	if (bLogFrameStats && (!bInitialSpawnLogged || StatReportsAfterSpawn > 0))
	{
		LogFrameStats(DeltaTime);
		return;
	}
#endif

	if (SpawnQueue.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

void UHayRenderComponent::SpawnCell(const int32 CellIndex)
{
	const FHayCell& Cell = Layout->GetCells()[CellIndex];

	ScratchTransforms.SetNum(Cell.PieceCount, EAllowShrinking::No);
	ParallelFor(Cell.PieceCount, [this, &Cell](const int32 PieceOffset)
	{
		ScratchTransforms[PieceOffset] = Layout->GetPieceLocalTransform(Cell, Cell.FirstPiece + PieceOffset);
	});

	UInstancedStaticMeshComponent* Chunk = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Chunk->SetStaticMesh(HayMesh);
	Chunk->SetMobility(EComponentMobility::Static);
	Chunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Chunk->SetCanEverAffectNavigation(false);
	Chunk->SetGenerateOverlapEvents(false);
	Chunk->bDisableCollision = true;
	Chunk->SetCastShadow(bCastShadow);
	Chunk->bAffectDynamicIndirectLighting = bAffectIndirectLighting;
	Chunk->bAffectDistanceFieldLighting = bAffectIndirectLighting;
	Chunk->SetupAttachment(GetOwner()->GetRootComponent());
	Chunk->RegisterComponent();
	Chunk->PreAllocateInstancesMemory(Cell.PieceCount);
	Chunk->AddInstances(ScratchTransforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);

	CellChunks[CellIndex] = Chunk;
	PiecesSpawned += Cell.PieceCount;

	OnCellSpawned.Broadcast(CellIndex, ScratchTransforms);
}

void UHayRenderComponent::LogInitialSpawnDone()
{
	bInitialSpawnLogged = true;

	const uint64 UsedPhysicalNow = FPlatformMemory::GetStats().UsedPhysical;
	int32		 SpawnedChunks = 0;
	int32		 NaniteChunks = 0;
	for (const UInstancedStaticMeshComponent* Chunk : CellChunks)
	{
		SpawnedChunks += Chunk ? 1 : 0;
		NaniteChunks += (Chunk && Chunk->SceneProxy && Chunk->SceneProxy->IsNaniteMesh()) ? 1 : 0;
	}

	UE_LOG(LogHay, Log, TEXT("Spawn done: %d pieces in %.2f s, process memory +%.0f MB, nanite chunks %d / %d, pile at %s"), PiecesSpawned, FPlatformTime::Seconds() - SpawnStartSeconds, (double(UsedPhysicalNow) - double(SpawnStartUsedPhysical)) / (1024.0 * 1024.0), NaniteChunks, SpawnedChunks, *GetOwner()->GetActorLocation().ToString());

#if WITH_EDITORONLY_DATA
	if (bDumpDiagnosticsAfterSpawn)
	{
		GetWorld()->GetTimerManager().SetTimer(DiagnosticsTimer, this, &UHayRenderComponent::DumpDiagnostics, DiagnosticsDelay, false);
	}
#endif
}

#if WITH_EDITORONLY_DATA
void UHayRenderComponent::LogFrameStats(const float DeltaTime)
{
	StatAccumulationSeconds += DeltaTime;
	++StatAccumulationFrames;

	if (StatAccumulationSeconds < 5.f)
	{
		return;
	}

	if (bInitialSpawnLogged)
	{
		--StatReportsAfterSpawn;
	}

	const float AvgMs = StatAccumulationSeconds / StatAccumulationFrames * 1000.f;
	UE_LOG(LogHay, Log, TEXT("frame %.2f ms (%.0f fps) | game %.2f render %.2f rhi %.2f gpu %.2f ms | pieces %d"), AvgMs, 1000.f / AvgMs, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(GRHIThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()), PiecesSpawned);

	StatAccumulationSeconds = 0.f;
	StatAccumulationFrames = 0;
}

void UHayRenderComponent::DumpDiagnostics() const
{
	const Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	UE_LOG(LogHay, Log, TEXT("Scalability: view %d aa %d shadow %d gi %d refl %d pp %d tex %d fx %d foliage %d shading %d landscape %d res %.0f"), Levels.ViewDistanceQuality, Levels.AntiAliasingQuality, Levels.ShadowQuality, Levels.GlobalIlluminationQuality, Levels.ReflectionQuality, Levels.PostProcessQuality, Levels.TextureQuality, Levels.EffectsQuality, Levels.FoliageQuality, Levels.ShadingQuality, Levels.LandscapeQuality, Levels.ResolutionQuality);

	UWorld* World = GetWorld();
	GEngine->Exec(World, TEXT("r.ProfileGPU.ShowUI 0"));
	GEngine->Exec(World, TEXT("profilegpu"));
	GEngine->Exec(World, TEXT("memreport -full"));
}
#endif