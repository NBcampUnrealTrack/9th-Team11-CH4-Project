#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "NPStatusVisualManager.generated.h"

class ANPControlReversalGameplayCue;
class ANPPhotoStunGameplayCue;
class UChildActorComponent;

UENUM(BlueprintType)
enum class ENPStatusVisualType : uint8
{
	Leader UMETA(DisplayName="1등 트로피"),
	ControlReversal UMETA(DisplayName="조작 반전 유령"),
	PhotoStun UMETA(DisplayName="사진 스턴 별풍선")
};

/** 캐릭터 상태 연출의 등장과 퇴장 전환을 한 곳에서 관리합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPStatusVisualManager : public AActor
{
	GENERATED_BODY()

public:
	ANPStatusVisualManager();
	virtual void Tick(float DeltaSeconds) override;

	void InitializeLeaderCrown(UChildActorComponent* InLeaderCrown);
	void RequestControlReversalVisual(ANPControlReversalGameplayCue* Visual, bool bActive);
	void RequestPhotoStunVisual(ANPPhotoStunGameplayCue* Visual, bool bActive);

	UFUNCTION(BlueprintCallable, Category="Status Visual")
	void RequestVisual(ENPStatusVisualType VisualType, bool bActive);

protected:
	/** X축은 시간(초), Y축은 스케일 배율입니다. 기본 형태는 0에서 1입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status Visual|Transition")
	FRuntimeFloatCurve AppearCurve;

	/** X축은 시간(초), Y축은 스케일 배율입니다. 기본 형태는 1에서 0입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status Visual|Transition")
	FRuntimeFloatCurve DisappearCurve;

	UFUNCTION(BlueprintImplementableEvent, Category="Status Visual", meta=(DisplayName="On Request"))
	void OnRequest(ENPStatusVisualType VisualType, bool bActive);

private:
	struct FTransitionState
	{
		float CurrentScale = 0.0f;
		float StartScale = 0.0f;
		float TargetScale = 0.0f;
		float Elapsed = 0.0f;
		float MinTime = 0.0f;
		float Duration = 0.0f;
		float CurveStartValue = 0.0f;
		float CurveEndValue = 1.0f;
		bool bHasCurve = false;
		bool bRequestedActive = false;
		bool bTransitioning = false;
	};

	static int32 GetVisualIndex(ENPStatusVisualType VisualType);
	const FRuntimeFloatCurve& GetTransitionCurve(bool bAppearing) const;
	void ApplyVisualScale(ENPStatusVisualType VisualType, float ScaleMultiplier);
	void PrepareAppearance(ENPStatusVisualType VisualType);
	void FinishDisappear(ENPStatusVisualType VisualType);
	bool HasActiveTransition() const;

	FTransitionState Transitions[3];

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> LeaderCrown;

	TWeakObjectPtr<ANPControlReversalGameplayCue> ControlReversalVisual;
	TWeakObjectPtr<ANPPhotoStunGameplayCue> PhotoStunVisual;
};
