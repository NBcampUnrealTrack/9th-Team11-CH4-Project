#include "Gameplay/AbilitySystem/GameplayCue/NPSpotlightBonusGameplayCue.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Sound/SoundBase.h"

ANPSpotlightBonusGameplayCue::ANPSpotlightBonusGameplayCue()
{
	PrimaryActorTick.bCanEverTick = false;

	BonusEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BonusEffect"));
	BonusEffect->SetupAttachment(SceneRoot);
	BonusEffect->SetAutoActivate(false);
}

bool ANPSpotlightBonusGameplayCue::OnExecute_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	Super::OnExecute_Implementation(Target, Parameters);
	if (GetNetMode() == NM_DedicatedServer)
	{
		GameplayCueFinishedCallback();
		return true;
	}
	if (IsValid(BonusSound))
	{
		UAudioComponent* BonusAudio = UGameplayStatics::SpawnSoundAtLocation(
			this,
			BonusSound,
			IsValid(Target) ? Target->GetActorLocation() : GetActorLocation(),
			FRotator::ZeroRotator,
			1.0f,
			1.0f,
			0.0f,
			BonusSoundAttenuation);
		if (IsValid(BonusAudio) && BonusSoundPlaybackDuration > 0.0f)
		{
			BonusAudio->StopDelayed(BonusSoundPlaybackDuration);
		}
	}
	if (!BonusEffect->GetAsset())
	{
		GameplayCueFinishedCallback();
		return true;
	}

	SetActorHiddenInGame(false);
	BonusEffect->SetHiddenInGame(false);
	BonusEffect->SetVisibility(true, true);
	BonusEffect->OnSystemFinished.AddUniqueDynamic(
		this,
		&ANPSpotlightBonusGameplayCue::OnBonusEffectFinished);
	bIsPlaying = true;
	BonusEffect->ReinitializeSystem();
	return true;
}

bool ANPSpotlightBonusGameplayCue::GameplayCuePendingRemove()
{
	return bIsPlaying || Super::GameplayCuePendingRemove();
}

bool ANPSpotlightBonusGameplayCue::Recycle()
{
	bIsPlaying = false;
	BonusEffect->OnSystemFinished.RemoveDynamic(
		this,
		&ANPSpotlightBonusGameplayCue::OnBonusEffectFinished);
	BonusEffect->DeactivateImmediate();
	return Super::Recycle();
}

void ANPSpotlightBonusGameplayCue::OnBonusEffectFinished(UNiagaraComponent* FinishedComponent)
{
	GameplayCueFinishedCallback();
}
