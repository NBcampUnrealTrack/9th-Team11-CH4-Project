#pragma once

#include "GameplayEffect.h"
#include "NPPhotoStunGameplayEffect.generated.h"

/** 사진 증거 판정에 성공한 플레이어에게 일시적인 조작 불가 태그를 부여합니다. */
UCLASS()
class NOPHOTOS_API UNPPhotoStunGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNPPhotoStunGameplayEffect(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
