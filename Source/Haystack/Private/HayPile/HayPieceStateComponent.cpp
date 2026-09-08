// Copyright (c) 2026 Vanan Andreas.

#include "HayPile/HayPieceStateComponent.h"
#include "Haystack.h"
#include "HayPile/HayLayoutComponent.h"
#include "HayPile/HayRenderComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayPieceStateComponent)

void FHayMovedPieceList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, const int32 FinalSize)
{
	for (const int32 Index : AddedIndices)
	{
		Owner->ApplyMovedPiece(Index);
	}
}

void FHayMovedPieceList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, const int32 FinalSize)
{
	for (const int32 Index : ChangedIndices)
	{
		Owner->ApplyMovedPiece(Index);
	}
}

UHayPieceStateComponent::UHayPieceStateComponent()
{
	SetIsReplicatedByDefault(true);
	MovedList.Owner = this;
}

void UHayPieceStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHayPieceStateComponent, MovedList);
}

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
	MovedIndexByPiece.Reset();
	Render->OnCellSpawned.AddUObject(this, &UHayPieceStateComponent::OnCellSpawned);

	// Entries that arrived before BeginPlay on a client, or the whole list on a late join.
	for (int32 ItemIndex = 0; ItemIndex < MovedList.Items.Num(); ++ItemIndex)
	{
		ApplyMovedPiece(ItemIndex);
	}
}

FTransform UHayPieceStateComponent::GetPieceWorldTransform(const int32 PieceIndex) const
{
	if (const int32* MovedIndex = MovedIndexByPiece.Find(PieceIndex))
	{
		return MovedList.Items[*MovedIndex].RestTransform * GetOwner()->GetActorTransform();
	}

	return Layout->GetPieceLocalTransform(PieceIndex) * GetOwner()->GetActorTransform();
}

void UHayPieceStateComponent::ApplyMovedPiece(const int32 ItemIndex)
{
	if (PieceMoved.IsEmpty())
	{
		// Not initialized yet.
		// Initialize replays the whole list.
		return;
	}

	const FHayMovedPiece& Moved = MovedList.Items[ItemIndex];
	if (!PieceMoved[Moved.PieceIndex])
	{
		PieceMoved[Moved.PieceIndex] = true;
		MovedIndexByPiece.Add(Moved.PieceIndex, ItemIndex);
		Render->RevealBelow(Moved.PieceIndex);
	}

	if (Moved.State == EHayPieceState::Held)
	{
		Render->HideInstance(Moved.PieceIndex, Moved.RestTransform);
	}
	else
	{
		Render->SetInstanceTransform(Moved.PieceIndex, Moved.RestTransform);
	}
}

void UHayPieceStateComponent::OnCellSpawned(const int32 CellIndex, TArrayView<const FTransform> LocalTransforms)
{
	const FHayCell& Cell = Layout->GetCells()[CellIndex];
	for (int32 ItemIndex = 0; ItemIndex < MovedList.Items.Num(); ++ItemIndex)
	{
		const int32 PieceIndex = MovedList.Items[ItemIndex].PieceIndex;
		if (PieceIndex >= Cell.FirstPiece && PieceIndex < Cell.FirstPiece + Cell.PieceCount)
		{
			ApplyMovedPiece(ItemIndex);
		}
	}
}

bool UHayPieceStateComponent::TakePiece(const int32 PieceIndex)
{
	if (!Layout || !GetOwner()->HasAuthority() || PieceIndex < 0 || PieceIndex >= Layout->NumPieces)
	{
		return false;
	}

	int32 ItemIndex;
	if (const int32* MovedIndex = MovedIndexByPiece.Find(PieceIndex))
	{
		ItemIndex = *MovedIndex;
		if (MovedList.Items[ItemIndex].State == EHayPieceState::Held)
		{
			return false;
		}
	}
	else
	{
		ItemIndex = MovedList.Items.AddDefaulted();
		MovedList.Items[ItemIndex].PieceIndex = PieceIndex;
		MovedList.Items[ItemIndex].RestTransform = Layout->GetPieceLocalTransform(PieceIndex);
	}

	FHayMovedPiece& Moved = MovedList.Items[ItemIndex];
	Moved.State = EHayPieceState::Held;
	MovedList.MarkItemDirty(Moved);
	ApplyMovedPiece(ItemIndex);

	return true;
}

bool UHayPieceStateComponent::PlacePiece(const int32 PieceIndex, const FTransform& WorldTransform)
{
	const int32* MovedIndex = MovedIndexByPiece.Find(PieceIndex);
	if (!GetOwner()->HasAuthority() || !MovedIndex || MovedList.Items[*MovedIndex].State != EHayPieceState::Held)
	{
		return false;
	}

	FHayMovedPiece& Moved = MovedList.Items[*MovedIndex];
	Moved.State = EHayPieceState::Loose;
	Moved.RestTransform = WorldTransform.GetRelativeTransform(GetOwner()->GetActorTransform());
	Moved.RestTransform.SetScale3D(FVector::OneVector);
	MovedList.MarkItemDirty(Moved);
	ApplyMovedPiece(*MovedIndex);
	return true;
}