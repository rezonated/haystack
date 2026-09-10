// Copyright (c) 2026 Vanan Andreas.

#include "HayPerfTestController.h"
#include "HayPile.h"
#include "HaystackGauntlet.h"
#include "HayPile/HayRenderComponent.h"

#include "EngineUtils.h"
#include "RHI.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "UnrealEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayPerfTestController)

void UHayPerfTestController::OnInit()
{
	FParse::Value(FCommandLine::Get(), TEXT("HayPerfOut="), OutDir);
	FParse::Value(FCommandLine::Get(), TEXT("HayPerfLabel="), Label);
	FParse::Value(FCommandLine::Get(), TEXT("HayPerfWarmup="), WarmupSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("HayPerfCapture="), CaptureSeconds);

	if (OutDir.IsEmpty())
	{
		OutDir = FPaths::ProfilingDir() / TEXT("HayPerf");
	}
	if (Label.IsEmpty())
	{
		Label = FString::Printf(TEXT("%s_%dx%d"), LexToString(FApp::GetBuildConfiguration()), GSystemResolution.ResX, GSystemResolution.ResY);
	}
	IFileManager::Get().MakeDirectory(*OutDir, true);

	PhaseStartSeconds = FPlatformTime::Seconds();
	UE_LOG(LogHayGauntlet, Log, TEXT("HayPerfTest: label %s, warmup %.0f s, capture %.0f s, output %s"), *Label, WarmupSeconds, CaptureSeconds, *OutDir);
}

void UHayPerfTestController::OnTick(const float TimeDelta)
{
	const double Now = FPlatformTime::Seconds();
	UWorld*		 World = GetWorld();

	switch (Phase)
	{
		case EPhase::WaitingForSpawn:
		{
			AHayPile* Pile = nullptr;
			for (AHayPile* WorldPile : TActorRange<AHayPile>(World))
			{
				Pile = WorldPile;
				break;
			}
			if (!Pile || !Pile->GetRender()->IsInitialSpawnDone())
			{
				return;
			}

			// Look at the pile, the measurement is about the hay on screen.
			APlayerController* Controller = GetFirstPlayerController();
			if (Controller && Controller->GetPawn())
			{
				Controller->SetControlRotation((Pile->GetActorLocation() - Controller->GetPawn()->GetPawnViewLocation()).Rotation());
			}

			Phase = EPhase::WarmingUp;
			PhaseStartSeconds = Now;
			MarkHeartbeatActive(TEXT("spawned, warming up"));
			return;
		}

		case EPhase::WarmingUp:
			if (Now - PhaseStartSeconds >= WarmupSeconds)
			{
				BeginCapture();
				Phase = EPhase::Capturing;
				PhaseStartSeconds = Now;
			}
			return;

		case EPhase::Capturing:
			if (Now - PhaseStartSeconds >= CaptureSeconds)
			{
				EndCapture();
				Phase = EPhase::Finishing;
				PhaseStartSeconds = Now;
			}
			return;

		case EPhase::Finishing:
			if (CsvFile.IsValid() && !CsvFile.IsReady())
			{
				return;
			}
			CsvPath = CsvFile.IsValid() ? CsvFile.Get() : FString();
			WriteSummary();
			EndTest(0);
			return;
	}
}

void UHayPerfTestController::BeginCapture()
{
	FrameMs.Reserve(static_cast<int32>(CaptureSeconds * 240.f));
	GameThreadMs.Reserve(FrameMs.Max());
	RenderThreadMs.Reserve(FrameMs.Max());
	GpuMs.Reserve(FrameMs.Max());
	SampleHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHayPerfTestController::SampleFrame));

#if CSV_PROFILER
	FCsvProfiler::Get()->BeginCapture(-1, OutDir, Label);
#endif
	MarkHeartbeatActive(TEXT("capturing"));
	UE_LOG(LogHayGauntlet, Log, TEXT("HayPerfTest: capture started"));
}

bool UHayPerfTestController::SampleFrame(const float DeltaSeconds)
{
	FrameMs.Add(DeltaSeconds * 1000.f);
	GameThreadMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
	RenderThreadMs.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
	GpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	return true;
}

void UHayPerfTestController::EndCapture()
{
	FTSTicker::GetCoreTicker().RemoveTicker(SampleHandle);
	SampleHandle.Reset();
#if CSV_PROFILER
	CsvFile = FCsvProfiler::Get()->EndCapture();
#endif
	UE_LOG(LogHayGauntlet, Log, TEXT("HayPerfTest: capture ended after %d frames"), FrameMs.Num());
}

float UHayPerfTestController::Percentile(TArray<float>& Sorted, const float Fraction)
{
	if (Sorted.IsEmpty())
	{
		return 0.f;
	}
	Sorted.Sort();
	return Sorted[FMath::Clamp(FMath::RoundToInt32(Fraction * (Sorted.Num() - 1)), 0, Sorted.Num() - 1)];
}

void UHayPerfTestController::WriteSummary()
{
	auto Average = [](const TArray<float>& Values)
	{
		double Sum = 0.0;
		for (const float Value : Values)
		{
			Sum += Value;
		}
		return Values.IsEmpty() ? 0.0 : Sum / Values.Num();
	};

	const double FrameAverage = Average(FrameMs);
	const float	 FrameP99 = Percentile(FrameMs, 0.99f);
	const double UsedPhysicalMb = FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0);

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("label=%s"), *Label));
	Lines.Add(FString::Printf(TEXT("configuration=%s"), LexToString(FApp::GetBuildConfiguration())));
	Lines.Add(FString::Printf(TEXT("resolution=%dx%d"), GSystemResolution.ResX, GSystemResolution.ResY));
	Lines.Add(FString::Printf(TEXT("frames=%d"), FrameMs.Num()));
	Lines.Add(FString::Printf(TEXT("frame_avg_ms=%.2f"), FrameAverage));
	Lines.Add(FString::Printf(TEXT("frame_p99_ms=%.2f"), FrameP99));
	Lines.Add(FString::Printf(TEXT("fps_avg=%.1f"), FrameAverage > 0.0 ? 1000.0 / FrameAverage : 0.0));
	Lines.Add(FString::Printf(TEXT("game_thread_avg_ms=%.2f"), Average(GameThreadMs)));
	Lines.Add(FString::Printf(TEXT("render_thread_avg_ms=%.2f"), Average(RenderThreadMs)));
	Lines.Add(FString::Printf(TEXT("gpu_avg_ms=%.2f"), Average(GpuMs)));
	Lines.Add(FString::Printf(TEXT("used_physical_mb=%.0f"), UsedPhysicalMb));
	Lines.Add(FString::Printf(TEXT("csv=%s"), *CsvPath));

	const FString SummaryPath = OutDir / Label + TEXT(".txt");
	FFileHelper::SaveStringArrayToFile(Lines, *SummaryPath);

	UE_LOG(LogHayGauntlet, Log, TEXT("HayPerf: %s | frame %.2f ms avg, %.2f ms p99 (%.1f fps) | game %.2f render %.2f gpu %.2f ms | %.0f MB | %s"), *Label, FrameAverage, FrameP99, FrameAverage > 0.0 ? 1000.0 / FrameAverage : 0.0, Average(GameThreadMs), Average(RenderThreadMs), Average(GpuMs), UsedPhysicalMb, *SummaryPath);
}
