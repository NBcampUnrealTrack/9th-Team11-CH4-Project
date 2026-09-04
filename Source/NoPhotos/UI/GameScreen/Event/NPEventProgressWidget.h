//게임 진행도 & 예정 이벤트 타이밍 표시하는 위젯
#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "UI/NPUserWidget.h"
#include "NPEventProgressWidget.generated.h"

class ANPMainGameState;
class UHorizontalBox;
class UProgressBar;
class UTexture2D;


UCLASS()
class NOPHOTOS_API UNPEventProgressWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	UNPEventProgressWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> GameProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> EventMarkerBox;

	//마크랑 이벤트 타이밍 맞춘다고 이것저것 좀 추가했습니다...
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	TObjectPtr<UTexture2D> EventMarkerImage;
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress", meta = (ClampMin = "1.0", UIMin = "1.0"))
	FVector2D EventMarkerImageSize = FVector2D(48.0f, 48.0f);
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	float EventMarkerHorizontalPadding = -15.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	float EventMarkerTriggerOffsetPixels = 8.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	FText DefaultMarkerText = FText::FromString(TEXT("●"));

	UPROPERTY(EditDefaultsOnly, Category = "Event Progress", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float GameDurationSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Event Progress|Warning")
	FLinearColor NormalProgressColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress|Warning")
	FLinearColor NearEventProgressColor = FLinearColor::Red;

	UPROPERTY(EditDefaultsOnly, Category = "Event Progress|Warning", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float EventWarningLeadTimeSeconds = 15.0f;

	FTimerHandle GameStateBindRetryTimerHandle;
	TWeakObjectPtr<ANPMainGameState> BoundMainGameState;
	TWeakObjectPtr<UNPMapEventManagerComponent> BoundEventManager;
	float ObservedGameDurationSeconds = 0.0f;
	float LastMarkerBoxWidth = -1.0f;

	UFUNCTION()
	void HandleMainGameStateChanged();

	UFUNCTION()
	void HandleEventScheduleChanged();

	bool TryBindToGameState();
	void RetryBindToGameState();
	void UnbindFromGameState();
	void UpdateProgressBar();
	float GetNextEventWarningAlpha() const;
	void RebuildEventMarkers();
	float GetResolvedGameDurationSeconds() const;
	void AddFillSpacer(float FillWeight);
};
