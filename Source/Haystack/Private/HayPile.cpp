// Copyright (c) 2026 Vanan Andreas.

#include "HayPile.h"
#include "HayPile/HayLayoutComponent.h"
#include "HayPile/HayPickComponent.h"
#include "HayPile/HayPieceStateComponent.h"
#include "HayPile/HayRenderComponent.h"

#include "Components/SphereComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayPile)

AHayPile::AHayPile()
{
	PrimaryActorTick.bCanEverTick = false;

	Dome = CreateDefaultSubobject<USphereComponent>(TEXT("Root"));
	Dome->SetMobility(EComponentMobility::Static);
	Dome->SetCollisionProfileName(TEXT("InvisibleWall"));
	Dome->ShapeColor = FColor(255, 190, 40);
	Dome->bDrawOnlyIfSelected = false;
	RootComponent = Dome;

	Layout = CreateDefaultSubobject<UHayLayoutComponent>(TEXT("Layout"));
	Render = CreateDefaultSubobject<UHayRenderComponent>(TEXT("Render"));
	PieceState = CreateDefaultSubobject<UHayPieceStateComponent>(TEXT("PieceState"));
	Pick = CreateDefaultSubobject<UHayPickComponent>(TEXT("Pick"));
}

void AHayPile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Dome->SetSphereRadius(Layout->DomeRadius);
}

void AHayPile::BeginPlay()
{
	Super::BeginPlay();

	// Order matters: pick subscribes to cell spawns, and render starts spawning on its first tick.
	Layout->Build();
	Render->Initialize();
	PieceState->Initialize();
	Pick->Initialize();
}