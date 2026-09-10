// Copyright (c) 2026 Vanan Andreas.

#include "UI/HayHUD.h"
#include "HayPile.h"
#include "Haystack.h"
#include "HayPile/HayPieceStateComponent.h"
#include "Player/HayInteractionComponent.h"
#include "UI/HayMessageWidget.h"

#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Canvas.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayHUD)

void AHayHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PromptWidgetClass)
	{
		UE_LOG(LogHay, Warning, TEXT("%s: no PromptWidgetClass set, interact prompt not shown"), *GetName());
	}

	for (AHayPile* WorldPile : TActorRange<AHayPile>(GetWorld()))
	{
		Pile = WorldPile;
		break;
	}

	if (!Pile)
	{
		UE_LOG(LogHay, Error, TEXT("%s: no HayPile in the level"), *GetName());
		return;
	}

	UHayPieceStateComponent* PieceState = Pile->GetPieceState();
	PieceState->OnNeedleFound.AddUObject(this, &AHayHUD::OnNeedleFound);
	if (PieceState->IsNeedleFound())
	{
		OnNeedleFound(PieceState->GetNeedleFoundBy());
	}
}

void AHayHUD::DrawHUD()
{
	Super::DrawHUD();

	const APawn*					Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	const UHayInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UHayInteractionComponent>() : nullptr;
	const bool						bHovering = Interaction && Interaction->GetHoveredPiece() != INDEX_NONE;

	if (Canvas)
	{
		DrawRect(bHovering ? CrosshairHoverColor : CrosshairColor, (Canvas->SizeX - CrosshairSize) * 0.5f, (Canvas->SizeY - CrosshairSize) * 0.5f, CrosshairSize, CrosshairSize);
	}

	UpdatePrompt(Interaction);
}

void AHayHUD::UpdatePrompt(const UHayInteractionComponent* Interaction)
{
	EHayPrompt Wanted = EHayPrompt::None;
	if (NeedleFoundWidget)
	{
		// The game is over, the win message owns the screen.
	}
	else if (Interaction && Interaction->GetHeldPiece() != INDEX_NONE)
	{
		Wanted = EHayPrompt::Drop;
	}
	else if (Interaction && Interaction->GetHoveredPiece() != INDEX_NONE)
	{
		Wanted = EHayPrompt::PickUp;
	}

	if (Wanted == ShownPrompt || !PromptWidgetClass)
	{
		return;
	}
	ShownPrompt = Wanted;

	if (!PromptWidget)
	{
		PromptWidget = CreateWidget<UHayMessageWidget>(PlayerOwner, PromptWidgetClass);
		PromptWidget->AddToViewport();
	}

	if (Wanted == EHayPrompt::None)
	{
		PromptWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	PromptWidget->SetMessage(FText::Format(Wanted == EHayPrompt::Drop ? DropPrompt : PickUpPrompt, InteractKeyName(Interaction)));
	PromptWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

FText AHayHUD::InteractKeyName(const UHayInteractionComponent* Interaction) const
{
	const UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerOwner->GetLocalPlayer());
	if (!Input || !Interaction->InteractAction)
	{
		return FText::GetEmpty();
	}

	const TArray<FKey> Keys = Input->QueryKeysMappedToAction(Interaction->InteractAction);
	for (const FKey& Key : Keys)
	{
		if (!Key.IsGamepadKey() && !Key.IsMouseButton())
		{
			return Key.GetDisplayName(/*bLongDisplayName*/ false);
		}
	}

	return Keys.IsEmpty() ? FText::GetEmpty() : Keys[0].GetDisplayName(/*bLongDisplayName*/ false);
}

void AHayHUD::OnNeedleFound(const FUniqueNetIdRepl& Player)
{
	if (NeedleFoundWidget)
	{
		return;
	}

	if (!NeedleFoundWidgetClass)
	{
		UE_LOG(LogHay, Warning, TEXT("%s: no NeedleFoundWidgetClass set, needle found message not shown"), *GetName());
		return;
	}

	const bool	bYou = PlayerOwner && PlayerOwner->PlayerState && PlayerOwner->PlayerState->GetUniqueId() == Player;
	const FText Text = bYou ? NeedleFoundByYouMessage : FText::Format(NeedleFoundMessage, FText::FromString(FindPlayerName(Player)));

	NeedleFoundWidget = CreateWidget<UHayMessageWidget>(PlayerOwner, NeedleFoundWidgetClass);
	NeedleFoundWidget->SetMessage(Text);
	NeedleFoundWidget->AddToViewport();
}

FString AHayHUD::FindPlayerName(const FUniqueNetIdRepl& Player) const
{
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetUniqueId() == Player)
			{
				return PlayerState->GetPlayerName();
			}
		}
	}

	return Player.ToString();
}
