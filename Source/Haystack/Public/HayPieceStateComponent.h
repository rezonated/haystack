// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HayPieceStateComponent.generated.h"

class UHayLayoutComponent;
class UHayRenderComponent;

enum class EHayPieceState : uint8
{
	Held,
	Loose,
};

/**
 * A piece that left its generated spot.
 * Pieces still at their spot have no entry.
 */
struct FHayMovedPiece
{
	int32 PieceIndex = INDEX_NONE;

	EHayPieceState State = EHayPieceState::Held;

	/**
	 * Rest transform relative to the pile actor. Valid when Loose.
	 */
	FTransform LooseTransform = FTransform::Identity;
};

/**
 * Which pieces have been taken out of the pile and where they are now.
 * This is the only per-piece state that changes during play, and later the only thing the server has to replicate.
 */
UCLASS(ClassGroup = Hay, meta = (BlueprintSpawnableComponent))
class UHayPieceStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * Needs a built layout and an initialized render component on the owner.
	 */
	void Initialize();

	/**
	 * Lifts a piece out of the pile or off the ground. Fails when the piece is already held.
	 */
	bool TakePiece(const int32 PieceIndex);

	/**
	 * Puts a held piece down at a world transform.
	 */
	bool PlacePiece(const int32 PieceIndex, const FTransform& WorldTransform);

	/**
	 * True for every piece with an entry in the moved list.
	 */
	bool IsPieceMoved(const int32 PieceIndex) const { return PieceMoved[PieceIndex]; }

	const TArray<FHayMovedPiece>& GetMovedPieces() const { return MovedPieces; }

	FTransform GetPieceWorldTransform(const int32 PieceIndex) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UHayLayoutComponent> Layout = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHayRenderComponent> Render = nullptr;

	/**
	 * One bit per piece, set when the piece has a MovedPieces entry.
	 */
	TBitArray<> PieceMoved = {};

	TArray<FHayMovedPiece> MovedPieces = {};

	TMap<int32, int32> MovedIndexByPiece = {};
};