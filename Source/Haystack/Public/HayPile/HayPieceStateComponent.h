// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "HayPieceStateComponent.generated.h"

class UHayLayoutComponent;
class UHayPieceStateComponent;
class UHayRenderComponent;

/**
 * Fired on every machine once the needle has a finder.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHayNeedleFound, const FUniqueNetIdRepl& /*Player*/);

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
UCLASS(ClassGroup = Hay, MinimalAPI, meta = (BlueprintSpawnableComponent))
class UHayPieceStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHayPieceStateComponent();

	/**
	 * Depth below the dome surface, cm, where the server may put the needle.
	 * An upper bound past the dome radius allows anywhere down to the center.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	FFloatInterval NeedleDepth = FFloatInterval(40.f, 100000.f);

	/**
	 * Needs a built layout and an initialized render component on the owner.
	 * Picks the needle on the server. Applies any entries that replicated in before this ran.
	 */
	void Initialize();

	int32 GetNeedlePiece() const { return NeedlePiece; }

	/**
	 * Net id of the player who lifted the needle, invalid until then.
	 */
	const FUniqueNetIdRepl& GetNeedleFoundBy() const { return NeedleFoundBy; }

	bool IsNeedleFound() const { return NeedleFoundBy.IsValid(); }

	FOnHayNeedleFound OnNeedleFound;

	/**
	 * Records the first player to lift the needle.
	 * Server only, ignored for any other piece.
	 */
	HAYSTACK_API void NotifyPieceTaken(const int32 PieceIndex, const FUniqueNetIdRepl& Player);

	/**
	 * Lifts a piece out of the pile or off the ground. Fails when the piece is already held.
	 * Server only.
	 */
	HAYSTACK_API bool TakePiece(const int32 PieceIndex);

	/**
	 * Puts a held piece down at a world transform.
	 * Server only.
	 */
	HAYSTACK_API bool PlacePiece(const int32 PieceIndex, const FTransform& WorldTransform);

	/**
	 * True for every piece with an entry in the moved list.
	 */
	bool IsPieceMoved(const int32 PieceIndex) const { return PieceMoved[PieceIndex]; }

	const TArray<FHayMovedPiece>& GetMovedPieces() const { return MovedList.Items; }

	HAYSTACK_API FTransform GetPieceWorldTransform(const int32 PieceIndex) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Mirrors one list entry into the render component and the lookup tables.
	 * Called on the server right after a change and on clients from the fast array callbacks.
	 */
	void ApplyMovedPiece(const int32 ItemIndex);

private:
	UFUNCTION()
	void OnRep_NeedlePiece();

	UFUNCTION()
	void OnRep_NeedleFoundBy();

	/**
	 * Random piece inside NeedleDepth.
	 * Falls back to any piece when the band holds none.
	 */
	int32 PickNeedlePiece() const;

	/**
	 * Hides the needle piece's hay slab and shows the needle mesh where the piece rests.
	 */
	void ApplyNeedle();

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

	UPROPERTY(ReplicatedUsing = OnRep_NeedlePiece)
	int32 NeedlePiece = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_NeedleFoundBy)
	FUniqueNetIdRepl NeedleFoundBy;

	/**
	 * One bit per piece, set when the piece has a MovedList entry.
	 * Empty until Initialize.
	 */
	TBitArray<> PieceMoved = {};

	TMap<int32, int32> MovedIndexByPiece = {};
};