#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "NPTimedBomb.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraSystem;
class USoundBase;

/** 생성 후 5초에 폭발하는 임시 유물. 산타 선물 후보 또는 별도 SpawnActor로 사용합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPTimedBomb : public ANPBaseRelic, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ANPTimedBomb(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Timed Bomb")
	bool HasExploded() const { return bHasExploded; }

	UFUNCTION(BlueprintPure, Category="Timed Bomb")
	float GetRemainingFuseTime() const;

	/** 구형 범위 내 수평 방사 방향 + 위쪽 속도. 중심에서는 위쪽으로만 밀어냅니다. */
	static FVector CalculateBlastVelocity(const FVector& Origin, const FVector& Target,
		float Radius, float HorizontalStrength, float UpwardStrength);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 연출 전용. 넉백과 폭발 판정은 서버 C++에서 한 번만 처리합니다. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Timed Bomb|Presentation")
	void OnBombExploded(FVector ExplosionLocation);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Timed Bomb|GAS")
	TObjectPtr<UAbilitySystemComponent> ExplosionAbilitySystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb", meta=(ClampMin="0.1", Units="s"))
	float FuseDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|Explosion", meta=(ClampMin="0.0", Units="cm"))
	float ExplosionRadius = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|Explosion", meta=(ClampMin="0.0", Units="cm/s"))
	float HorizontalKnockbackStrength = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|Explosion", meta=(ClampMin="0.0", Units="cm/s"))
	float UpwardKnockbackStrength = 600.0f;

	/** Visibility를 막는 벽/엄폐물 뒤의 대상은 제외합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|Explosion")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|GAS")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|Presentation")
	TObjectPtr<UNiagaraSystem> ExplosionSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|Presentation")
	TObjectPtr<USoundBase> ExplosionSound;

	/** 임시 확인용 폭발 범위. 디버그 드로잉이 활성화된 빌드에서만 표시합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Timed Bomb|Presentation")
	bool bDrawDebugExplosion = true;

private:
	void Explode();
	void ApplyBlastKnockback(const FVector& Origin);
	void ApplyExplodedState();

	UFUNCTION()
	void OnRep_HasExploded();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosion(FVector_NetQuantize ExplosionLocation);

	UPROPERTY(ReplicatedUsing=OnRep_HasExploded)
	bool bHasExploded = false;

	UPROPERTY(Replicated)
	float ExplosionServerTime = 0.0f;

	FTimerHandle FuseTimer;
	bool bExplosionPresentationPlayed = false;
};
