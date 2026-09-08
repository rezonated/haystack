// Copyright (c) 2026 Vanan Andreas.

#include "HayPieceStateComponent.h"
#include "HayLayoutComponent.h"
#include "HayRenderComponent.h"
#include "Haystack.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayPieceStateComponent)

void UHayPieceStateComponent::Initialize()
{
	Layout = GetOwner()->FindComponentByClass<UHayLayoutComponent>();
	Render = GetOwner()->FindComponentByClass<UHayRenderComponent>();
	if (!Layout || !Layout->IsBuilt() || !Render)
	{
		UE_LOG(LogHay, Error, TEXT("%s: HayPieceState needs a built HayLayout and a HayRender on the same actor"), *GetOwner()->GetName());
		return;
	}

	PieceMoved.Init(false, Layout->NumPieces);
	MovedPieces.Reset();
	MovedIndexByPiece.Reset();
}

FTransform UHayPieceStateComponent::GetPieceWorldTransform(const int32 PieceIndex) const
{
	if (const int32* MovedIndex = MovedIndexByPiece.Find(PieceIndex))
	{
		return MovedPieces[*MovedIndex].LooseTransform * GetOwner()->GetActorTransform();
	}

	return Layout->GetPieceLocalTransform(PieceIndex) * GetOwner()->GetActorTransform();
}

bool UHayPieceStateComponent::TakePiece(const int32 PieceIndex)
{
	if (!Layout || PieceIndex < 0 || PieceIndex >= Layout->NumPieces)
	{
		return false;
	}

	FTransform RestTransform;
	if (const int32* MovedIndex = MovedIndexByPiece.Find(PieceIndex))
	{
		FHayMovedPiece& Moved = MovedPieces[*MovedIndex];
		if (Moved.State == EHayPieceState::Held)
		{
			return false;
		}

		Moved.State = EHayPieceState::Held;
		RestTransform = Moved.LooseTransform;
	}
	else
	{
		PieceMoved[PieceIndex] = true;
		MovedIndexByPiece.Add(PieceIndex, MovedPieces.Num());
		FHayMovedPiece& Moved = MovedPieces.AddDefaulted_GetRef();
		Moved.PieceIndex = PieceIndex;
		Moved.State = EHayPieceState::Held;
		RestTransform = Layout->GetPieceLocalTransform(PieceIndex);
		Render->RevealBelow(PieceIndex);
	}

	Render->HideInstance(PieceIndex, RestTransform);
	return true;
}

bool UHayPieceStateComponent::PlacePiece(const int32 PieceIndex, const FTransform& WorldTransform)
{
	const int32* MovedIndex = MovedIndexByPiece.Find(PieceIndex);
	if (!MovedIndex || MovedPieces[*MovedIndex].State != EHayPieceState::Held)
	{
		return false;
	}

	FHayMovedPiece& Moved = MovedPieces[*MovedIndex];
	Moved.State = EHayPieceState::Loose;
	Moved.LooseTransform = WorldTransform.GetRelativeTransform(GetOwner()->GetActorTransform());
	Moved.LooseTransform.SetScale3D(FVector::OneVector);
	Render->SetInstanceTransform(PieceIndex, Moved.LooseTransform);
	return true;
}