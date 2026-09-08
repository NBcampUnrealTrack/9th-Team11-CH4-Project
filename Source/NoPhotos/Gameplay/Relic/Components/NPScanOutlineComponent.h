#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPScanOutlineComponent.generated.h"

class UCurveFloat;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPScanOutlineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPScanOutlineComponent();

	UFUNCTION(BlueprintCallable, Category="Scan Outline")
	void PlayOutline();

	UFUNCTION(BlueprintCallable, Category="Scan Outline")
	void StopOutline();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan Outline")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan Outline")
	TObjectPtr<UCurveFloat> ExpansionCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan Outline", meta=(ClampMin="0.0"))
	float MaxExpansionRatio = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan Outline", meta=(ClampMin="0.01", Units="s"))
	float AnimationDuration = 2.0f;

private:
	UMeshComponent* ResolveTargetMesh();
	void SetExpansionRatio(float Ratio);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMaterialInstance;

	TWeakObjectPtr<UMeshComponent> TargetMesh;
	float ElapsedTime = 0.0f;
};
