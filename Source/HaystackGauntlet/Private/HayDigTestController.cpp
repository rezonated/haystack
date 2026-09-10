// Copyright (c) 2026 Vanan Andreas.

#include "HayDigTestController.h"
#include "HayPile.h"
#include "HaystackGauntlet.h"
#include "HayPile/HayPieceStateComponent.h"
#include "Testing/HayDigBotComponent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayDigTestController)

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
		UE_LOG(LogHayGauntlet, Log, TEXT("HayDigTest: needle found after %.0f s"), FoundSeconds - StartSeconds);
	}
}
