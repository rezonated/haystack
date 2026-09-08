// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HayPile.generated.h"

class UHayLayoutComponent;
class UHayPickComponent;
class UHayPieceStateComponent;
class UHayRenderComponent;
class USphereComponent;

/**
 * The haystack.
 * A blocking sphere for the pawn plus four components: layout (where pieces are), render (chunks on screen), piece state (what moved) and
 * pick (what the player is looking at).
 */
UCLASS(Blueprintable, HideCategories = (Input, Replication, Collision, Rendering, HLOD, Physics, Networking, LevelInstance, Cooking, DataLayers, WorldPartition))
class AHayPile : public AActor
{
	GENERATED_BODY()

public:
	AHayPile();

	UHayLayoutComponent* GetLayout() const { return Layout; }

	UHayRenderComponent* GetRender() const { return Render; }

	UHayPieceStateComponent* GetPieceState() const { return PieceState; }

	UHayPickComponent* GetPick() const { return Pick; }

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	/**
	 * Root.
	 * Blocks pawns, ignores visibility traces, draws the dome radius in the editor.
	 * Radius follows the layout's DomeRadius.
	 */
	UPROPERTY(VisibleAnywhere, Category = Hay)
	TObjectPtr<USphereComponent> Dome = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Hay)
	TObjectPtr<UHayLayoutComponent> Layout = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Hay)
	TObjectPtr<UHayRenderComponent> Render = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Hay)
	TObjectPtr<UHayPieceStateComponent> PieceState = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Hay)
	TObjectPtr<UHayPickComponent> Pick = nullptr;
};