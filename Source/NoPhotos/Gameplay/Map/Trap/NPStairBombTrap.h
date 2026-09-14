#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Gameplay/Map/Trap/NPStairTrapBase.h"
#include "NPStairBombTrap.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 * 지정된 투척 위치에서 목표 지점까지 폭탄을 포물선으로 날린 뒤,
 * Active 단계 진입 순간 서버에서 방사형 GAS 넉백을 적용하는 반복 계단 함정입니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPStairBombTrap
	: public ANPStairTrapBase
	, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ANPStairBombTrap();

	virtual void Tick(float DeltaSeconds) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleTrapStateChanged(
		ENPStairTrapState PreviousState,
		ENPStairTrapState NewState) override;

	/** ThrowerActor/Socket을 사용하지 못할 때 사용할 기본 투척 시작점입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap")
	TObjectPtr<USceneComponent> ThrowOriginComponent;

	/** ThrowTargetActor가 없을 때 사용할 기본 폭발 목표점입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap")
	TObjectPtr<USceneComponent> ThrowTargetComponent;

	/** 매 사이클 재사용되는 폭탄 외형입니다. 실제 충돌 판정에는 사용하지 않습니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap")
	TObjectPtr<UStaticMeshComponent> BombMeshComponent;

	/** 서버에서 넉백 Gameplay Effect를 발신하는 ASC입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|GAS")
	TObjectPtr<UAbilitySystemComponent> ExplosionAbilitySystem;

	/** 선택 사항. 지정하면 이 Actor의 위치 또는 Mesh Socket에서 폭탄을 시작합니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Stair Bomb Trap|Throw")
	TObjectPtr<AActor> ThrowerActor;

	/** ThrowerActor의 Mesh에 존재하면 이 Socket을 투척 시작점으로 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Throw")
	FName ThrowSocketName = NAME_None;

	/** 선택 사항. 지정하면 이 Actor의 위치에서 폭발합니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Stair Bomb Trap|Throw")
	TObjectPtr<AActor> ThrowTargetActor;

	/** Warning 진입부터 목표 위치 도착까지 걸리는 시간입니다. Controller WarningDuration과 맞추는 것을 권장합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Throw",
		meta=(ClampMin="0.01", Units="s"))
	float ThrowDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Throw",
		meta=(ClampMin="0.0", Units="cm"))
	float ThrowArcHeight = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Throw")
	bool bRotateDuringFlight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Throw",
		meta=(Units="deg/s"))
	FRotator FlightRotationSpeed = FRotator(360.0f, 180.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Explosion",
		meta=(ClampMin="0.0", Units="cm"))
	float ExplosionRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Explosion",
		meta=(ClampMin="0.0", Units="cm/s"))
	float HorizontalKnockbackStrength = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Explosion",
		meta=(ClampMin="0.0", Units="cm/s"))
	float UpwardKnockbackStrength = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|Explosion")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stair Bomb Trap|GAS")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stair Bomb Trap|Presentation")
	TObjectPtr<UNiagaraSystem> ExplosionSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stair Bomb Trap|Presentation")
	TObjectPtr<USoundBase> ExplosionSound;

	/** 투척 장치의 애니메이션, 준비음 같은 선택적 블루프린트 연출 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category="Stair Bomb Trap|Presentation", meta=(DisplayName="On Throw Started"))
	void BP_OnThrowStarted();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category="Stair Bomb Trap|Presentation", meta=(DisplayName="On Bomb Exploded"))
	void BP_OnBombExploded(FVector ExplosionLocation);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category="Stair Bomb Trap|Presentation", meta=(DisplayName="On Thrower Reset"))
	void BP_OnThrowerReset();

private:
	void BeginThrow();
	void UpdateBombPose();
	void ExplodeOnce();
	void ApplyBlastKnockback(const FVector& ExplosionLocation);
	void SetBombVisible(bool bVisible);
	FVector ResolveThrowStartLocation() const;
	FVector ResolveThrowTargetLocation() const;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayExplosion(
		FVector_NetQuantize ExplosionLocation,
		int32 CycleSequence);

	FVector ThrowStartLocation = FVector::ZeroVector;
	FVector ThrowTargetLocation = FVector::ZeroVector;
	FRotator ThrowStartRotation = FRotator::ZeroRotator;
	int32 LastExplodedCycleSequence = INDEX_NONE;
	int32 LastPresentedExplosionCycle = INDEX_NONE;
};
