#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NPControlReversalComponent.generated.h"

class UAbilitySystemComponent;

/** GAS 태그를 관찰하여 수평면 이동(W/S, A/D)을 반전합니다. 상태는 ASC가 복제합니다. */
UCLASS(ClassGroup=(Input), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPControlReversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPControlReversalComponent();
	void ApplyRawMovementInput(const FVector& WorldMoveInput, float InputViewYaw);

	/** Horizontal은 수평면 전체를 뜻하며 전후/좌우 이동 모두 포함합니다. */
	UFUNCTION(BlueprintPure, Category="Control Reversal")
	bool IsHorizontalReversed() const { return bHorizontalReversed; }

	/** 원본 입력만 전달해야 합니다. 반환값을 RPC로 보내거나 다시 변환하지 않습니다. */
	static FVector ResolveMovementInput(const FVector& WorldMoveInput, float InputViewYaw, bool bReverseHorizontal);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleControlTagChanged(FGameplayTag Tag, int32 NewCount);
	void ApplyResolvedInput();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle ControlTagHandle;
	FVector RawMovementInput = FVector::ZeroVector;
	float RawInputViewYaw = 0.0f;
	bool bHorizontalReversed = false;
	bool bHasRawInput = false;
};
