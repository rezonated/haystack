// Copyright (c) 2026 Vanan Andreas.

#include "Testing/HayNetStats.h"
#include "Haystack.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Stats/Stats.h"

static FAutoConsoleCommandWithWorldAndArgs HayNetStatsCommand(TEXT("Hay.NetStats"), TEXT("Logs connections, bandwidth and game thread time every N seconds. 0 stops."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FHayNetStats::Command));

FTSTicker::FDelegateHandle FHayNetStats::TickerHandle;
float					   FHayNetStats::IntervalSeconds = DefaultIntervalSeconds;
double					   FHayNetStats::AccumulatedSeconds = 0.0;
double					   FHayNetStats::AccumulatedGameThreadMs = 0.0;
int32					   FHayNetStats::Frames = 0;
uint64					   FHayNetStats::LastInTotalBytes = 0;
uint64					   FHayNetStats::LastOutTotalBytes = 0;
double					   FHayNetStats::InPacketsSum = 0.0;
double					   FHayNetStats::OutPacketsSum = 0.0;
int32					   FHayNetStats::NetSamples = 0;

const UNetDriver* FHayNetStats::FindGameNetDriver()
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetNetDriver())
		{
			return Context.World()->GetNetDriver();
		}
	}
	return nullptr;
}

void FHayNetStats::Command(const TArray<FString>& Args, UWorld* World)
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	IntervalSeconds = Args.IsEmpty() ? DefaultIntervalSeconds : FCString::Atof(*Args[0]);
	if (IntervalSeconds <= 0.f)
	{
		UE_LOG(LogHay, Log, TEXT("NetStats off"));
		return;
	}

	AccumulatedSeconds = 0.0;
	AccumulatedGameThreadMs = 0.0;
	Frames = 0;
	LastInTotalBytes = LastOutTotalBytes = 0;
	InPacketsSum = OutPacketsSum = 0.0;
	NetSamples = 0;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FHayNetStats::Tick));
	UE_LOG(LogHay, Log, TEXT("NetStats every %.1f s"), IntervalSeconds);
}

bool FHayNetStats::Tick(const float DeltaSeconds)
{
	AccumulatedSeconds += DeltaSeconds;
	AccumulatedGameThreadMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
	++Frames;

	// Packet counters are per second and reset by the driver, so sample them every frame and average.
	if (const UNetDriver* NetDriver = FindGameNetDriver())
	{
		InPacketsSum += NetDriver->InPackets;
		OutPacketsSum += NetDriver->OutPackets;
		++NetSamples;
	}

	if (AccumulatedSeconds >= IntervalSeconds)
	{
		Log();
		AccumulatedSeconds = 0.0;
		AccumulatedGameThreadMs = 0.0;
		Frames = 0;
		InPacketsSum = OutPacketsSum = 0.0;
		NetSamples = 0;
	}
	return true;
}

void FHayNetStats::Log()
{
	int32  Connections = 0;
	double InKilobytesPerSecond = 0.0;
	double OutKilobytesPerSecond = 0.0;
	if (const UNetDriver* NetDriver = FindGameNetDriver())
	{
		Connections = NetDriver->ServerConnection ? 1 : NetDriver->ClientConnections.Num();

		// A driver created after the last log, as on a client that just connected, restarts the deltas.
		const uint64 InTotal = NetDriver->InTotalBytes;
		const uint64 OutTotal = NetDriver->OutTotalBytes;
		if (InTotal >= LastInTotalBytes && OutTotal >= LastOutTotalBytes && (LastInTotalBytes | LastOutTotalBytes) != 0)
		{
			InKilobytesPerSecond = (InTotal - LastInTotalBytes) / AccumulatedSeconds / 1024.0;
			OutKilobytesPerSecond = (OutTotal - LastOutTotalBytes) / AccumulatedSeconds / 1024.0;
		}
		LastInTotalBytes = InTotal;
		LastOutTotalBytes = OutTotal;
	}
	else
	{
		LastInTotalBytes = LastOutTotalBytes = 0;
	}

	const double Samples = FMath::Max(1, NetSamples);
	const double FrameMs = Frames > 0 ? AccumulatedSeconds * 1000.0 / Frames : 0.0;
	const double GameThreadMs = Frames > 0 ? AccumulatedGameThreadMs / Frames : 0.0;
	UE_LOG(LogHay, Log, TEXT("NetStats: %d connections | in %.1f KB/s %.0f pkt/s | out %.1f KB/s %.0f pkt/s | frame %.2f ms game %.2f ms"), Connections, InKilobytesPerSecond, InPacketsSum / Samples, OutKilobytesPerSecond, OutPacketsSum / Samples, FrameMs, GameThreadMs);
}
