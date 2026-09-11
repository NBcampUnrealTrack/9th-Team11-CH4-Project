#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPImpactReceiveComponent.generated.h"

class UPrimitiveComponent;
class AActor;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnDurabilityDepleted,
	const FVector&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnDurabilityDamaged,
	int32,
	int32,
	int32);

UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPImpactReceiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPImpactReceiveComponent();

	UFUNCTION(BlueprintPure, Category = "Durability")
	bool IsBroken() const { return CurrentHealth <= 0; }

	void IgnoreGrabImpact();
	void DepleteDurability(const FVector& ImpactLocation);
	void SetImpactThresholds(float InMinThreshold, float InMaxThreshold);
	/** 루트가 아닌 Primitive에서 충격을 받을 오브젝트가 지정할 수 있습니다. */
	void SetImpactTargetComponent(UPrimitiveComponent* InTargetComponent);
	/** 여러 Primitive가 하나의 내구도를 공유할 때 사용합니다. */
	void SetImpactTargetComponents(
		const TArray<UPrimitiveComponent*>& InTargetComponents);

	/** 서버에서 확정된 Trace 등의 비물리 충격을 기존 내구도 규칙으로 적용합니다. */
	bool ApplyExternalImpact(
		UPrimitiveComponent* HitComponent,
		AActor* ImpactSource,
		float ImpactStrength,
		const FVector& ImpactLocation);

	FOnDurabilityDamaged OnDamaged;
	FOnDurabilityDepleted OnDepleted;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Durability", meta=(ClampMin="0.0"))
	float MinImpactThreshold = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Durability", meta=(ClampMin="0.0"))
	float MaxImpactThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Durability", meta=(ClampMin="1"))
	int32 MinDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Durability", meta=(ClampMin="1"))
	int32 MaxDamage = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Durability", meta=(ClampMin="1"))
	int32 MaxHealth = 10;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Durability")
	int32 CurrentHealth = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Durability", meta=(ClampMin="0.0", Units="s"))
	float DamageCooldown = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Durability", meta=(ClampMin="0.0", Units="s"))
	float GrabImpactIgnoreDuration = 0.25f;

private:
	bool ApplyImpact(
		UPrimitiveComponent* HitComponent,
		AActor* ImpactSource,
		float ImpactStrength,
		const FVector& ImpactLocation);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> ImpactTargetComponents;

	double IgnoreDamageUntilTime = 0.0;
	double NextDamageAllowedTime = 0.0;
};
