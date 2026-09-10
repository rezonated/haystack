// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "GauntletTestController.h"
#include "HayDigTestController.generated.h"

class AHayPile;
struct FUniqueNetIdRepl;

/**
 * Gauntlet controller for the multiplayer dig test, run on every role.
 * Puts a dig bot on the local player once it has a pawn and passes when anyone finds the needle. A listen server waits a
 * few seconds after the find so the clients receive it before the host exits. Fails on the timeout.
 *
 * Command line: -HayDigTimeout=<seconds>, default 600. -HayDigNeedle makes the bot dig straight at the needle.
 */
UCLASS()
class UHayDigTestController : public UGauntletTestController
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;

	virtual void OnTick(const float TimeDelta) override;

private:
	void OnNeedleFound(const FUniqueNetIdRepl& Player);

	static constexpr float DefaultTimeoutSeconds = 600.f;

	/**
	 * Seconds a listen server keeps running after the find, so the last replication reaches the clients.
	 */
	static constexpr double HostExitDelaySeconds = 3.0;

	float TimeoutSeconds = DefaultTimeoutSeconds;

	bool bTowardNeedle = false;

	bool bBotAdded = false;

	bool bFound = false;

	double StartSeconds = 0.0;

	double FoundSeconds = 0.0;

	TWeakObjectPtr<AHayPile> SubscribedPile;
};
