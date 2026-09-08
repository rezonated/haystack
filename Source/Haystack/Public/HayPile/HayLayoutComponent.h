// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HayLayoutComponent.generated.h"

/**
 * One spherical cell of the dome: a radial shell cut into cos(theta) bands and
 * azimuth wedges. Its pieces are a contiguous range of piece indices.
 */
struct FHayCell
{
	/** Radial limits of the shell, cm. */
	float InnerRadius = 0.f;
	float OuterRadius = 0.f;

	/** cos(theta) limits of the band, theta measured from straight up, so Max is the pole side. */
	float CosThetaMin = 0.f;
	float CosThetaMax = 1.f;

	/** Azimuth limits of the wedge, radians. */
	float PhiStart = 0.f;
	float PhiEnd = 0.f;

	/** Contiguous piece index range this cell owns. */
	int32 FirstPiece = 0;
	int32 PieceCount = 0;

	/** 0 is the outermost shell. */
	int32 Shell = 0;
};

/**
 * Random number stream for one piece, derived from (Seed, PieceIndex) alone.
 * HashCombine is the engine mixer documented as never changing, so server and clients get the same stream.
 */
struct FHayPieceRandom
{
	FHayPieceRandom(const uint32 Seed, const uint32 PieceIndex);

	/** [0, 1) */
	float Next01();

	/** [-1, 1) */
	float NextSigned();

private:
	uint32 Base = 0;

	uint32 DrawCount = 0;
};

/**
 * Where every hay piece is.
 * The dome is cut into shells, bands and wedges, and each piece's transform is a pure function of (Seed, PieceIndex), so every machine computes the same pile and nothing per piece is stored or sent.
 */
UCLASS(ClassGroup = Hay, meta = (BlueprintSpawnableComponent))
class UHayLayoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 1))
	int32 NumPieces = 4000000;

	/**
	 * Hemisphere radius, cm.
	 * Pieces fill its volume uniformly.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 100))
	float DomeRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = Hay)
	int32 Seed = 1337;

	/**
	 * Radial thickness of one shell, cm.
	 * Sight into the pile ends after about 15 cm at 4M pieces, so 20 cm shells stay opaque.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 5))
	float ShellThickness = 20.f;

	/**
	 * Pieces per cell the partition aims for.
	 * Smaller cells cull tighter and update cheaper, more cells cost more components.
	 */
	UPROPERTY(EditAnywhere, Category = Hay, meta = (ClampMin = 1024))
	int32 TargetPiecesPerCell = 32768;

	/**
	 * Cuts the dome into cells from the properties above.
	 */
	void Build();

	bool IsBuilt() const { return !Cells.IsEmpty(); }

	const TArray<FHayCell>& GetCells() const { return Cells; }

	int32 GetNumShells() const { return ShellStart.Num() - 1; }

	/**
	 * Index of the first cell of a shell. Shell == NumShells gives one past the last cell.
	 */
	int32 GetShellStart(const int32 Shell) const { return ShellStart[Shell]; }

	/**
	 * Cell that holds a piece index.
	 */
	int32 GetCellOfPiece(const int32 PieceIndex) const;

	/**
	 * Cell of a shell that contains a direction from the dome center.
	 */
	int32 FindCell(const int32 Shell, const FVector& LocalDirection) const;

	/**
	 * Transform of one piece relative to the pile actor.
	 */
	FTransform GetPieceLocalTransform(const int32 PieceIndex) const;

	/**
	 * Same as GetPieceLocalTransform, when the caller already knows the cell.
	 */
	FTransform GetPieceLocalTransform(const FHayCell& Cell, const int32 PieceIndex) const;

private:
	/**
	 * Cube root by Newton steps, starting from a value known to be at or above the answer.
	 */
	static float CubeRootFromAbove(const float Cubed, float Estimate);

	static constexpr int32 CubeRootNewtonSteps = 8;

	/**
	 * Rejects quaternion candidates too close to the origin, where normalizing would amplify rounding.
	 */
	static constexpr float MinQuaternionLengthSquared = 0.01f;

	TArray<FHayCell> Cells = {};

	/**
	 * First cell index of each shell, plus one past the end.
	 */
	TArray<int32> ShellStart = {};

	/**
	 * Bands and wedges of each shell's partition.
	 */
	TArray<FIntPoint> ShellGrid = {};
};