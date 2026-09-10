// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "GauntletTestController.h"
#include "Containers/Ticker.h"
#include "Async/Future.h"
#include "HayPerfTestController.generated.h"

class AHayPile;

/**
 * Gauntlet controller for the idle performance test.
 * Waits for the pile's outer shells to spawn, aims the player at the pile, warms up, then records a CSV profiler capture
 * and its own per frame samples for a fixed time. Ends with one HayPerf log line and a summary text file next to the
 * CSV, so a Shipping build without the CSV profiler still leaves numbers behind.
 *
 * Command line: -HayPerfOut=<dir>, -HayPerfLabel=<name>, -HayPerfWarmup=<seconds> default 5, -HayPerfCapture=<seconds>
 * default 60.
 */
UCLASS()
class UHayPerfTestController : public UGauntletTestController
{
	GENERATED_BODY()

public:
	virtual void OnInit() override;

	virtual void OnTick(const float TimeDelta) override;

private:
	enum class EPhase : uint8
	{
		WaitingForSpawn,
		WarmingUp,
		Capturing,
		Finishing,
	};

	/**
	 * Per frame sample, off the core ticker so the 1 Hz Gauntlet tick does not limit it.
	 */
	bool SampleFrame(const float DeltaSeconds);

	void BeginCapture();

	void EndCapture();

	void WriteSummary();

	static float Percentile(TArray<float>& Sorted, const float Fraction);

	static constexpr float DefaultWarmupSeconds = 5.f;

	static constexpr float DefaultCaptureSeconds = 60.f;

	FString OutDir;

	FString Label;

	float WarmupSeconds = DefaultWarmupSeconds;

	float CaptureSeconds = DefaultCaptureSeconds;

	EPhase Phase = EPhase::WaitingForSpawn;

	double PhaseStartSeconds = 0.0;

	FTSTicker::FDelegateHandle SampleHandle;

	TArray<float> FrameMs;

	TArray<float> GameThreadMs;

	TArray<float> RenderThreadMs;

	TArray<float> GpuMs;

	TSharedFuture<FString> CsvFile;

	FString CsvPath;
};
