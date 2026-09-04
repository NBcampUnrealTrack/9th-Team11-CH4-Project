// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "UI/NPUserWidget.h"
#include "NPEventProgressWidget.generated.h"

class ANPMainGameState;
class UHorizontalBox;
class UProgressBar;
class UTexture2D;

/** 게임 진행도와 예정 이벤트 위치를 함께 표시합니다. */
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
	/** 게임 전체 시간에 대한 현재 진행도를 표시하는 ProgressBar입니다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> GameProgressBar;

	/** 예정 이벤트 마커와 비율 Spacer를 담는 HorizontalBox입니다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> EventMarkerBox;

	/** 예정 이벤트 위치에 표시할 이미지입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	TObjectPtr<UTexture2D> EventMarkerImage;

	/** 이벤트 마커 이미지의 레이아웃 크기입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress", meta = (ClampMin = "1.0", UIMin = "1.0"))
	FVector2D EventMarkerImageSize = FVector2D(48.0f, 48.0f);

	/** 마커 이미지 중심을 이벤트 시점에 맞추기 위해 슬롯 양쪽에 적용할 수평 패딩입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	float EventMarkerHorizontalPadding = -15.0f;

	/** 실제 이벤트 시점보다 마커를 오른쪽으로 옮길 픽셀 값입니다. 양수면 바가 마커에 진입한 뒤 이벤트가 시작되는 인상을 줍니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	float EventMarkerTriggerOffsetPixels = 8.0f;

	/** EventMarkerImage가 없을 때 사용할 기본 텍스트 마커입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress")
	FText DefaultMarkerText = FText::FromString(TEXT("●"));

	/** 0이면 매치 시작 시 관측한 남은 시간을 전체 시간으로 사용합니다. 늦은 참가도 정확히 표시하려면 게임 모드의 제한 시간과 같은 값으로 설정하세요. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float GameDurationSeconds = 0.0f;

	/** 다음 예정 이벤트가 가까워지기 전까지 유지할 프로그레스 바 색입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress|Warning")
	FLinearColor NormalProgressColor = FLinearColor::White;

	/** 다음 예정 이벤트가 시작될 때 도달할 프로그레스 바 색입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Event Progress|Warning")
	FLinearColor NearEventProgressColor = FLinearColor::Red;

	/** 다음 이벤트 시작 전, 기본색에서 경고색으로 변하기 시작하는 시간입니다. */
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
