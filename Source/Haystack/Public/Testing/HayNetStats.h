// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class UNetDriver;
class UWorld;

/**
 * Periodic log of network and frame cost for the running game world.
 * Console command Hay.NetStats [seconds], default 5, 0 stops. Each line carries client connections, in and out KB/s
 * and packets/s from the net driver, and the average game thread and frame milliseconds over the interval.
 */
class FHayNetStats
{
public:
	static void Command(const TArray<FString>& Args, UWorld* World);

private:
	static bool Tick(const float DeltaSeconds);

	static void Log();

	/**
	 * Net driver of the running game world, null when there is none or no world.
	 */
	static const UNetDriver* FindGameNetDriver();

	static constexpr float DefaultIntervalSeconds = 5.f;

	static FTSTicker::FDelegateHandle TickerHandle;

	static float IntervalSeconds;

	static double AccumulatedSeconds;

	static double AccumulatedGameThreadMs;

	static int32 Frames;

	/**
	 * Cumulative driver totals at the last log, so bandwidth is a delta over the interval and needs no stat collection.
	 */
	static uint64 LastInTotalBytes;

	static uint64 LastOutTotalBytes;

	static double InPacketsSum;

	static double OutPacketsSum;

	static int32 NetSamples;
};
