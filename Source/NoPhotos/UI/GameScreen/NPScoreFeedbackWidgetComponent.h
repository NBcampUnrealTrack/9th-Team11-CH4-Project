#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "UI/GameScreen/NPScoreFeedbackWidget.h"
#include "NPScoreFeedbackWidgetComponent.generated.h"

/** 서버가 확정한 사진 감점과 유물 반환 보상을 모든 클라이언트의 머리 위에 표시합니다. */
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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowScoreFeedback(
		int32 Amount,
		ENPScoreFeedbackType FeedbackType,
		float DurationSeconds);

	void ShowScoreFeedbackLocally(
		int32 Amount,
		ENPScoreFeedbackType FeedbackType,
		float DurationSeconds);
	void HideFeedback();
	void UpdateFacingCamera();

	FTimerHandle HideTimer;
};
