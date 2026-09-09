#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/Gimmick/Components/NPRelicGimmickComponent.h"
#include "NPPullGimmickComponent.generated.h"

class UGrabbableComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPullSucceeded,
	int32,
	CurrentPullCount,
	int32,
	RequiredPullCount);

UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPullGimmickComponent : public UNPRelicGimmickComponent
{
	GENERATED_BODY()

public:
	UNPPullGimmickComponent();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(
		BlueprintCallable,
		Category="Pull Gimmick",
		meta=(ToolTip="로컬 Pull 연출이 끝났을 때 호출합니다. 서버의 기믹 완료 판정에는 영향을 주지 않습니다."))
	void NotifyPullFinished();

	UFUNCTION(BlueprintPure, Category="Pull Gimmick")
	bool IsPullPresentationPlaying() const { return bIsPullPresentationPlaying; }

	UFUNCTION(BlueprintPure, Category="Pull Gimmick")
	int32 GetCurrentPullCount() const { return CurrentPullCount; }

	UPROPERTY(
		BlueprintAssignable,
		Category="Pull Gimmick",
		meta=(ToolTip="서버가 Pull 성공을 확정하고 CurrentPullCount가 갱신될 때 서버와 각 클라이언트에서 발생합니다."))
	FOnPullSucceeded OnPullSucceeded;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_CurrentPullCount();

	void HandleGrabStarted(UPrimitiveComponent* GrabbedComponent);
	void HandleGrabForceUpdated(
		const FVector& LinearForce,
		const FVector& AngularForce,
		float IntentForceAlignment);
	void HandleGrabEnded();

	UPROPERTY(
		EditAnywhere,
		Category="Pull Gimmick",
		meta=(ToolTip="World Space 기준의 당김 방향입니다. (0, 0, 0)이면 모든 방향의 힘 크기로 판정합니다."))
	FVector PullDirection = FVector::UpVector;

	UPROPERTY(EditAnywhere, Category="Pull Gimmick", meta=(ClampMin="0.0"))
	float PullForceThreshold = 600000.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category="Pull Gimmick",
		meta=(ClampMin="0.0", ClampMax="1.0", AllowPrivateAccess="true"))
	float MinimumIntentAlignment = 0.7f;

	UPROPERTY(EditAnywhere, Category="Pull Gimmick", meta=(ClampMin="1"))
	int32 RequiredPullCount = 3;

	UPROPERTY(
		ReplicatedUsing=OnRep_CurrentPullCount,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category="Pull Gimmick",
		meta=(AllowPrivateAccess="true"))
	int32 CurrentPullCount = 0;

	UPROPERTY(VisibleInstanceOnly, Category="Pull Gimmick|Debug")
	int32 PullAttemptCount = 0;

	UPROPERTY(Transient)
	UGrabbableComponent* GrabbableComponent = nullptr;

	bool bPullForceExceeded = false;

	UPROPERTY(VisibleInstanceOnly, Category="Pull Gimmick|Debug")
	bool bIsPullPresentationPlaying = false;

	float CurrentAttemptMaxPullForce = 0.0f;
	float CurrentAttemptMaxLinearForce = 0.0f;
};
