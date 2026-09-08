#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPAimableRelicVisualProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USceneComponent;
struct FTimerHandle;

/**
 * 조준 유물의 서버 Trace 결과를 시각적으로 보여주기 위한 로컬 전용 투사체입니다.
 * 충돌과 게임 판정은 수행하지 않으며 네트워크로 직접 복제되지 않습니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPAimableRelicVisualProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANPAimableRelicVisualProjectile();

	/** 총구에서 Trace 종료점까지 날아가도록 로컬 투사체를 초기화합니다. */
	void InitializeVisualProjectile(
		const FVector& StartLocation,
		const FVector& EndLocation);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNiagaraComponent> TrailEffectComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** BP 자식에서 받아온 Trail Niagara System을 지정합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Projectile|Effect")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	/** Niagara 에셋의 기본 진행축이 +X가 아닐 때 보정할 회전입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Projectile|Effect")
	FRotator TrailRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Projectile|Effect",
		meta=(ClampMin="0.0"))
	float TrailScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Projectile|Movement",
		meta=(ClampMin="1.0", Units="cm/s"))
	float TravelSpeed = 12000.0f;

	/** 너무 가까운 사격도 최소한 이 시간 동안 보이도록 실제 속도를 조절합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Projectile|Movement",
		meta=(ClampMin="0.0", Units="s"))
	float MinimumVisibleDuration = 0.03f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Projectile|Movement",
		meta=(ClampMin="0.01", Units="s"))
	float MaximumLifeTime = 1.0f;

	/** 목표 지점에서 이동을 멈춘 뒤 시각 효과를 유지할 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Projectile|Movement",
		meta=(ClampMin="0.0", Units="s"))
	float PostArrivalLifeTime = 3.0f;

private:
	void HandleReachedDestination();

	FVector DestinationLocation = FVector::ZeroVector;
	FTimerHandle ArrivalTimer;
};
