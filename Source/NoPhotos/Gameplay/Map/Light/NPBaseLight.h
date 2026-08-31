#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPBaseLight.generated.h"

class USceneComponent;

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

/** 메시 기반 조명의 공통 활성화 상태를 관리하는 기본 액터입니다. */
UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API ANPBaseLight : public AActor
{
	GENERATED_BODY()

public:
	ANPBaseLight();

	UFUNCTION(BlueprintCallable, Category = "Light")
	void SetLightEnabled(bool bEnabled);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void ApplyLightState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	bool bLightEnabled = true;
};
