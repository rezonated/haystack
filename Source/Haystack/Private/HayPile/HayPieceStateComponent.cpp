// Copyright (c) 2026 Vanan Andreas.

#include "HayPile/HayPieceStateComponent.h"
#include "Haystack.h"
#include "HayPile/HayLayoutComponent.h"
#include "HayPile/HayRenderComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

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

	// Push based: the server marks these dirty where it writes them, so replication never compares them per frame.
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHayPieceStateComponent, MovedList, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UHayPieceStateComponent, NeedlePiece, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UHayPieceStateComponent, NeedleFoundBy, Params);
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

	if (GetOwner()->HasAuthority())
	{
		NeedlePiece = PickNeedlePiece();
		MARK_PROPERTY_DIRTY_FROM_NAME(UHayPieceStateComponent, NeedlePiece, this);
		const FVector NeedleLocation = GetPieceWorldTransform(NeedlePiece).GetLocation();
		UE_LOG(LogHay, Log, TEXT("Needle is piece %d in shell %d at %s"), NeedlePiece, Layout->GetCells()[Layout->GetCellOfPiece(NeedlePiece)].Shell, *NeedleLocation.ToString());
	}

	// Entries that arrived before BeginPlay on a client, or the whole list on a late join.
	for (int32 ItemIndex = 0; ItemIndex < MovedList.Items.Num(); ++ItemIndex)
	{
		ApplyMovedPiece(ItemIndex);
	}
	ApplyNeedle();
}

int32 UHayPieceStateComponent::PickNeedlePiece() const
{
	FFloatInterval Band = NeedleDepth;
	if (Band.Min > Band.Max || Band.Min >= Layout->DomeRadius || Band.Max <= 0.f)
	{
		UE_LOG(LogHay, Warning, TEXT("%s: NeedleDepth %.0f to %.0f cm holds no pieces, needle placed anywhere"), *GetOwner()->GetName(), Band.Min, Band.Max);
		Band = FFloatInterval(0.f, Layout->DomeRadius);
	}

	// Uniform over pieces, so deeper bands with more pieces are likelier.
	// Redraw until one lands inside the depth band.
	int32 PieceIndex;
	do
	{
		PieceIndex = FMath::RandRange(0, Layout->NumPieces - 1);
	}
	while (!Band.Contains(Layout->DomeRadius - Layout->GetPieceLocalTransform(PieceIndex).GetLocation().Length()));

	return PieceIndex;
}

void UHayPieceStateComponent::OnRep_NeedlePiece()
{
	ApplyNeedle();
}

void UHayPieceStateComponent::ApplyNeedle()
{
	if (PieceMoved.IsEmpty() || NeedlePiece == INDEX_NONE)
	{
		return;
	}

	if (const int32* MovedIndex = MovedIndexByPiece.Find(NeedlePiece))
	{
		// Moved entries already know how to draw the needle.
		ApplyMovedPiece(*MovedIndex);
		return;
	}

	const FTransform Rest = Layout->GetPieceLocalTransform(NeedlePiece);
	Render->HideInstance(NeedlePiece, Rest);
	Render->ShowNeedle(Rest);
}

void UHayPieceStateComponent::NotifyPieceTaken(const int32 PieceIndex, const FUniqueNetIdRepl& Player)
{
	if (GetOwner()->HasAuthority() && PieceIndex == NeedlePiece && !NeedleFoundBy.IsValid())
	{
		NeedleFoundBy = Player;
		MARK_PROPERTY_DIRTY_FROM_NAME(UHayPieceStateComponent, NeedleFoundBy, this);
		OnRep_NeedleFoundBy();
	}
}

void UHayPieceStateComponent::OnRep_NeedleFoundBy()
{
	if (NeedleFoundBy.IsValid())
	{
		UE_LOG(LogHay, Log, TEXT("Needle found by %s"), *NeedleFoundBy.ToString());
		OnNeedleFound.Broadcast(NeedleFoundBy);
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

	const bool bNeedle = Moved.PieceIndex == NeedlePiece;
	if (Moved.State == EHayPieceState::Held)
	{
		Render->HideInstance(Moved.PieceIndex, Moved.RestTransform);
		if (bNeedle)
		{
			Render->HideNeedle();
		}
	}
	else if (bNeedle)
	{
		// The needle's hay slab never shows. The needle mesh takes its place.
		Render->HideInstance(Moved.PieceIndex, Moved.RestTransform);
		Render->ShowNeedle(Moved.RestTransform);
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

	if (NeedlePiece >= Cell.FirstPiece && NeedlePiece < Cell.FirstPiece + Cell.PieceCount)
	{
		ApplyNeedle();
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