#pragma once

#include "Gameplay/Relic/Components/NPAimableRelicComponent.h"
#include "Gameplay/Relic/Projectile/NPFireballProjectile.h"
#include "NPFireballRelicComponent.generated.h"

UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPFireballRelicComponent
	: public UNPAimableRelicComponent
{
	GENERATED_BODY()

public:
	UNPFireballRelicComponent();

	virtual bool TryFire(
		ANPReplicatedStablePhysicsPawn* ShooterPawn,
		UAbilitySystemComponent* SourceAbilitySystem,
		const FVector& CameraLocation,
		const FVector& CameraForward) override;

private:
	void ExecuteFire(
		TWeakObjectPtr<ANPReplicatedStablePhysicsPawn> ShooterPawn,
		TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem,
		FVector CameraLocation,
		FVector CameraForward);

	void ApplyShooterRecoil(
		ANPReplicatedStablePhysicsPawn* ShooterPawn,
		UAbilitySystemComponent* SourceAbilitySystem,
		const FVector& LaunchDirection) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true"))
	TSubclassOf<ANPFireballProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true", ClampMin="1.0", Units="cm/s"))
	float LaunchSpeed = 1600.0f;

	/** 카메라 Pitch에 더할 상대 발사 각도입니다. 양수는 위쪽, 음수는 아래쪽입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true", Units="deg"))
	float LaunchPitchOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float SpawnForwardOffset = 30.0f;

	/** 발사자 위치에서 월드 위쪽으로 더할 투사체 생성 높이입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true", Units="cm"))
	float SpawnUpwardOffset = 0.0f;

	/** 발사 연출을 재생한 뒤 실제 투사체를 생성하기까지의 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="s"))
	float FireDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true", ClampMin="0.1", Units="s"))
	float ProjectileLifeTime = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Recoil", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm/s"))
	float RecoilBackwardSpeed = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Recoil", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm/s"))
	float RecoilUpwardSpeed = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Recoil|Shooter", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm/s"))
	float ShooterRecoilBackwardSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Recoil|Shooter", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm/s"))
	float ShooterRecoilUpwardSpeed = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball", meta=(AllowPrivateAccess="true"))
	FNPFireballExplosionSettings ExplosionSettings;
};
