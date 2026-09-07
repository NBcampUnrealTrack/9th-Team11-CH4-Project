#pragma once

#include "CoreMinimal.h"
#include "Shakes/DefaultCameraShakeBase.h"
#include "NPThroneLiftCameraShake.generated.h"

UCLASS()
class NOPHOTOS_API UNPThroneLiftCameraShake : public UDefaultCameraShakeBase
{
	GENERATED_BODY()

public:
	UNPThroneLiftCameraShake(const FObjectInitializer& ObjectInitializer);
};
