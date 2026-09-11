// Copyright (c) 2026 Vanan Andreas.

#include "HayDigTestController.h"
#include "HayPile.h"
#include "HaystackGauntlet.h"
#include "HayPile/HayPieceStateComponent.h"
#include "Testing/HayDigBotComponent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayDigTestController)

static const TCHAR* NetModeName(const ENetMode Mode)
{
	switch (Mode)
	{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		default:
			return TEXT("Client");
	}
}

void UHayDigTestController::OnInit()
{
	FParse::Value(FCommandLine::Get(), TEXT("HayDigTimeout="), TimeoutSeconds);
	bTowardNeedle = FParse::Param(FCommandLine::Get(), TEXT("HayDigNeedle"));
	StartSeconds = FPlatformTime::Seconds();
	UE_LOG(LogHayGauntlet, Log, TEXT("HayDigTest: timeout %.0f s, %s"), TimeoutSeconds, bTowardNeedle ? TEXT("toward the needle") : TEXT("random spots"));
}

void UHayDigTestController::OnTick(const float TimeDelta)
{
	const double Now = FPlatformTime::Seconds();
	UWorld*		 World = GetWorld();

	if (bFound)
	{
		// Clients leave at once. The host lingers so its last packets land.
		if (!World || World->GetNetMode() == NM_Client || Now - FoundSeconds >= HostExitDelaySeconds)
		{
			EndTest(0);
		}
		return;
	}

	if (Now - StartSeconds > TimeoutSeconds)
	{
		UE_LOG(LogHayGauntlet, Error, TEXT("HayDigTest: needle not found after %.0f s"), TimeoutSeconds);
		EndTest(1);
		return;
	}

	if (!World)
	{
		return;
	}

	// The pile changes with the map, a connecting client sees two worlds. Follow whichever pile is live.
	if (!SubscribedPile.IsValid())
	{
		for (AHayPile* Pile : TActorRange<AHayPile>(World))
		{
			SubscribedPile = Pile;
			Pile->GetPieceState()->OnNeedleFound.AddUObject(this, &UHayDigTestController::OnNeedleFound);
			if (Pile->GetPieceState()->IsNeedleFound())
			{
				OnNeedleFound(Pile->GetPieceState()->GetNeedleFoundBy());
			}
			break;
		}
	}

	APlayerController* Controller = GetFirstPlayerController();
	if (!bBotAdded && Controller && Controller->IsLocalController() && Controller->GetPawn())
	{
		// A pawn in a standalone world means the URL that should have hosted or joined never applied, so the roles
		// would each dig their own pile and the test would pass without a single packet.
		const ENetMode NetMode = World->GetNetMode();
		if (NetMode == NM_Standalone)
		{
			UE_LOG(LogHayGauntlet, Error, TEXT("HayDigTest: running standalone, the command line URL was dropped. Shipping needs UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING=1."));
			EndTest(1);
			return;
		}
		UE_LOG(LogHayGauntlet, Log, TEXT("HayDigTest: net mode %s"), NetModeName(NetMode));

		UHayDigBotComponent* Bot = NewObject<UHayDigBotComponent>(Controller, TEXT("HayDigBot"));
		Bot->bDigTowardNeedle = bTowardNeedle;
		Bot->RegisterComponent();
		bBotAdded = true;
		MarkHeartbeatActive(TEXT("digging"));
	}
}

void UHayDigTestController::OnNeedleFound(const FUniqueNetIdRepl& Player)
{
	if (!bFound)
	{
		bFound = true;
		FoundSeconds = FPlatformTime::Seconds();
		MarkHeartbeatActive(TEXT("needle found"));
		const UWorld*			World = GetWorld();
		const AGameStateBase*	GameState = World ? World->GetGameState() : nullptr;
		UE_LOG(LogHayGauntlet, Log, TEXT("HayDigTest: needle found after %.0f s with %d players"), FoundSeconds - StartSeconds, GameState ? GameState->PlayerArray.Num() : 0);
	}
}
