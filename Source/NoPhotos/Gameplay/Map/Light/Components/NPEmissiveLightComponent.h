#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "NPEmissiveLightComponent.generated.h"

class UMaterialInstanceDynamic;

/** 런타임에 메시 머티리얼의 밝기를 제어하는 조명 컴포넌트입니다. */
UCLASS(ClassGroup = (Light), meta = (BlueprintSpawnableComponent))
class NOPHOTOS_API UNPEmissiveLightComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UNPEmissiveLightComponent();

	UFUNCTION(BlueprintCallable, Category = "Emissive Light")
	void SetLightEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Emissive Light")
	bool IsLightEnabled() const { return bLightEnabled; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive Light|Material", meta = (ClampMin = "0"))
	int32 MaterialIndex = 0;

	/** 머티리얼에 노출된 발광 세기 Scalar Parameter 이름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive Light|Material")
	FName IntensityParameterName = TEXT("EmissiveIntensity");

private:
	void ApplyLightState();

	bool bLightEnabled = true;

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterialInstance;
};
