#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPTrapKnockbackComponent.generated.h"

class APawn;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNPTrapKnockbackHitSignature,
	APawn*, TargetPawn,
	FVector, HitLocation);

/** 서버 권한으로 함정 충돌 대상을 검증하고 기존 GAS 넉백 Effect를 적용합니다. */
UCLASS(ClassGroup=(Trap), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPTrapKnockbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPTrapKnockbackComponent();

	/** 서버에서 넉백이 실제 적용된 뒤 모든 클라이언트에서 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category="Trap|Knockback|Presentation")
	FNPTrapKnockbackHitSignature OnKnockbackHit;

	/** 새 함정 활성 주기를 시작하고 주기별 피격 기록을 초기화합니다. */
	void BeginActivationCycle(int32 CycleSequence);
	void EndActivationCycle();
	void SetHitOncePerActivation(bool bEnabled)
	{
		bHitOncePerActivation = bEnabled;
	}

	/** 충돌한 Actor가 유효한 Pawn인지 검사한 뒤 전달받은 월드 방향으로 넉백합니다. */
	bool TryApplyKnockback(
		AActor* OtherActor,
		const FHitResult& Hit,
		const FVector& WorldKnockbackDirection);

	UFUNCTION(BlueprintPure, Category="Trap|Knockback")
	bool IsActivationCycleActive() const { return bActivationCycleActive; }

	UFUNCTION(BlueprintPure, Category="Trap|Knockback")
	int32 GetActiveCycleSequence() const { return ActiveCycleSequence; }

protected:
	/** 기본값은 프로젝트의 UNPKnockbackGameplayEffect입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap|Knockback")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	/** 전달받은 수평 방향으로 가하는 속도 변화입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap|Knockback",
		meta=(ClampMin="0.0", Units="cm/s"))
	float ForwardKnockbackStrength = 1500.0f;

	/** 계단이나 턱에 걸리지 않도록 함께 가하는 월드 Up 방향 속도 변화입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap|Knockback",
		meta=(ClampMin="0.0", Units="cm/s"))
	float UpwardKnockbackStrength = 250.0f;

	/** 같은 활성 주기 안에서는 같은 Pawn을 한 번만 타격합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap|Knockback")
	bool bHitOncePerActivation = true;

	/** 반복 타격을 허용할 때 같은 Pawn에게 다시 적용할 수 있는 최소 간격입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap|Knockback",
		meta=(ClampMin="0.0", Units="s"))
	float PerTargetRehitCooldown = 0.5f;

private:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastNotifyKnockbackHit(
		APawn* TargetPawn,
		FVector_NetQuantize HitLocation);

	void RemoveInvalidTargetRecords();

	TSet<TWeakObjectPtr<APawn>> HitPawnsThisCycle;
	TMap<TWeakObjectPtr<APawn>, double> LastHitTimes;
	int32 ActiveCycleSequence = INDEX_NONE;
	bool bActivationCycleActive = false;
};
