#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Map/Light/NPBaseLight.h"
#include "NPVolumetricSpotLight.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTexture;

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

/** 볼륨 라이트 메시의 머티리얼 파라미터를 액터 단위로 제어합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPVolumetricSpotLight : public ANPBaseLight
{
	GENERATED_BODY()

public:
	ANPVolumetricSpotLight();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void ApplyLightState() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volumetric Spot Light|Components")
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volumetric Spot Light|Material")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volumetric Spot Light|Material", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float NearFadeDistance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volumetric Spot Light|Material", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Opacity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volumetric Spot Light|Material")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volumetric Spot Light|Material")
	TObjectPtr<UTexture> FalloffTexture;

private:
	void ApplyMaterialParameters();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
};
