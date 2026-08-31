#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Map/Light/NPBaseLight.h"
#include "NPSimpleLight.generated.h"

class UNPEmissiveLightComponent;

/** 발광 머티리얼이 적용된 메시로 표현하는 기본 조명입니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPSimpleLight : public ANPBaseLight
{
	GENERATED_BODY()

public:
	ANPSimpleLight();

protected:
	virtual void ApplyLightState() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simple Light|Components")
	TObjectPtr<UNPEmissiveLightComponent> EmissiveLight;
};
