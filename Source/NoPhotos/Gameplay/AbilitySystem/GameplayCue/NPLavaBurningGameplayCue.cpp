#include "Gameplay/AbilitySystem/GameplayCue/NPLavaBurningGameplayCue.h"

#include "NiagaraComponent.h"

ANPLavaBurningGameplayCue::ANPLavaBurningGameplayCue()
{
	LavaFireLeft = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LavaFireLeft"));
	LavaFireLeft->SetupAttachment(SceneRoot);
	LavaFireLeft->SetRelativeLocation(FVector(0.0f, -25.0f, 100.0f));
	LavaFireLeft->SetAutoActivate(false);

	LavaFireRight = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LavaFireRight"));
	LavaFireRight->SetupAttachment(SceneRoot);
	LavaFireRight->SetRelativeLocation(FVector(0.0f, 25.0f, 100.0f));
	LavaFireRight->SetAutoActivate(false);
}

bool ANPLavaBurningGameplayCue::WhileActive_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	Super::WhileActive_Implementation(Target, Parameters);
	SetLavaActive(true);
	return true;
}

bool ANPLavaBurningGameplayCue::OnRemove_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	SetLavaActive(false);
	Super::OnRemove_Implementation(Target, Parameters);
	return true;
}

bool ANPLavaBurningGameplayCue::Recycle()
{
	SetLavaActive(false);
	return Super::Recycle();
}

void ANPLavaBurningGameplayCue::SetLavaActive(bool bActive)
{
	for (UNiagaraComponent* Fire : { LavaFireLeft.Get(), LavaFireRight.Get() })
	{
		if (!Fire)
		{
			continue;
		}
		if (bActive)
		{
			Fire->Activate(true);
		}
		else
		{
			Fire->DeactivateImmediate();
		}
	}
}
