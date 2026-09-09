// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HayHUD.generated.h"

class AHayPile;
class UHayInteractionComponent;
class UHayMessageWidget;
struct FUniqueNetIdRepl;

enum class EHayPrompt : uint8
{
	None,
	PickUp,
	Drop,
};

/**
 * Crosshair dot that turns when a piece is under it, the interact prompt, and the needle found message.
 */
UCLASS()
class AHayHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Hay)
	TSubclassOf<UHayMessageWidget> PromptWidgetClass = nullptr;

	UPROPERTY(EditAnywhere, Category = Hay)
	TSubclassOf<UHayMessageWidget> NeedleFoundWidgetClass = nullptr;

	/**
	 * {0} is the interact key as bound for keyboard and mouse.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	FText PickUpPrompt = NSLOCTEXT("Hay", "PickUpPrompt", "{0} to pick up");

	UPROPERTY(EditAnywhere, Category = Hay)
	FText DropPrompt = NSLOCTEXT("Hay", "DropPrompt", "{0} to drop");

	/**
	 * {0} is the finder's name.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	FText NeedleFoundMessage = NSLOCTEXT("Hay", "NeedleFound", "{0} found the needle");

	UPROPERTY(EditAnywhere, Category = Hay)
	FText NeedleFoundByYouMessage = NSLOCTEXT("Hay", "NeedleFoundByYou", "You found the needle");

	UPROPERTY(EditAnywhere, Category = Hay)
	FLinearColor CrosshairColor = FLinearColor::White;

	/**
	 * Crosshair color while a piece is under it.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	FLinearColor CrosshairHoverColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0))
	float CrosshairSize = 4.f;

	virtual void BeginPlay() override;

	virtual void DrawHUD() override;

private:
	void OnNeedleFound(const FUniqueNetIdRepl& Player);

	/**
	 * Shows, hides or rewrites the prompt when the hover or held state changed since last frame.
	 */
	void UpdatePrompt(const UHayInteractionComponent* Interaction);

	/**
	 * Display name of the first keyboard key mapped to the interact action, else the first key of any kind.
	 */
	FText InteractKeyName(const UHayInteractionComponent* Interaction) const;

	/**
	 * Player state name for the id, or the id text when no player state matches.
	 */
	FString FindPlayerName(const FUniqueNetIdRepl& Player) const;

	UPROPERTY(Transient)
	TObjectPtr<AHayPile> Pile = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHayMessageWidget> PromptWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHayMessageWidget> NeedleFoundWidget = nullptr;

	EHayPrompt ShownPrompt = EHayPrompt::None;
};
