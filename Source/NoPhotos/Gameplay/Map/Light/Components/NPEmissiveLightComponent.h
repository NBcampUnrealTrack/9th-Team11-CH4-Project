#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "NPEmissiveLightComponent.generated.h"

class FLifetimeProperty;
class UMaterialInstanceDynamic;
class UMaterialInterface;

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

UCLASS(ClassGroup = (Light), meta = (BlueprintSpawnableComponent))
class NOPHOTOS_API UNPEmissiveLightComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UNPEmissiveLightComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Emissive Light")
	void SetLightEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Emissive Light")
	void SetLightIntensity(float InLightIntensity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Emissive Light")
	void SetLightColor(FLinearColor InLightColor);

	UFUNCTION(BlueprintPure, Category = "Emissive Light")
	bool IsLightEnabled() const { return bLightEnabled; }

	UFUNCTION(BlueprintPure, Category = "Emissive Light")
	float GetLightIntensity() const { return LightIntensity; }

	UFUNCTION(BlueprintPure, Category = "Emissive Light")
	FLinearColor GetLightColor() const { return LightColor; }

protected:
	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnRep_LightState();

	/** 발광 표현에 사용할 머티리얼 인스턴스입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive Light|Material")
	TObjectPtr<UMaterialInterface> LightMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive Light|Material", meta = (ClampMin = "0"))
	int32 MaterialIndex = 0;

	/** 머티리얼에 노출된 발광 세기 Scalar Parameter 이름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive Light|Material")
	FName IntensityParameterName = TEXT("EmissiveIntensity");

	/** 머티리얼에 노출된 발광 색상 Vector Parameter 이름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive Light|Material")
	FName ColorParameterName = TEXT("EmissiveColor");

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_LightState, BlueprintReadOnly, Category = "Emissive Light")
	bool bLightEnabled = true;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_LightState, BlueprintReadOnly, Category = "Emissive Light", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LightIntensity = 1.0f;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_LightState, BlueprintReadOnly, Category = "Emissive Light")
	FLinearColor LightColor = FLinearColor::White;

private:
	void ApplyLightState();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterialInstance;
};
