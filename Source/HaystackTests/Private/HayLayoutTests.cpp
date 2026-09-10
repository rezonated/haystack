// Copyright (c) 2026 Vanan Andreas.

#include "HayPile/HayLayoutComponent.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Small pile the tests can walk piece by piece.
 */
static UHayLayoutComponent* HayMakeTestLayout(const int32 NumPieces, const int32 Seed)
{
	UHayLayoutComponent* Layout = NewObject<UHayLayoutComponent>(GetTransientPackage());
	Layout->NumPieces = NumPieces;
	Layout->DomeRadius = 300.f;
	Layout->Seed = Seed;
	Layout->Build();
	return Layout;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHayLayoutCellsCoverPieces, "Haystack.Layout.CellsCoverEveryPiece", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FHayLayoutCellsCoverPieces::RunTest(const FString& Parameters)
{
	const UHayLayoutComponent* Layout = HayMakeTestLayout(100000, 7);
	const TArray<FHayCell>&	   Cells = Layout->GetCells();

	TestTrue(TEXT("built"), Layout->IsBuilt());
	TestTrue(TEXT("at least two shells"), Layout->GetNumShells() >= 2);

	int32 Covered = 0;
	int32 Expected = 0;
	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		const FHayCell& Cell = Cells[CellIndex];
		TestEqual(TEXT("cells are contiguous"), Cell.FirstPiece, Expected);
		Expected += Cell.PieceCount;
		Covered += Cell.PieceCount;
		TestTrue(TEXT("shell index within range"), Cell.Shell >= 0 && Cell.Shell < Layout->GetNumShells());
		TestTrue(TEXT("shell start bounds the cell"), CellIndex >= Layout->GetShellStart(Cell.Shell) && CellIndex < Layout->GetShellStart(Cell.Shell + 1));
	}
	TestEqual(TEXT("every piece belongs to one cell"), Covered, Layout->NumPieces);
	TestEqual(TEXT("last shell start is one past the end"), Layout->GetShellStart(Layout->GetNumShells()), Cells.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHayLayoutPiecesInsideCells, "Haystack.Layout.PiecesLieInsideTheirCell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FHayLayoutPiecesInsideCells::RunTest(const FString& Parameters)
{
	const UHayLayoutComponent* Layout = HayMakeTestLayout(100000, 7);
	const TArray<FHayCell>&	   Cells = Layout->GetCells();

	// Cell edges are exact on the generated coordinates, so only rounding needs slack.
	constexpr float Slack = 0.01f;
	int32			Outside = 0;
	int32			WrongCell = 0;
	for (int32 PieceIndex = 0; PieceIndex < Layout->NumPieces; ++PieceIndex)
	{
		const int32 CellIndex = Layout->GetCellOfPiece(PieceIndex);
		const FHayCell& Cell = Cells[CellIndex];
		if (PieceIndex < Cell.FirstPiece || PieceIndex >= Cell.FirstPiece + Cell.PieceCount)
		{
			++WrongCell;
			continue;
		}

		const FVector Location = Layout->GetPieceLocalTransform(Cell, PieceIndex).GetLocation();
		const float	  Radius = Location.Length();
		const float	  CosTheta = Radius > 0.f ? Location.Z / Radius : 1.f;
		float		  Phi = FMath::Atan2(Location.Y, Location.X);
		if (Phi < Cell.PhiStart - Slack)
		{
			Phi += 2.f * PI;
		}

		const bool bInside = Radius >= Cell.InnerRadius - Slack && Radius <= Cell.OuterRadius + Slack && CosTheta >= Cell.CosThetaMin - Slack && CosTheta <= Cell.CosThetaMax + Slack && Phi >= Cell.PhiStart - Slack && Phi <= Cell.PhiEnd + Slack;
		Outside += bInside ? 0 : 1;

		if (bInside && Layout->FindCell(Cell.Shell, Location) != CellIndex)
		{
			++WrongCell;
		}
	}

	TestEqual(TEXT("pieces outside their cell bounds"), Outside, 0);
	TestEqual(TEXT("pieces whose cell lookup disagrees"), WrongCell, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHayLayoutDeterministic, "Haystack.Layout.SameSeedSamePile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FHayLayoutDeterministic::RunTest(const FString& Parameters)
{
	const UHayLayoutComponent* A = HayMakeTestLayout(50000, 1337);
	const UHayLayoutComponent* B = HayMakeTestLayout(50000, 1337);
	const UHayLayoutComponent* C = HayMakeTestLayout(50000, 1338);

	int32 Differences = 0;
	int32 SameAcrossSeeds = 0;
	for (int32 PieceIndex = 0; PieceIndex < A->NumPieces; PieceIndex += 97)
	{
		const FTransform TransformA = A->GetPieceLocalTransform(PieceIndex);
		Differences += TransformA.Equals(B->GetPieceLocalTransform(PieceIndex), 0.f) ? 0 : 1;
		SameAcrossSeeds += TransformA.GetLocation().Equals(C->GetPieceLocalTransform(PieceIndex).GetLocation(), 0.001f) ? 1 : 0;
	}

	TestEqual(TEXT("same seed, same transforms bit for bit"), Differences, 0);
	TestEqual(TEXT("different seed, different pile"), SameAcrossSeeds, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHayPieceRandomStream, "Haystack.Layout.PieceRandomStream", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FHayPieceRandomStream::RunTest(const FString& Parameters)
{
	FHayPieceRandom A(1337, 42);
	FHayPieceRandom B(1337, 42);
	FHayPieceRandom Other(1337, 43);

	constexpr int32 Draws = 100000;
	double			Sum = 0.0;
	int32			OutOfRange = 0;
	int32			Mismatch = 0;
	int32			SameAsOther = 0;
	for (int32 Draw = 0; Draw < Draws; ++Draw)
	{
		const float ValueA = A.Next01();
		const float ValueB = B.Next01();
		const float ValueOther = Other.Next01();
		Sum += ValueA;
		OutOfRange += ValueA >= 0.f && ValueA < 1.f ? 0 : 1;
		Mismatch += ValueA == ValueB ? 0 : 1;
		SameAsOther += ValueA == ValueOther ? 1 : 0;
	}

	TestEqual(TEXT("values outside [0, 1)"), OutOfRange, 0);
	TestEqual(TEXT("two streams with the same seed and piece disagree"), Mismatch, 0);
	TestTrue(TEXT("neighbouring pieces get different streams"), SameAsOther < Draws / 1000);
	TestTrue(TEXT("mean close to one half"), FMath::Abs(Sum / Draws - 0.5) < 0.01);

	FHayPieceRandom Signed(1, 1);
	int32			SignedOutOfRange = 0;
	for (int32 Draw = 0; Draw < 1000; ++Draw)
	{
		const float Value = Signed.NextSigned();
		SignedOutOfRange += Value >= -1.f && Value < 1.f ? 0 : 1;
	}
	TestEqual(TEXT("signed values outside [-1, 1)"), SignedOutOfRange, 0);
	return true;
}

#endif
