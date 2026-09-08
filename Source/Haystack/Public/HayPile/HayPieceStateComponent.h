// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "HayPieceStateComponent.generated.h"

class UHayLayoutComponent;
class UHayPieceStateComponent;
class UHayRenderComponent;

UENUM()
enum class EHayPieceState : uint8
{
	Held,
	Loose,
};

/**
 * A piece that left its generated spot.
 * Pieces still at their spot have no entry.
 */
USTRUCT()
struct FHayMovedPiece : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PieceIndex = INDEX_NONE;

	UPROPERTY()
	EHayPieceState State = EHayPieceState::Held;

	/**
	 * Where the instance sits when not held, relative to the pile actor.
	 * The generated spot until the piece is first put down, then wherever it was last placed.
	 */
	UPROPERTY()
	FTransform RestTransform = FTransform::Identity;
};

/**
 * Replicated list of moved pieces. Only ever grows.
 * The server writes it, clients apply each add or change to their own render component.
 */
USTRUCT()
struct FHayMovedPieceList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FHayMovedPiece> Items;

	UPROPERTY(NotReplicated)
	TObjectPtr<UHayPieceStateComponent> Owner = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FHayMovedPiece, FHayMovedPieceList>(Items, DeltaParms, *this);
	}

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, const int32 FinalSize);

	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, const int32 FinalSize);
};

template <>
struct TStructOpsTypeTraits<FHayMovedPieceList> : public TStructOpsTypeTraitsBase2<FHayMovedPieceList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/**
 * Which pieces have been taken out of the pile and where they are now.
 * This is the only per-piece state that changes during play and the only thing the server replicates about the pile.
 * TakePiece and PlacePiece run on the server. Clients receive the list and mirror each entry into their render component.
 */
UCLASS(ClassGroup = Hay, meta = (BlueprintSpawnableComponent))
class UHayPieceStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHayPieceStateComponent();

	/**
	 * Needs a built layout and an initialized render component on the owner.
	 * Applies any entries that replicated in before this ran.
	 */
	void Initialize();

	/**
	 * Lifts a piece out of the pile or off the ground. Fails when the piece is already held.
	 * Server only.
	 */
	bool TakePiece(const int32 PieceIndex);

	/**
	 * Puts a held piece down at a world transform.
	 * Server only.
	 */
	bool PlacePiece(const int32 PieceIndex, const FTransform& WorldTransform);

	/**
	 * True for every piece with an entry in the moved list.
	 */
	bool IsPieceMoved(const int32 PieceIndex) const { return PieceMoved[PieceIndex]; }

	const TArray<FHayMovedPiece>& GetMovedPieces() const { return MovedList.Items; }

	FTransform GetPieceWorldTransform(const int32 PieceIndex) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Mirrors one list entry into the render component and the lookup tables.
	 * Called on the server right after a change and on clients from the fast array callbacks.
	 */
	void ApplyMovedPiece(const int32 ItemIndex);

private:
	/**
	 * A cell that spawns after some of its pieces moved needs those instances hidden or relocated.
	 * Happens on clients that join late and on any machine that takes a piece before the cells below it spawn.
	 */
	void OnCellSpawned(const int32 CellIndex, TArrayView<const FTransform> LocalTransforms);

	UPROPERTY(Transient)
	TObjectPtr<UHayLayoutComponent> Layout = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHayRenderComponent> Render = nullptr;

	UPROPERTY(Replicated)
	FHayMovedPieceList MovedList;

	/**
	 * One bit per piece, set when the piece has a MovedList entry.
	 * Empty until Initialize.
	 */
	TBitArray<> PieceMoved = {};

	TMap<int32, int32> MovedIndexByPiece = {};
};