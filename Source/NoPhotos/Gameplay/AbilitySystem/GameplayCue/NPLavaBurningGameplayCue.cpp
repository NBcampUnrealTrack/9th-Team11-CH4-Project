#include "Gameplay/AbilitySystem/GameplayCue/NPLavaBurningGameplayCue.h"

#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"

ANPLavaBurningGameplayCue::ANPLavaBurningGameplayCue()
{
	bAllowMultipleWhileActiveEvents = true;

	LavaFireLeft = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LavaFireLeft"));
	LavaFireLeft->SetupAttachment(SceneRoot);
	LavaFireLeft->SetRelativeLocation(FVector(0.0f, -25.0f, 100.0f));
	LavaFireLeft->SetAutoActivate(false);

	LavaFireRight = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LavaFireRight"));
	LavaFireRight->SetupAttachment(SceneRoot);
	LavaFireRight->SetRelativeLocation(FVector(0.0f, 25.0f, 100.0f));
	LavaFireRight->SetAutoActivate(false);

	LavaAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LavaAudio"));
	LavaAudio->SetupAttachment(SceneRoot);
	LavaAudio->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	LavaAudio->SetAutoActivate(false);
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

	if (bActive)
	{
		RestartLavaSound();
	}
	else
	{
		StopLavaSound();
	}
}

void ANPLavaBurningGameplayCue::RestartLavaSound()
{
	GetWorldTimerManager().ClearTimer(LavaSoundStopTimerHandle);
	LavaAudio->Stop();
	LavaAudio->Play(FMath::Max(0.0f, LavaSoundStartTime));
	GetWorldTimerManager().SetTimer(
		LavaSoundStopTimerHandle,
		this,
		&ANPLavaBurningGameplayCue::StopLavaSound,
		FMath::Max(0.01f, LavaSoundDuration),
		false);
}

void ANPLavaBurningGameplayCue::StopLavaSound()
{
	GetWorldTimerManager().ClearTimer(LavaSoundStopTimerHandle);
	LavaAudio->Stop();
}
