// Copyright (c) 2026 Vanan Andreas.

#include "HayLayoutComponent.h"

#include "Templates/TypeHash.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayLayoutComponent)

/**
 * A float holds 24 mantissa bits, so the top 24 bits of a hash map onto [0, 1) without rounding.
 */
static constexpr uint32 HayFloatMantissaBits = 24;
static constexpr float	HayOneOverMantissaRange = 1.0f / 16777216.0f;

FHayPieceRandom::FHayPieceRandom(const uint32 Seed, const uint32 PieceIndex)
	: Base(HashCombine(Seed, PieceIndex))
{
}

float FHayPieceRandom::Next01()
{
	const uint32 Hash = HashCombine(Base, ++DrawCount);
	return (Hash >> (32 - HayFloatMantissaBits)) * HayOneOverMantissaRange;
}

float FHayPieceRandom::NextSigned()
{
	return Next01() * 2.f - 1.f;
}

float UHayLayoutComponent::CubeRootFromAbove(const float Cubed, float Estimate)
{
	for (int32 Step = 0; Step < CubeRootNewtonSteps && Estimate > 0.f; ++Step)
	{
		Estimate -= (Estimate * Estimate * Estimate - Cubed) / (3.f * Estimate * Estimate);
	}

	return FMath::Max(Estimate, 0.f);
}

void UHayLayoutComponent::Build()
{
	Cells.Reset();
	ShellStart.Reset();
	ShellGrid.Reset();

	const int32	 NumShells = FMath::Max(1, FMath::CeilToInt(DomeRadius / ShellThickness));
	const double Radius = DomeRadius;
	const double HemisphereVolume = (2.0 * PI / 3.0) * Radius * Radius * Radius;
	const double Density = NumPieces / HemisphereVolume;

	double RunningPieceCount = 0.0;
	for (int32 Shell = 0; Shell < NumShells; ++Shell)
	{
		ShellStart.Add(Cells.Num());

		const double OuterRadius = Radius - Shell * ShellThickness;
		const double InnerRadius = FMath::Max(0.0, OuterRadius - ShellThickness);
		const double ShellPieces = Density * (2.0 * PI / 3.0) * (OuterRadius * OuterRadius * OuterRadius - InnerRadius * InnerRadius * InnerRadius);

		const int32 NumSectors = FMath::Clamp(FMath::RoundToInt32(ShellPieces / TargetPiecesPerCell), 1, 512);
		const int32 NumBands = FMath::Max(1, FMath::RoundToInt32(FMath::Sqrt(NumSectors / 3.0)));
		const int32 NumWedges = FMath::Max(1, FMath::RoundToInt32(static_cast<double>(NumSectors) / NumBands));
		const double CellPieces = ShellPieces / (NumBands * NumWedges);
		ShellGrid.Add(FIntPoint(NumBands, NumWedges));

		for (int32 Band = 0; Band < NumBands; ++Band)
		{
			for (int32 Wedge = 0; Wedge < NumWedges; ++Wedge)
			{
				FHayCell& Cell = Cells.AddDefaulted_GetRef();
				Cell.InnerRadius = static_cast<float>(InnerRadius);
				Cell.OuterRadius = static_cast<float>(OuterRadius);
				Cell.CosThetaMax = 1.f - static_cast<float>(Band) / NumBands;
				Cell.CosThetaMin = 1.f - static_cast<float>(Band + 1) / NumBands;
				Cell.PhiStart = 2.f * PI * Wedge / NumWedges;
				Cell.PhiEnd = 2.f * PI * (Wedge + 1) / NumWedges;
				Cell.Shell = Shell;
				Cell.FirstPiece = FMath::RoundToInt32(RunningPieceCount);
				RunningPieceCount += CellPieces;
				Cell.PieceCount = FMath::RoundToInt32(RunningPieceCount) - Cell.FirstPiece;
			}
		}
	}
	ShellStart.Add(Cells.Num());

	// Rounding drift lands on the last cell so the total is exactly NumPieces.
	FHayCell& LastCell = Cells.Last();
	LastCell.PieceCount = NumPieces - LastCell.FirstPiece;
}

