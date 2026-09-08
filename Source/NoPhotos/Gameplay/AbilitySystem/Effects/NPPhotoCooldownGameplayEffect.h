#pragma once

#include "GameplayEffect.h"
#include "NPPhotoCooldownGameplayEffect.generated.h"

/** 사진 촬영 Ability의 재사용 대기시간을 표현합니다. */
UCLASS()
class NOPHOTOS_API UNPPhotoCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNPPhotoCooldownGameplayEffect(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
