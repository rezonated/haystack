// Copyright (c) 2026 Vanan Andreas.

#include "HayPickComponent.h"
#include "HayLayoutComponent.h"
#include "HayPieceStateComponent.h"
#include "HayRenderComponent.h"
#include "Haystack.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayPickComponent)

void UHayPickComponent::Initialize()
{
	Layout = GetOwner()->FindComponentByClass<UHayLayoutComponent>();
	Render = GetOwner()->FindComponentByClass<UHayRenderComponent>();
	State = GetOwner()->FindComponentByClass<UHayPieceStateComponent>();

	if (!Layout || !Layout->IsBuilt() || !Render || !State)
	{
		UE_LOG(LogHay, Error, TEXT("%s: HayPick needs a built HayLayout, a HayRender and a HayPieceState on the same actor"), *GetOwner()->GetName());
		return;
	}

	CellPick.SetNum(Layout->GetCells().Num());
	PieceBoundRadius = Render->GetPieceHalfExtents().Size();
	Render->OnCellSpawned.AddUObject(this, &UHayPickComponent::OnCellSpawned);
}

void UHayPickComponent::OnCellSpawned(const int32 CellIndex, TArrayView<const FTransform> LocalTransforms)
{
	FHayCellPick& Pick = CellPick[CellIndex];
	Pick.Locations.SetNumUninitialized(LocalTransforms.Num());
	Pick.Rotations.SetNumUninitialized(LocalTransforms.Num());
	Pick.Bounds = FBox3f(ForceInit);
	for (int32 PieceOffset = 0; PieceOffset < LocalTransforms.Num(); ++PieceOffset)
	{
		Pick.Locations[PieceOffset] = FVector3f(LocalTransforms[PieceOffset].GetLocation());
		Pick.Rotations[PieceOffset] = FQuat4f(LocalTransforms[PieceOffset].GetRotation());
		Pick.Bounds += Pick.Locations[PieceOffset];
	}

	Pick.Bounds = Pick.Bounds.ExpandBy(PieceBoundRadius);
}

bool UHayPickComponent::RayHitsPiece(const FVector3f& Origin, const FVector3f& Direction, const FVector3f& PieceLocation, const FQuat4f& PieceRotation, const FVector3f& HalfExtents, const float BoundRadius, float& InOutBestDistance)
{
	const FVector3f ToPiece = PieceLocation - Origin;
	const float		AlongRay = ToPiece | Direction;
	if (AlongRay - BoundRadius > InOutBestDistance || AlongRay + BoundRadius < 0.f)
	{
		return false;
	}

	if (ToPiece.SizeSquared() - AlongRay * AlongRay > BoundRadius * BoundRadius)
	{
		return false;
	}

	// In piece space the piece is an axis-aligned box centered at the origin.
	const FVector3f LocalOrigin = PieceRotation.UnrotateVector(-ToPiece);
	const FVector3f LocalDirection = PieceRotation.UnrotateVector(Direction);

	float Entry = 0.f;
	float Exit = InOutBestDistance;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float InverseDirection = 1.f / LocalDirection[Axis];
		float		Near = (-HalfExtents[Axis] - LocalOrigin[Axis]) * InverseDirection;
		float		Far = (HalfExtents[Axis] - LocalOrigin[Axis]) * InverseDirection;
		if (Near > Far)
		{
			Swap(Near, Far);
		}

		Entry = FMath::Max(Entry, Near);
		Exit = FMath::Min(Exit, Far);
		if (Entry > Exit)
		{
			return false;
		}
	}

	InOutBestDistance = Entry;
	return true;
}

bool UHayPickComponent::RayPick(const FVector& WorldOrigin, const FVector& WorldDirection, const float MaxDistance, FHayPickResult& OutResult)
{
	if (!State)
	{
		return false;
	}

	const FTransform& PileToWorld = GetOwner()->GetActorTransform();
	const FVector3f	  Origin(PileToWorld.InverseTransformPosition(WorldOrigin));
	const FVector3f	  Direction(PileToWorld.InverseTransformVectorNoScale(WorldDirection).GetSafeNormal());
	const FVector3f	  HalfExtents = Render->GetPieceHalfExtents();

	float BestDistance = MaxDistance;
	int32 BestPiece = INDEX_NONE;

	// Loose pieces on the ground are few, test them all.
	for (const FHayMovedPiece& Moved : State->GetMovedPieces())
	{
		if (Moved.State == EHayPieceState::Loose && RayHitsPiece(Origin, Direction, FVector3f(Moved.LooseTransform.GetLocation()), FQuat4f(Moved.LooseTransform.GetRotation()), HalfExtents, PieceBoundRadius, BestDistance))
		{
			BestPiece = Moved.PieceIndex;
		}
	}

	// Cells the ray enters, nearest first, so the search can stop once a hit is closer than the next cell.
	PickCandidates.Reset();
	for (int32 CellIndex = 0; CellIndex < CellPick.Num(); ++CellIndex)
	{
		const FHayCellPick& Pick = CellPick[CellIndex];
		if (Pick.Locations.IsEmpty())
		{
			continue;
		}

		const FVector3f Extent = Direction * BestDistance;
		if (FMath::LineBoxIntersection(Pick.Bounds, Origin, Origin + Extent, Extent))
		{
			const float EntryDistance = Pick.Bounds.IsInsideOrOn(Origin) ? 0.f : FVector3f::Dist(Origin, Pick.Bounds.GetClosestPointTo(Origin));
			PickCandidates.Emplace(EntryDistance, CellIndex);
		}
	}
	PickCandidates.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });

	const TArray<FHayCell>& Cells = Layout->GetCells();
	for (const TPair<float, int32>& Candidate : PickCandidates)
	{
		if (Candidate.Key >= BestDistance)
		{
			break;
		}

		const FHayCell&		Cell = Cells[Candidate.Value];
		const FHayCellPick& Pick = CellPick[Candidate.Value];
		for (int32 PieceOffset = 0; PieceOffset < Cell.PieceCount; ++PieceOffset)
		{
			const int32 PieceIndex = Cell.FirstPiece + PieceOffset;
			if (State->IsPieceMoved(PieceIndex))
			{
				continue;
			}

			if (RayHitsPiece(Origin, Direction, Pick.Locations[PieceOffset], Pick.Rotations[PieceOffset], HalfExtents, PieceBoundRadius, BestDistance))
			{
				BestPiece = PieceIndex;
			}
		}
	}

	if (BestPiece == INDEX_NONE)
	{
		return false;
	}

	OutResult.PieceIndex = BestPiece;
	OutResult.Distance = BestDistance;
	OutResult.WorldTransform = State->GetPieceWorldTransform(BestPiece);
	return true;
}