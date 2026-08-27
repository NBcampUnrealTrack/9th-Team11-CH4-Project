#pragma once

#include "GameplayEffect.h"
#include "NPInvisibilityGameplayEffect.generated.h"

/** 영역 이벤트의 투명화 + 시야 제한. 적용한 시스템이 자신의 핸들로 제거합니다. */
UCLASS()
class NOPHOTOS_API UNPInvisibilityGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNPInvisibilityGameplayEffect(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
