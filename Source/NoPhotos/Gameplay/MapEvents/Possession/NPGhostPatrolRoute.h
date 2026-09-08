#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NPGhostPatrolRoute.generated.h"

class USplineComponent;

/** 레벨에 배치해 빙의 유령의 왕복 순찰 경로를 지정하는 Spline 액터입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPGhostPatrolRoute : public AActor
{
	GENERATED_BODY()

public:
	ANPGhostPatrolRoute();

	UFUNCTION(BlueprintPure, Category="Ghost Patrol Route")
	USplineComponent* GetSpline() const { return PatrolSpline; }

	UFUNCTION(BlueprintPure, Category="Ghost Patrol Route")
	bool SupportsRouteGroup(FGameplayTag InRouteGroup) const;

	/** 열린/닫힌 상태와 관계없이 두 점 이상이고 길이가 있으면 사용할 수 있습니다. */
	UFUNCTION(BlueprintPure, Category="Ghost Patrol Route")
	bool IsUsableRoute() const;

	FVector GetWorldLocationAtDistance(float Distance) const;
	FVector GetWorldDirectionAtDistance(float Distance) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ghost Patrol Route")
	TObjectPtr<USplineComponent> PatrolSpline;

	/** 같은 그룹을 사용하는 빙의 이벤트만 이 루트를 선택합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ghost Patrol Route")
	FGameplayTag RouteGroup;
};
