#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPFireballProjectile.generated.h"

class ANPBaseRelic;
class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPFireballExplosionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="1.0", Units="cm"))
	float Radius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0.0", Units="cm/s"))
	float HorizontalKnockbackStrength = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Explosion", meta=(ClampMin="0.0", Units="cm/s"))
	float UpwardKnockbackStrength = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Explosion")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;
};

UCLASS(Blueprintable)
class NOPHOTOS_API ANPFireballProjectile : public AActor
{
	GENERATED_BODY()

public:
	ANPFireballProjectile();

	void InitializeProjectile(
		UAbilitySystemComponent* InSourceAbilitySystem,
		ANPBaseRelic* InSourceRelic,
		const FVector& LaunchVelocity,
		const FNPFireballExplosionSettings& InExplosionSettings,
		float LifeTime);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION()
	void HandleProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplode(
		FVector_NetQuantize10 ExplosionLocation,
		float ExplosionRadius);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNiagaraComponent> FireballEffectComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Flight")
	TObjectPtr<UNiagaraSystem> FlightEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Flight")
	FVector FlightEffectRelativeLocation = FVector::ZeroVector;

	/** 원래 위쪽으로 타오르는 불꽃의 +Z를 투사체 진행 반대 방향인 -X로 향하게 합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Flight")
	FRotator FlightEffectRelativeRotation = FRotator(90.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Flight", meta=(ClampMin="0.0"))
	float FireballScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Explosion", meta=(ClampMin="0.0"))
	float ExplosionSoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fireball|Effects|Explosion", meta=(ClampMin="0.0", Units="s"))
	float ExplosionSoundStartTime = 0.0f;

private:
	void Explode(const FVector& ExplosionLocation);
	void ApplyExplosionImpulse(const FVector& ExplosionLocation);
	void ApplyCharacterKnockback(AActor* TargetActor, const FVector& KnockbackVelocity);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<ANPBaseRelic> SourceRelic;

	UPROPERTY(Transient)
	FNPFireballExplosionSettings ExplosionSettings;

	bool bInitialized = false;
	bool bExploded = false;
};
