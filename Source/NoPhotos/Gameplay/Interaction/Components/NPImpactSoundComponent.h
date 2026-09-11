#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPImpactSoundComponent.generated.h"

class UPrimitiveComponent;
class USoundBase;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPImpactSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPImpactSoundComponent();

	/** 지정하지 않으면 소유 액터의 루트 PrimitiveComponent를 사용합니다. */
	UFUNCTION(BlueprintCallable, Category="Impact Sound")
	void SetImpactTargetComponent(UPrimitiveComponent* InTargetComponent);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Sound")
	TArray<TObjectPtr<USoundBase>> ImpactSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact Sound", meta=(ClampMin="0.01"))
	float MinimumPitch = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact Sound", meta=(ClampMin="0.01"))
	float MaximumPitch = 1.1f;

	/** 감지 대상 질량(kg)에 곱하여 최소 충격량을 계산합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Sound", meta=(ClampMin="0.0", DisplayName="Minimum Impact Impulse Per Mass"))
	float MinimumImpactImpulse = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Sound", meta=(ClampMin="0.0", Units="s"))
	float ImpactCooldown = 0.2f;

private:
	void BindImpactTarget();
	void UnbindImpactTarget();

	UFUNCTION()
	void HandleTargetHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ImpactTargetComponent;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LastPlayedSound;

	double NextImpactAllowedTime = 0.0;
};
