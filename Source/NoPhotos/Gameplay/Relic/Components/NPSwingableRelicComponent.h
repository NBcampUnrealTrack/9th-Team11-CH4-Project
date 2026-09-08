#pragma once

#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "NPSwingableRelicComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct NOPHOTOS_API FNPRelicSwingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing", meta=(ClampMin="0.01", Units="s"))
	float Duration = 0.5f;

	/** 한 번 휘두르기가 끝난 뒤 다음 사용 입력을 받을 때까지의 대기시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing", meta=(ClampMin="0.0", Units="s"))
	float CooldownAfterSwing = 0.25f;

	/** 부호에 따라 회전 방향이 결정됩니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing")
	float Torque = -1500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing", meta=(ClampMin="0.0", Units="deg/s"))
	float MaxAngularSpeed = 1080.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Physics")
	bool bDisableHeldRelicGravity = true;

	/** 0이면 휘두르는 동안 질량을 변경하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Physics", meta=(ClampMin="0.0", Units="kg"))
	float HeldRelicMass = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Physical Animation", meta=(ClampMin="0.0"))
	float ArmOrientationStrength = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Physical Animation", meta=(ClampMin="0.0"))
	float ArmAngularVelocityStrength = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Physical Animation", meta=(ClampMin="0.0"))
	float HandOrientationStrength = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Physical Animation", meta=(ClampMin="0.0"))
	float HandAngularVelocityStrength = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Grab")
	bool bPreventGrabConstraintBreak = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Knockback")
	TSubclassOf<UGameplayEffect> KnockbackEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Knockback", meta=(ClampMin="0.0", Units="cm/s"))
	float KnockbackStrength = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Knockback", meta=(ClampMin="0.0", Units="cm/s"))
	float MinimumHitSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Knockback", meta=(ClampMin="0.0", Units="s"))
	float RehitCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Knockback", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CameraDirectionWeight = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Swing|Knockback|Debug")
	bool bDrawKnockbackDirection = true;
};

UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPSwingableRelicComponent
	: public UNPUsableRelicComponent
{
	GENERATED_BODY()

public:
	UNPSwingableRelicComponent();
	const FNPRelicSwingSettings& GetSwingSettings() const
	{
		return SwingSettings;
	}
	bool CanStartSwing() const;
	void StartSwingCooldown();

	void StartHitDetection(
		AActor* InAttackInstigator,
		UAbilitySystemComponent* InSourceAbilitySystem,
		const FVector& InCameraDirection);
	void StopHitDetection();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRelicHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	FVector CalculateKnockbackDirection(const FHitResult& Hit) const;
	UPrimitiveComponent* ResolveRelicMesh();

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> RelicMesh;

	TWeakObjectPtr<AActor> AttackInstigator;
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	FVector CameraDirection = FVector::ForwardVector;
	TMap<TWeakObjectPtr<AActor>, double> LastHitTimes;
	bool bHitDetectionActive = false;
	bool bPreviousNotifyRigidBodyCollision = false;
	double NextSwingAllowedTime = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Ability", meta=(AllowPrivateAccess="true"))
	FNPRelicSwingSettings SwingSettings;
};
