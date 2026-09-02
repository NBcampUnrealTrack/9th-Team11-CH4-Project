//유물 주변에서 유물 바라볼 때 유물 위에 뜨는 미니 정보
#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "NPRelicHoverInfoComponent.generated.h"

UCLASS()
class NOPHOTOS_API UNPRelicHoverInfoComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNPRelicHoverInfoComponent();

	UFUNCTION(BlueprintCallable, Category="Relic Hover Info")
	void RefreshRelicInfo();

	//유물 정보 가시여부 함수
	UFUNCTION(BlueprintCallable, Category="Relic Hover Info")
	void SetHoverInfoVisible(bool bShouldBeVisible);

	void SetFocusedByLocalPlayer(bool bIsFocused);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void RefreshHoverVisibility();
	void UpdateFacingCamera();

	FVector RelicLocationOffset = FVector::ZeroVector;
	bool bVisibleByGameplay = true;
	bool bFocusedByLocalPlayer = false;
};
