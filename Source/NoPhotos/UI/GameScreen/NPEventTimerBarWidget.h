#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPEventTimerBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class NOPHOTOS_API UNPEventTimerBarWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Map Event|Timer")
	void StartEventTimer(float InEndServerWorldTime, float InTotalDurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Map Event|Timer")
	void StartFallbackEventTimer();

	UFUNCTION(BlueprintCallable, Category = "Map Event|Timer")
	void StopEventTimer();

	UFUNCTION(BlueprintPure, Category = "Map Event|Timer")
	bool IsEventTimerRunning() const { return bIsTimerRunning; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Timer", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "s"))
	float DefaultEventDurationSeconds = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Event|Timer", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float UpdateIntervalSeconds = 0.05f;


private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> LeftTimeBar;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> LeftTimeText;

	float EventEndServerWorldTime = 0.0f;
	float TotalDurationSeconds = 0.0f;
	float TimeUntilNextUpdate = 0.0f;
	bool bIsTimerRunning = false;

	float GetServerWorldTimeSeconds() const;
	void UpdateTimerDisplay();
};
