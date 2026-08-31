#include "NPSantaFlightRoute.h"

#include "Components/SceneComponent.h"
#include "NPSantaFlightTypes.h"
#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#include "Components/SplineComponent.h"
#endif

ANPSantaFlightRoute::ANPSantaFlightRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RouteRoot")));
	RouteGroup = FGameplayTag::RequestGameplayTag(TEXT("Santa"), false);
#if WITH_EDITORONLY_DATA
	PathPreview = CreateEditorOnlyDefaultSubobject<USplineComponent>(TEXT("PathPreview"));
	DirectionPreview = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("DirectionPreview"));
	if (PathPreview)
	{
		PathPreview->SetupAttachment(GetRootComponent());
		PathPreview->SetClosedLoop(false);
		PathPreview->SetDrawDebug(true);
		PathPreview->SetHiddenInGame(true);
	}
	if (DirectionPreview)
	{
		DirectionPreview->SetupAttachment(GetRootComponent());
		DirectionPreview->SetHiddenInGame(true);
		DirectionPreview->ArrowColor = FColor::Red;
		DirectionPreview->ArrowSize = 3.0f;
	}
#endif
}

bool ANPSantaFlightRoute::SupportsRouteGroup(FGameplayTag Group) const
{
	return Group.IsValid() && RouteGroup == Group;
}

bool ANPSantaFlightRoute::GetFlightEndpoints(FVector& OutStart, FVector& OutEnd) const
{
	return NPSantaFlight::BuildStraightPath(GetActorTransform(), FlightHeight, FlightDistance, OutStart, OutEnd);
}

void ANPSantaFlightRoute::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITORONLY_DATA
	FVector Start, End;
	const bool bValidPath = GetFlightEndpoints(Start, End);
	if (PathPreview)
	{
		PathPreview->ClearSplinePoints(false);
		if (bValidPath)
		{
			PathPreview->AddSplinePoint(Start, ESplineCoordinateSpace::World, false);
			PathPreview->AddSplinePoint(End, ESplineCoordinateSpace::World, false);
			PathPreview->SetSplinePointType(0, ESplinePointType::Linear, false);
			PathPreview->SetSplinePointType(1, ESplinePointType::Linear, false);
		}
		PathPreview->UpdateSpline();
	}
	if (DirectionPreview)
	{
		DirectionPreview->SetVisibility(bValidPath);
		if (bValidPath)
		{
			DirectionPreview->SetWorldLocationAndRotation((Start + End) * 0.5, (End - Start).Rotation());
			DirectionPreview->SetWorldScale3D(FVector::OneVector);
		}
	}
#endif
}
