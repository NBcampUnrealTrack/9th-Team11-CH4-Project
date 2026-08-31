#pragma once

#include "GameplayEffect.h"
#include "NPPossessionGameplayEffect.generated.h"

/** 빙의 전후/좌우 이동 반전 상태. 이벤트는 자신이 적용한 핸들만 제거합니다. */
UCLASS()
class NOPHOTOS_API UNPPossessionGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNPPossessionGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
