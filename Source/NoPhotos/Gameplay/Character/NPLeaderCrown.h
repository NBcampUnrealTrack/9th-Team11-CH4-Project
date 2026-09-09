#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPLeaderCrown.generated.h"

class UStaticMeshComponent;

/** Blueprint에서 왕관 메시와 크기를 지정합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPLeaderCrown : public AActor
{
	GENERATED_BODY()

public:
	ANPLeaderCrown();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintSetter, Category="Crown")
	void SetVisibleToOwner(bool bNewVisibleToOwner);

	void SetVisualScaleMultiplier(float ScaleMultiplier);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crown")
	TObjectPtr<UStaticMeshComponent> CrownMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetVisibleToOwner,
		Category="Crown", meta=(DisplayName="나에게 보이기"))
	bool bVisibleToOwner = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crown", meta=(Units="deg/s"))
	float RotationSpeed = 30.0f;

private:
	FVector BaseCrownScale = FVector::OneVector;
	bool bBaseCrownScaleInitialized = false;
};
