#pragma once

#include "GameplayEffect.h"
#include "NPKnockbackGameplayEffect.generated.h"

UCLASS()
class NOPHOTOS_API UNPKnockbackGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UNPKnockbackGameplayEffect(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
