#pragma once

#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "NPAimableRelicComponent.generated.h"

class ANPReplicatedStablePhysicsPawn;
class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraSystem;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 정확한 Line Trace가 빗나갔을 때 플레이어를 보정 탐색하는 반지름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace", meta=(ClampMin="0.0", Units="cm"))
	float AimAssistRadius = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockback")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Debug")
	bool bDrawDebugTrace = true;
};

/** 잡은 플레이어에게 조준/발사 GAS Ability를 제공하는 유물 컴포넌트입니다. */
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPAimableRelicComponent : public UNPUsableRelicComponent
{
	GENERATED_BODY()

public:
	UNPAimableRelicComponent();

	const FNPRelicAimSettings& GetAimSettings() const { return AimSettings; }

	/** 서버에서 발사 간격을 검증하고 Line Trace 및 넉백 Effect를 적용합니다. */
	bool TryFire(
		ANPReplicatedStablePhysicsPawn* ShooterPawn,
		UAbilitySystemComponent* SourceAbilitySystem,
		const FVector& CameraLocation,
		const FVector& CameraForward);

protected:
	/** 서버가 승인한 발사를 모든 클라이언트의 총구에서 재생합니다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMuzzleEffect();

private:
	bool TryFindAssistedPlayer(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		ANPReplicatedStablePhysicsPawn* ShooterPawn,
		AActor* Relic,
		FHitResult& OutHit) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aimable Relic", meta=(AllowPrivateAccess="true"))
	FNPRelicAimSettings AimSettings;

	/** BP 조준 유물에서 할당할 단발성 총구 Niagara System입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aimable Relic|Effects", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNiagaraSystem> MuzzleEffect;

	/** RelicMesh에 생성한 총구 Socket 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aimable Relic|Effects", meta=(AllowPrivateAccess="true"))
	FName MuzzleSocketName = TEXT("Muzzle");

	double LastServerFireTime = -TNumericLimits<double>::Max();
};
