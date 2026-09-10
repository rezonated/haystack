// Copyright (c) 2026 Vanan Andreas.

#include "HayPile/HayPickComponent.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHayPickRayHitsPiece, "Haystack.Pick.RayHitsPiece", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FHayPickRayHitsPiece::RunTest(const FString& Parameters)
{
	// A hay slab: 40 cm along X, 2.5 cm along Y, 1 cm along Z, sitting 100 cm down the ray.
	const FVector3f HalfExtents(20.f, 1.25f, 0.5f);
	const float		BoundRadius = HalfExtents.Size();
	const FVector3f Piece(100.f, 0.f, 0.f);
	const FVector3f Origin = FVector3f::ZeroVector;

	// Head on into the end of the slab: enters at 80 cm.
	{
		float Best = 1000.f;
		TestTrue(TEXT("head on hit"), UHayPickComponent::RayHitsPiece(Origin, FVector3f::ForwardVector, Piece, FQuat4f::Identity, HalfExtents, BoundRadius, Best));
		TestTrue(TEXT("entry distance is the near face"), FMath::IsNearlyEqual(Best, 80.f, 0.01f));
	}

	// Rotated a quarter turn about Z, the slab is 2.5 cm along the ray and 40 cm across it: enters at 98.75 cm.
	{
		float Best = 1000.f;
		const FQuat4f Quarter(FVector3f::UpVector, HALF_PI);
		TestTrue(TEXT("rotated hit"), UHayPickComponent::RayHitsPiece(Origin, FVector3f::ForwardVector, Piece, Quarter, HalfExtents, BoundRadius, Best));
		TestTrue(TEXT("rotated entry distance"), FMath::IsNearlyEqual(Best, 98.75f, 0.01f));
	}

	// A ray passing 2 cm beside the slab misses it, even though it is inside the bounding sphere.
	{
		float Best = 1000.f;
		TestFalse(TEXT("miss beside the slab"), UHayPickComponent::RayHitsPiece(FVector3f(0.f, 3.25f, 0.f), FVector3f::ForwardVector, Piece, FQuat4f::Identity, HalfExtents, BoundRadius, Best));
		TestEqual(TEXT("miss leaves the best distance alone"), Best, 1000.f);
	}

	// Pointing away.
	{
		float Best = 1000.f;
		TestFalse(TEXT("behind the origin"), UHayPickComponent::RayHitsPiece(Origin, FVector3f::BackwardVector, Piece, FQuat4f::Identity, HalfExtents, BoundRadius, Best));
	}

	// A closer hit already found: this piece is farther, so it is rejected.
	{
		float Best = 50.f;
		TestFalse(TEXT("farther than the best hit"), UHayPickComponent::RayHitsPiece(Origin, FVector3f::ForwardVector, Piece, FQuat4f::Identity, HalfExtents, BoundRadius, Best));
		TestEqual(TEXT("best distance unchanged"), Best, 50.f);
	}

	return true;
}

#endif
