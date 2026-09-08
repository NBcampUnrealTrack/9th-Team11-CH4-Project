#include "NPGhostPatrolRoute.h"

#include "Components/SplineComponent.h"

ANPGhostPatrolRoute::ANPGhostPatrolRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	PatrolSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PatrolSpline"));
	SetRootComponent(PatrolSpline);
	PatrolSpline->SetClosedLoop(false);
	PatrolSpline->SetDrawDebug(true);

	RouteGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("Possession")), false);
}

bool ANPGhostPatrolRoute::SupportsRouteGroup(const FGameplayTag InRouteGroup) const
{
	return InRouteGroup.IsValid() && RouteGroup == InRouteGroup;
}

bool ANPGhostPatrolRoute::IsUsableRoute() const
{
	return IsValid(PatrolSpline)
		&& PatrolSpline->GetNumberOfSplinePoints() >= 2
		&& PatrolSpline->GetSplineLength() > KINDA_SMALL_NUMBER;
}

FVector ANPGhostPatrolRoute::GetWorldLocationAtDistance(const float Distance) const
{
	if (!IsUsableRoute())
	{
		return GetActorLocation();
	}

	return PatrolSpline->GetLocationAtDistanceAlongSpline(
		FMath::Clamp(Distance, 0.0f, PatrolSpline->GetSplineLength()),
		ESplineCoordinateSpace::World);
}

FVector ANPGhostPatrolRoute::GetWorldDirectionAtDistance(const float Distance) const
{
	if (!IsUsableRoute())
	{
		return GetActorForwardVector();
	}

	return PatrolSpline->GetDirectionAtDistanceAlongSpline(
		FMath::Clamp(Distance, 0.0f, PatrolSpline->GetSplineLength()),
		ESplineCoordinateSpace::World);
}
