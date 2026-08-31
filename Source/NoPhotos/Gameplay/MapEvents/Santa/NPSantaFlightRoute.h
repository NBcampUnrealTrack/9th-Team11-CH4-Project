#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NPSantaFlightRoute.generated.h"

class USplineComponent;
class UArrowComponent;

/** 위치=경로 중심, Yaw=비행 방향. NavMesh와 SpawnVolume을 사용하지 않는 직선 경로입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPSantaFlightRoute : public AActor
{
	GENERATED_BODY()

public:
	ANPSantaFlightRoute();
	virtual void OnConstruction(const FTransform& Transform) override;

	bool SupportsRouteGroup(FGameplayTag Group) const;
	float GetSelectionWeight() const { return SelectionWeight; }

	UFUNCTION(BlueprintPure, Category="Santa Route")
	bool GetFlightEndpoints(FVector& OutStart, FVector& OutEnd) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Route")
	FGameplayTag RouteGroup;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Route", meta=(ClampMin="0.0"))
	float SelectionWeight = 1.0f;

	/** 기준점에서 월드 위쪽으로 더하는 높이입니다. 지면 자동 탐색은 하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Route", meta=(ClampMin="0.0", Units="cm"))
	float FlightHeight = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Santa Route", meta=(ClampMin="1.0", Units="cm"))
	float FlightDistance = 12000.0f;

private:
#if WITH_EDITORONLY_DATA
	/** 두 점은 계산된 미리보기이며 사용자가 편집하는 곡선 경로가 아닙니다. */
	UPROPERTY()
	TObjectPtr<USplineComponent> PathPreview;

	UPROPERTY()
	TObjectPtr<UArrowComponent> DirectionPreview;
#endif
};
