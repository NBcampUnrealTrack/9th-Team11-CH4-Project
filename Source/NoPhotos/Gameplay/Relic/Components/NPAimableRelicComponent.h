#pragma once

#include "GameplayTagContainer.h"
#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "NPAimableRelicComponent.generated.h"

class ANPReplicatedStablePhysicsPawn;
class ANPAimableRelicVisualProjectile;
class UAbilitySystemComponent;
class UGameplayEffect;
class UPrimitiveComponent;
class USceneComponent;
class UWorld;

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPRelicAimSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim", meta=(ClampMin="1.0", Units="cm"))
	float MaximumRange = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire", meta=(ClampMin="0.0", Units="s"))
	float FireInterval = 1.0f;

	/** 클라이언트가 보낸 카메라가 Pawn 원점에서 떨어질 수 있는 최대 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Security", meta=(ClampMin="0.0", Units="cm"))
	float MaximumCameraDistanceFromPawn = 700.0f;

	/** 클라이언트 카메라 방향과 서버 시선 사이의 최대 허용 각도입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Security", meta=(ClampMin="0.0", ClampMax="180.0", Units="deg"))
	float MaximumCameraDirectionError = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockback", meta=(ClampMin="0.0", Units="cm/s"))
	float HorizontalKnockbackStrength = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockback", meta=(ClampMin="0.0", Units="cm/s"))
	float UpwardKnockbackStrength = 350.0f;

	/** 서버 Trace가 내구도 컴포넌트에 전달할 충격량입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact", meta=(ClampMin="0.0"))
	float DurabilityImpactStrength = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 정확한 Line Trace가 빗나갔을 때 플레이어를 보정 탐색하는 반지름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace", meta=(ClampMin="0.0", Units="cm"))
	float AimAssistRadius = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockback")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

};

/** 잡은 플레이어에게 조준/발사 GAS Ability를 제공하는 유물 컴포넌트입니다. */
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPAimableRelicComponent : public UNPUsableRelicComponent
{
	GENERATED_BODY()

public:
	UNPAimableRelicComponent();

	const FNPRelicAimSettings& GetAimSettings() const { return AimSettings; }
	FTransform GetMuzzleWorldTransform() const { return GetMuzzleTransform(); }
	void SetMuzzleSourceComponent(USceneComponent* InMuzzleSourceComponent);

	/** 서버에서 발사 간격을 검증하고 Line Trace 및 넉백 Effect를 적용합니다. */
	virtual bool TryFire(
		ANPReplicatedStablePhysicsPawn* ShooterPawn,
		UAbilitySystemComponent* SourceAbilitySystem,
		const FVector& CameraLocation,
		const FVector& CameraForward);

	/** 플레이어 입력 없이 총구의 월드 방향으로 발사합니다. 서버 권한 액터가 호출해야 합니다. */
	bool TryFireFromWorldDirection(
		AActor* SourceActor,
		UAbilitySystemComponent* SourceAbilitySystem,
		const FVector& FireDirection);

	/** 이동 중인 명시적 대상의 현재 위치를 향해 발사합니다. 중간 장애물은 그대로 피격됩니다. */
	bool TryFireAtTarget(
		AActor* SourceActor,
		UAbilitySystemComponent* SourceAbilitySystem,
		AActor* TargetActor,
		UPrimitiveComponent* TargetComponent,
		const FVector& TargetLocation);

protected:
	bool TryConsumeFireCooldown();
	FTransform GetMuzzleTransform() const;

	/** 발사 순간 실행할 Gameplay Cue입니다. 파생 유물 또는 Blueprint에서 교체할 수 있습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aimable Relic|Effects")
	FGameplayTag FireGameplayCueTag;

	/** 서버 Trace의 총구 시작점과 종료점을 각 클라이언트의 로컬 장식 투사체로 재생합니다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnVisualProjectile(
		FVector_NetQuantize10 StartLocation,
		FVector_NetQuantize10 EndLocation);

private:
	bool TryFireInternal(
		AActor* SourceActor,
		ANPReplicatedStablePhysicsPawn* AimAssistSourcePawn,
		UAbilitySystemComponent* SourceAbilitySystem,
		const FVector& TraceStart,
		const FVector& FireDirection,
		bool bAllowAimAssist,
		AActor* ExplicitTargetActor,
		UPrimitiveComponent* ExplicitTargetComponent,
		const FVector& ExplicitTargetLocation);

	bool TryFindAssistedPlayer(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		ANPReplicatedStablePhysicsPawn* ShooterPawn,
		AActor* Relic,
		FHitResult& OutHit) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aimable Relic", meta=(AllowPrivateAccess="true"))
	FNPRelicAimSettings AimSettings;

	/** RelicMesh에 생성한 총구 Socket 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aimable Relic|Effects", meta=(AllowPrivateAccess="true"))
	FName MuzzleSocketName = TEXT("Muzzle");

	/** BP 조준 유물에서 Niagara Trail을 설정한 로컬 장식 투사체 클래스를 지정합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aimable Relic|Effects", meta=(AllowPrivateAccess="true"))
	TSubclassOf<ANPAimableRelicVisualProjectile> VisualProjectileClass;

	TWeakObjectPtr<USceneComponent> MuzzleSourceComponent;

	double LastServerFireTime = -TNumericLimits<double>::Max();
};
