#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "NPPhotoPenaltyWidgetComponent.generated.h"

class UNPPhotoPenaltyWidget;

/** 로컬 소유자를 포함한 모든 클라이언트에서 머리 위 가격 감점 UI를 잠시 표시합니다. */
UCLASS(Blueprintable, ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPhotoPenaltyWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNPPhotoPenaltyWidgetComponent();

	UFUNCTION(BlueprintCallable, Category="Photo Penalty")
	void ShowPenalty(int32 AppliedPhotoPenalty, float DurationSeconds = 2.0f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void HidePenalty();
	void UpdateFacingCamera();

	FTimerHandle HideTimer;
};
