#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "NPShowRelicLocationComponent.generated.h"

class UTextBlock;
class UNPPlayerBonusQuestComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnQuestRelicReturned);

// 로컬 플레이어에게 배정된 퀘스트 유물 위에 색상·거리 마커를 표시
UCLASS(Blueprintable, ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPShowRelicLocationComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNPShowRelicLocationComponent();

	//색상 선택
	static FLinearColor GetQuestRelicColor(int32 RelicIndex);

	UPROPERTY(BlueprintAssignable, Category="Relic Location")
	FNPOnQuestRelicReturned OnRelicReturned;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool GetAssignedRelicIndex(int32& OutRelicIndex) const;
	void RefreshMarker();
	void SetMarkerVisibility(bool bShouldBeVisible);

	TWeakObjectPtr<UNPPlayerBonusQuestComponent> BoundBonusQuestComponent;
	TWeakObjectPtr<UTextBlock> PointText;
	TWeakObjectPtr<UTextBlock> LeftDistanceText;
	FVector RelicLocationOffset = FVector::ZeroVector;
	bool bWasReturned = false;
};
