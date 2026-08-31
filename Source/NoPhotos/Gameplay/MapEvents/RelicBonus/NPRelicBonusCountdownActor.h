#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPRelicBonusCountdownActor.generated.h"

class FLifetimeProperty;
class UNPRelicBonusCountdownWidget;
class USceneComponent;
class UWidgetComponent;

/** 반환 존 위에 남은 이벤트 시간을 표시하는 복제 월드 UI 액터입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPRelicBonusCountdownActor : public AActor
{
	GENERATED_BODY()

public:
	ANPRelicBonusCountdownActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Relic Bonus Countdown")
	void SetCountdownDuration(float DurationSeconds);

	/** 이벤트 액터와 완전히 동일한 서버 종료 시각을 사용합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Relic Bonus Countdown")
	void SetCountdownEndServerWorldTime(float InEndServerWorldTime);

	UFUNCTION(BlueprintPure, Category="Relic Bonus Countdown")
	UWidgetComponent* GetCountdownWidgetComponent() const { return CountdownWidgetComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UWidgetComponent> CountdownWidgetComponent;

private:
	UFUNCTION()
	void OnRep_EndServerWorldTime();

	void UpdateCountdown();
	void FaceLocalPlayerCamera();
	float GetSynchronizedWorldTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_EndServerWorldTime)
	float EndServerWorldTime = 0.0f;

	int32 LastDisplayedSeconds = INDEX_NONE;
};
