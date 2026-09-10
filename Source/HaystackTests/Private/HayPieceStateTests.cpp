// Copyright (c) 2026 Vanan Andreas.

#include "HayPile.h"
#include "HayPile/HayLayoutComponent.h"
#include "HayPile/HayPieceStateComponent.h"
#include "HayPile/HayRenderComponent.h"

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Misc/AutomationTest.h"
#include "Online/CoreOnline.h"
#include "OnlineSubsystemTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * A game world with one small pile that has begun play. Destroyed with the scope.
 */
struct FHayTestPileWorld
{
	UWorld*	  World = nullptr;
	AHayPile* Pile = nullptr;

	FHayTestPileWorld(FAutomationTestBase& Test)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HayTestWorld"));
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);

		Pile = World->SpawnActor<AHayPile>(AHayPile::StaticClass(), FTransform(FVector(100.f, 20.f, 130.f)));
		Pile->GetLayout()->NumPieces = 20000;
		Pile->GetLayout()->DomeRadius = 200.f;
		Pile->GetPieceState()->NeedleDepth = FFloatInterval(10.f, 60.f);
		Pile->GetRender()->HayMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Geometry/Meshes/SM_Hay.SM_Hay"));
		if (!Pile->GetRender()->HayMesh)
		{
			Test.AddError(TEXT("SM_Hay not found, the pile cannot initialize"));
		}

		// No game instance or game mode here, so begin play goes straight to the one actor under test.
		World->InitializeActorsForPlay(FURL());
		Pile->DispatchBeginPlay();
	}

	~FHayTestPileWorld()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHayPieceStateTakeAndPlace, "Haystack.PieceState.TakeAndPlace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FHayPieceStateTakeAndPlace::RunTest(const FString& Parameters)
{
	FHayTestPileWorld		 Scope(*this);
	UHayPieceStateComponent* State = Scope.Pile->GetPieceState();
	constexpr int32			 Piece = 123;

	TestFalse(TEXT("untouched piece is not moved"), State->IsPieceMoved(Piece));
	TestTrue(TEXT("first take succeeds"), State->TakePiece(Piece));
	TestTrue(TEXT("taken piece is moved"), State->IsPieceMoved(Piece));
	TestFalse(TEXT("taking a held piece fails"), State->TakePiece(Piece));
	TestEqual(TEXT("one entry in the moved list"), State->GetMovedPieces().Num(), 1);
	TestEqual(TEXT("entry is held"), State->GetMovedPieces()[0].State, EHayPieceState::Held);

	const FTransform Drop(FQuat::Identity, FVector(500.f, 0.f, 10.f));
	TestFalse(TEXT("placing a piece nobody holds fails"), State->PlacePiece(Piece + 1, Drop));
	TestTrue(TEXT("placing the held piece succeeds"), State->PlacePiece(Piece, Drop));
	TestEqual(TEXT("entry is loose"), State->GetMovedPieces()[0].State, EHayPieceState::Loose);
	TestTrue(TEXT("world transform is where it was placed"), State->GetPieceWorldTransform(Piece).GetLocation().Equals(Drop.GetLocation(), 0.01f));
	TestFalse(TEXT("placing twice fails"), State->PlacePiece(Piece, Drop));

	TestTrue(TEXT("a loose piece can be taken again"), State->TakePiece(Piece));
	TestEqual(TEXT("still one entry"), State->GetMovedPieces().Num(), 1);

	TestFalse(TEXT("out of range index fails"), State->TakePiece(Scope.Pile->GetLayout()->NumPieces));
	TestFalse(TEXT("negative index fails"), State->TakePiece(-1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHayPieceStateNeedle, "Haystack.PieceState.Needle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FHayPieceStateNeedle::RunTest(const FString& Parameters)
{
	FHayTestPileWorld		   Scope(*this);
	UHayPieceStateComponent*   State = Scope.Pile->GetPieceState();
	const UHayLayoutComponent* Layout = Scope.Pile->GetLayout();

	const int32 Needle = State->GetNeedlePiece();
	TestTrue(TEXT("needle picked"), Needle >= 0 && Needle < Layout->NumPieces);
	const float Depth = Layout->DomeRadius - Layout->GetPieceLocalTransform(Needle).GetLocation().Length();
	TestTrue(TEXT("needle inside the depth band"), State->NeedleDepth.Contains(Depth));
	TestFalse(TEXT("not found at start"), State->IsNeedleFound());

	int32 Broadcasts = 0;
	State->OnNeedleFound.AddLambda([&Broadcasts](const FUniqueNetIdRepl&) { ++Broadcasts; });

	FUniqueNetIdRepl Player;
	Player.SetUniqueNetId(FUniqueNetIdString::Create(TEXT("tester"), FName(TEXT("NULL"))));
	TestTrue(TEXT("test id is valid"), Player.IsValid());

	State->NotifyPieceTaken(Needle == 0 ? 1 : 0, Player);
	TestFalse(TEXT("another piece does not count"), State->IsNeedleFound());

	State->NotifyPieceTaken(Needle, Player);
	TestTrue(TEXT("needle found"), State->IsNeedleFound());
	TestTrue(TEXT("finder recorded"), State->GetNeedleFoundBy() == Player);
	TestEqual(TEXT("delegate fired once"), Broadcasts, 1);

	FUniqueNetIdRepl Second;
	Second.SetUniqueNetId(FUniqueNetIdString::Create(TEXT("second"), FName(TEXT("NULL"))));
	State->NotifyPieceTaken(Needle, Second);
	TestTrue(TEXT("first finder stays"), State->GetNeedleFoundBy() == Player);
	TestEqual(TEXT("delegate not fired again"), Broadcasts, 1);
	return true;
}

#endif
