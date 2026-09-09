//게임 진행도 & 예정 이벤트 타이밍 표시하는 위젯
#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "UI/NPUserWidget.h"
#include "NPEventProgressWidget.generated.h"

class ANPMainGameState;
class UHorizontalBox;
class UProgressBar;
class USizeBox;
class UUserWidget;
class UWidget;
class UWidgetAnimation;


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

	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	TSubclassOf<UUserWidget> EventMarkerWidgetClass;
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

	struct FEventMarkerRuntime
	{
		TWeakObjectPtr<UUserWidget> Widget;
		TWeakObjectPtr<USizeBox> SizeBox;
		ENPScheduledMapEventState State = ENPScheduledMapEventState::Pending;
	};

	TMap<int32, FEventMarkerRuntime> EventMarkers;

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
	FEventMarkerRuntime* CreateEventMarker(int32 ScheduleIndex, const FText& Title);
	void SetEventMarkerState(FEventMarkerRuntime& Marker, ENPScheduledMapEventState NewState, bool bPlayTransition);
	static UWidget* FindMarkerWidget(const UUserWidget* MarkerWidget, FName WidgetName);
	static UWidgetAnimation* FindMarkerAnimation(const UUserWidget* MarkerWidget, FName AnimationName);
};
