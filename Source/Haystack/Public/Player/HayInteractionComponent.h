// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HayInteractionComponent.generated.h"

class AHayFallingPiece;
class AHayPile;
class UCameraComponent;
class UInputAction;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Hover, grab and drop of hay pieces for the pawn this sits on.
 * The owning client picks and asks, the server takes and drops. A dropped piece falls as a physics actor until it rests,
 * then the pile's moved list carries its spot to everyone.
 * The hover outline exists only on the locally controlled pawn. The held piece shows in front of the local camera and in the
 * hand of every remote pawn.
 */
UCLASS(ClassGroup = Hay, meta = (BlueprintSpawnableComponent))
class UHayInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHayInteractionComponent();

	/**
	 * Press to grab the hovered piece, press again to drop it.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	TObjectPtr<UInputAction> InteractAction = nullptr;

	/**
	 * Post process material that draws the hover outline from custom depth.
	 * Added to the local camera only.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	TObjectPtr<UMaterialInterface> OutlineMaterial = nullptr;

	/**
	 * How far from the camera a piece can be hovered or grabbed, cm.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 10))
	float Reach = 300.f;

	/**
	 * The server accepts a grab up to Reach plus this many cm from the pawn's eyes.
	 * Covers the camera sitting off the eye point and the pawn moving during the round trip.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0))
	float ServerReachSlack = 150.f;

	/**
	 * Where the held piece floats, relative to the local camera. Also where the server releases it on drop.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	FVector HeldOffset = FVector(60.f, 0.f, -8.f);

	UPROPERTY(EditAnywhere, Category = Hay)
	FRotator HeldRotation = FRotator(0.f, 90.f, 0.f);

	/**
	 * Socket on the pawn's skeletal mesh that carries the held piece on remote pawns.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	FName HeldSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, Category = Hay)
	FTransform HeldSocketOffset = FTransform::Identity;

	/**
	 * Speed given to a dropped piece along the view direction, cm/s, on top of the pawn's own velocity. Zero lets it fall.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0))
	float ThrowSpeed = 600.f;

	/**
	 * Spawned by the server where the held piece was when dropped.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	TSubclassOf<AHayFallingPiece> FallingPieceClass = nullptr;

	virtual void TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Piece under the crosshair on the local pawn, INDEX_NONE otherwise.
	 */
	int32 GetHoveredPiece() const { return HoveredPiece; }

	int32 GetHeldPiece() const { return HeldPiece; }

	/**
	 * Asks the server to lift a piece. The interact key calls this with the hovered piece, bots call it directly.
	 */
	void RequestTake(const int32 PieceIndex);

	/**
	 * Asks the server to throw the held piece.
	 */
	void RequestDrop();

private:
	UFUNCTION()
	void OnPawnRestarted(APawn* Pawn);

	UFUNCTION()
	void OnRep_HeldPiece();

	UFUNCTION(Server, Reliable)
	void Server_Take(const int32 PieceIndex);

	/**
	 * Releases the held piece from the hand as a falling piece.
	 */
	UFUNCTION(Server, Reliable)
	void Server_Drop(const int32 PieceIndex);

	void BindInput();

	void Interact();

	void UpdateHover();

	bool GetViewRay(FVector& OutOrigin, FVector& OutDirection) const;

	/**
	 * Server side range check for a grab at a world location.
	 */
	bool IsWithinServerReach(const FVector& WorldLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<AHayPile> Pile = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	/**
	 * Copy of the hovered piece rendered into custom depth only, for the outline.
	 * Local pawn only.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HoverProxy = nullptr;

	/**
	 * The piece in hand.
	 * On the hand socket for remote pawns, in front of the camera for the local one.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeldMesh = nullptr;

	/**
	 * Piece this pawn holds, written by the server.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_HeldPiece)
	int32 HeldPiece = INDEX_NONE;

	int32 HoveredPiece = INDEX_NONE;

	bool bInputBound = false;
};