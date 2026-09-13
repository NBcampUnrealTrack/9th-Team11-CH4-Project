#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NPCrowdControlImmunityGameplayEffect.generated.h"

/** 적용된 동안 사진 스턴과 물리 넉백 래그돌을 차단하는 공용 상태 효과입니다. */
UCLASS()
class NOPHOTOS_API UNPCrowdControlImmunityGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNPCrowdControlImmunityGameplayEffect(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
