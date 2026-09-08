// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HayInteractionComponent.generated.h"

class AHayPile;
class UCameraComponent;
class UInputAction;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Hover, grab and drop of hay pieces for the pawn this sits on.
 * Hover outline and the held piece view exist only on the locally controlled pawn, so other players never see them.
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
	 * Where the held piece floats, relative to the camera.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	FVector HeldOffset = FVector(60.f, 0.f, -8.f);

	UPROPERTY(EditAnywhere, Category = Hay)
	FRotator HeldRotation = FRotator(0.f, 90.f, 0.f);

	virtual void TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnPawnRestarted(APawn* Pawn);

	void BindInput();

	void Interact();

	void UpdateHover();

	bool FindDropTransform(FTransform& OutWorldTransform) const;

	bool GetViewRay(FVector& OutOrigin, FVector& OutDirection) const;

	UPROPERTY(Transient)
	TObjectPtr<AHayPile> Pile = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	/**
	 * Copy of the hovered piece rendered into custom depth only, for the outline.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HoverProxy = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeldMesh = nullptr;

	int32 HoveredPiece = INDEX_NONE;

	int32 HeldPiece = INDEX_NONE;

	bool bInputBound = false;
};