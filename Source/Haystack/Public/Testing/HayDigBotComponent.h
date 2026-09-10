// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HayDigBotComponent.generated.h"

class AHayPile;
class UHayInteractionComponent;
struct FUniqueNetIdRepl;

/**
 * Drives the local player's pawn to dig the way a player would, without a human.
 * Sits on the player controller. It picks a spot on the dome, walks to the pile edge beside it, then aims the view at a
 * fresh point around the spot for every take and takes whatever piece the crosshair ray hits first, through the pawn's
 * interaction component. Removing the front piece exposes the next one along the ray, so the holes deepen like a real
 * dig. Pieces are held briefly and thrown away from the pile. After SecondsPerSpot or too many empty aims it moves to a
 * new spot. With bDigTowardNeedle the spot is the needle's location, so the run ends once the needle is the first piece
 * the ray meets. Walks with the NavMesh when the level has one, straight otherwise. Stops once anyone finds the needle.
 * Toggled with the console command Hay.DigBot, optional argument "needle", or added by a test controller.
 */
UCLASS(ClassGroup = Hay, MinimalAPI)
class UHayDigBotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHayDigBotComponent();

	/**
	 * Dig at the needle instead of random spots. For functional tests that must finish.
	 */
	UPROPERTY(EditAnywhere, Category = Hay)
	bool bDigTowardNeedle = false;

	/**
	 * Seconds of digging at one spot before choosing another. Ignored when digging toward the needle.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 1))
	float SecondsPerSpot = 5.f;

	/**
	 * Aims that hit nothing before the spot is abandoned.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 1))
	int32 MissesPerSpot = 10;

	/**
	 * Every take aims at a fresh point within this distance of the spot, cm, never closer than half of it to the last aim.
	 * The dig becomes a patch of holes rather than one tunnel. Zero aims at the spot itself every time.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0))
	float AimSpreadRadius = 60.f;

	/**
	 * How far outside the dome the bot stands, cm.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0))
	float StandDistance = 100.f;

	/**
	 * The standing point swings up to this many degrees around the pile from the spot's own direction, so two bots
	 * digging at the same spot do not queue for the same square meter.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0, ClampMax = 90))
	float StandSpreadDegrees = 30.f;

	/**
	 * Distance to the standing point that counts as arrived, cm.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 10))
	float ArriveRadius = 80.f;

	/**
	 * Seconds without progress toward the standing point before the spot is abandoned.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0.5))
	float StuckSeconds = 3.f;

	/**
	 * Seconds of walking, progress or not, before the spot is abandoned.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 5))
	float WalkTimeoutSeconds = 60.f;

	/**
	 * Seconds between a throw and the next take.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0))
	float TakeInterval = 0.5f;

	/**
	 * Seconds a piece stays in hand before the throw.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0))
	float HoldSeconds = 0.3f;

	/**
	 * Seconds to wait for the server to confirm a take before aiming again.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0.1))
	float RequestTimeout = 2.f;

	/**
	 * Fraction of the interaction reach a spot may be from the standing point's eyes, so latency never pushes a grab out of range.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 0.1, ClampMax = 1))
	float ReachFraction = 0.8f;

	/**
	 * Console command Hay.DigBot [on|off|needle]. No argument toggles the bot on the first local player controller,
	 * "on" and "needle" make sure it runs, "off" removes it. Waits for the controller when none exists yet, as on a
	 * client that is still connecting. The engine runs -ExecCmds once per map load, so scripts should pass "on" or
	 * "needle" rather than rely on the toggle.
	 */
	static void ToggleOnLocalPlayer(const TArray<FString>& Args, UWorld* World);

	virtual void BeginPlay() override;

	virtual void TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	static constexpr int32 ProgressLogEveryTakes = 20;

	static constexpr float WaitForControllerSeconds = 60.f;

	static constexpr float WaitForControllerPollSeconds = 0.5f;

	enum class ERequest : uint8
	{
		Toggle,
		On,
		Off,
	};

	/**
	 * True when a local controller exists and the request was applied to it.
	 */
	static bool Apply(UWorld* World, const ERequest Request, const bool bTowardNeedle);

	/**
	 * Logs the run and stops ticking. Whoever found it.
	 */
	void OnNeedleFound(const FUniqueNetIdRepl& Player);

	/**
	 * Picks the dig spot and the standing point beside it. Random spots are rerolled until the spot is within reach of the eyes.
	 */
	void ChooseSpot(const APawn* Pawn, const float Reach);

	/**
	 * Moves the pawn toward the standing point with movement input along a NavMesh path, straight when there is none.
	 * Input replicates like a player's keys, so this works on clients where path following would be corrected away.
	 * True once it is there.
	 */
	bool WalkToStandPoint(APawn* Pawn, const float DeltaTime);

	/**
	 * Aims at the next point of the spot and asks for the first piece the ray hits.
	 */
	void Dig(APawn* Pawn, UHayInteractionComponent* Interaction, const double Now);

	/**
	 * Random point around the spot, kept away from the previous aim so consecutive takes hit different places.
	 */
	FVector NextAimPoint();

	UPROPERTY(Transient)
	TObjectPtr<AHayPile> Pile = nullptr;

	FVector DigSpot = FVector::ZeroVector;

	FVector LastAimPoint = FVector::ZeroVector;

	FVector StandPoint = FVector::ZeroVector;

	bool bAtStandPoint = false;

	/**
	 * Distance to a path point that counts as reached, cm.
	 */
	static constexpr float WaypointRadius = 60.f;

	bool bPathBuilt = false;

	TArray<FVector> PathPoints;

	int32 PathIndex = 0;

	float SecondsWithoutProgress = 0.f;

	float SecondsWalking = 0.f;

	float BestDistanceToStand = 0.f;

	int32 TakesAtSpot = 0;

	int32 MissesAtSpot = 0;

	double DiggingSinceSeconds = 0.0;

	int32 PendingTake = INDEX_NONE;

	/**
	 * A throw was requested and the held piece has not cleared yet.
	 */
	bool bPendingDrop = false;

	double NextActionSeconds = 0.0;

	double PendingSinceSeconds = 0.0;

	double StartSeconds = 0.0;

	int32 Takes = 0;

	int32 Spots = 0;
};