int32 UHayLayoutComponent::GetCellOfPiece(const int32 PieceIndex) const
{
	// Binary search for the last cell whose first piece is at or before PieceIndex.
	int32 Low = 0;
	int32 High = Cells.Num() - 1;
	while (Low < High)
	{
		const int32 Middle = (Low + High + 1) / 2;
		Cells[Middle].FirstPiece <= PieceIndex ? Low = Middle : High = Middle - 1;
	}

	return Low;
}

int32 UHayLayoutComponent::FindCell(const int32 Shell, const FVector& LocalDirection) const
{
	const int32	  NumBands = ShellGrid[Shell].X;
	const int32	  NumWedges = ShellGrid[Shell].Y;
	const FVector Direction = LocalDirection.GetSafeNormal();
	float		  Phi = FMath::Atan2(Direction.Y, Direction.X);
	if (Phi < 0.f)
	{
		Phi += 2.f * PI;
	}

	// Direction.Z is cos(theta).
	// Bands count down from the pole, wedges count up from +X.
	const float CosTheta = FMath::Clamp(static_cast<float>(Direction.Z), 0.f, 1.f);
	const int32 Band = FMath::Clamp(FMath::FloorToInt((1.f - CosTheta) * NumBands), 0, NumBands - 1);
	const int32 Wedge = FMath::Clamp(FMath::FloorToInt(Phi / (2.f * PI) * NumWedges), 0, NumWedges - 1);

	return ShellStart[Shell] + Band * NumWedges + Wedge;
}

FTransform UHayLayoutComponent::GetPieceLocalTransform(const int32 PieceIndex) const
{
	return GetPieceLocalTransform(Cells[GetCellOfPiece(PieceIndex)], PieceIndex);
}

FTransform UHayLayoutComponent::GetPieceLocalTransform(const FHayCell& Cell, const int32 PieceIndex) const
{
	FHayPieceRandom Rand(static_cast<uint32>(Seed), static_cast<uint32>(PieceIndex));

	// Uniform inside the cell volume.
	// Volume grows with radius cubed, so the radius comes from a uniform draw between the cubed limits.
	// cos(theta) and phi are uniform between their limits.
	const float InnerCubed = Cell.InnerRadius * Cell.InnerRadius * Cell.InnerRadius;
	const float OuterCubed = Cell.OuterRadius * Cell.OuterRadius * Cell.OuterRadius;
	const float Radius = CubeRootFromAbove(InnerCubed + Rand.Next01() * (OuterCubed - InnerCubed), Cell.OuterRadius);
	const float CosTheta = Cell.CosThetaMin + Rand.Next01() * (Cell.CosThetaMax - Cell.CosThetaMin);
	const float SinTheta = FMath::Sqrt(FMath::Max(0.f, 1.f - CosTheta * CosTheta));
	float		SinPhi, CosPhi;
	FMath::SinCos(&SinPhi, &CosPhi, Cell.PhiStart + Rand.Next01() * (Cell.PhiEnd - Cell.PhiStart));
	const FVector Location(Radius * SinTheta * CosPhi, Radius * SinTheta * SinPhi, Radius * CosTheta);

	// Uniform rotation: a uniform point on the unit 4D sphere is a uniform unit quaternion.
	// Draw points in the 4D cube and keep the first one inside the ball, then normalize.
	FVector4f UnitQuaternion;
	for (;;)
	{
		UnitQuaternion = FVector4f(Rand.NextSigned(), Rand.NextSigned(), Rand.NextSigned(), Rand.NextSigned());
		const float LengthSquared = UnitQuaternion.X * UnitQuaternion.X + UnitQuaternion.Y * UnitQuaternion.Y + UnitQuaternion.Z * UnitQuaternion.Z + UnitQuaternion.W * UnitQuaternion.W;
		if (LengthSquared > 1.f || LengthSquared <= MinQuaternionLengthSquared)
		{
			continue;
		}

		UnitQuaternion /= FMath::Sqrt(LengthSquared);
		break;
	}

	return FTransform(FQuat(UnitQuaternion.X, UnitQuaternion.Y, UnitQuaternion.Z, UnitQuaternion.W), Location);
}