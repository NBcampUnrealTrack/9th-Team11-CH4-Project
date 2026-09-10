#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "UI/GameScreen/NPScoreFeedbackWidget.h"
#include "NPScoreFeedbackWidgetComponent.generated.h"

/** 서버가 확정한 점수 피드백을 모든 클라이언트의 머리 위에 표시합니다. */
UCLASS(Blueprintable, ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPScoreFeedbackWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNPScoreFeedbackWidgetComponent();

	/** 서버에서 호출하면 현재 관련된 모든 클라이언트에 피드백을 표시합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Score Feedback")
	void ShowScoreFeedback(
		int32 Amount,
		ENPScoreFeedbackType FeedbackType,
		float DurationSeconds = 2.0f);

	/** 유물 반환 점수와 개인 미션 보너스를 머리 위에 함께 표시합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Score Feedback")
	void ShowRelicReturnFeedback(
		int32 ReturnScore,
		int32 MissionBonusScore,
		float DurationSeconds = 2.0f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CreateMissionBonusWidgetComponent();
	UNPScoreFeedbackWidget* ResolveMissionBonusWidget();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowScoreFeedback(
		int32 Amount,
		ENPScoreFeedbackType FeedbackType,
		float DurationSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowRelicReturnFeedback(
		int32 ReturnScore,
		int32 MissionBonusScore,
		float DurationSeconds);

	void ShowScoreFeedbackLocally(
		int32 Amount,
		ENPScoreFeedbackType FeedbackType,
		float DurationSeconds);
	void ShowRelicReturnFeedbackLocally(
		int32 ReturnScore,
		int32 MissionBonusScore,
		float DurationSeconds);
	UNPScoreFeedbackWidget* ResolveFeedbackWidget();
	void BeginDisplayingFeedback(float DurationSeconds);
	void HideFeedback();
	void UpdateFacingCamera();

	FTimerHandle HideTimer;

	/** 기존 반환 점수 위에 표시할 개인 미션 월드 위젯의 상대 위치입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Feedback|Mission Bonus",
		meta=(AllowPrivateAccess="true"))
	FVector MissionBonusRelativeLocation = FVector(0.0f, 0.0f, 30.0f);

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> MissionBonusWidgetComponent;
};
