#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPRelicDeliveryEffect.generated.h"

class UNiagaraComponent;
class UStaticMesh;
class USkeletalMeshComponent;

/** 반환된 유물 메시를 사용하여 캐릭터를 향하는 Niagara 연출을 재생합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPRelicDeliveryEffect : public AActor
{
	GENERATED_BODY()

public:
	ANPRelicDeliveryEffect();

	virtual void Tick(float DeltaSeconds) override;

	void InitializeEffect(
		UStaticMesh* RelicMesh,
		AActor* InTargetActor,
		int32 RelicPrice);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Effect")
	float EmitterScale = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Effect", meta=(ClampMin="0"))
	int32 MinimumRelicPrice = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Effect", meta=(ClampMin="1"))
	int32 MaximumRelicPrice = 1000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic|Delivery Effect", meta=(ClampMin="1.0"))
	float MaximumCoinMultifly = 5.0f;

private:
	void UpdateTargetLocation();

	UFUNCTION()
	void HandleSystemFinished(UNiagaraComponent* FinishedComponent);

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> TargetMesh;
};
