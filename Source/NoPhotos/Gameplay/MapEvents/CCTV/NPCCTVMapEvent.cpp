#include "Gameplay/MapEvents/CCTV/NPCCTVMapEvent.h"

#include "Components/AudioComponent.h"
#include "Components/RectLightComponent.h"
#include "Core/Audio/NPSoundSubsystem.h"
#include "Engine/RectLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Map/CCTV/NPCCTVSensor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

void ANPCCTVMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	SetRectLightsDisabled(bNewActive);
	if (bNewActive)
	{
		StartEventSounds();
	}
	else
	{
		StopEventSounds();
	}

	if (!HasAuthority())
	{
		return;
	}
	TargetReservationEndTimes.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ANPCCTVSensor> Iterator(World); Iterator; ++Iterator)
	{
		if (ANPCCTVSensor* Sensor = *Iterator; IsValid(Sensor))
		{
			Sensor->SetOwningMapEvent(bNewActive ? this : nullptr);
			Sensor->SetCCTVActive(bNewActive);
		}
	}
}

bool ANPCCTVMapEvent::IsTargetReserved(
	ANPStablePhysicsPawn* Target,
	const float ServerTime) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	const float* ReservationEndTime = TargetReservationEndTimes.Find(Target);
	return ReservationEndTime && ServerTime < *ReservationEndTime;
}

bool ANPCCTVMapEvent::TryReserveTarget(
	ANPStablePhysicsPawn* Target,
	const float ServerTime,
	const float ReservationDuration)
{
	if (!HasAuthority() || !IsEventActive() || !IsValid(Target)
		|| IsTargetReserved(Target, ServerTime))
	{
		return false;
	}

	TargetReservationEndTimes.Add(
		Target,
		ServerTime + FMath::Max(0.0f, ReservationDuration));
	return true;
}

void ANPCCTVMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEventSounds();
	SetRectLightsDisabled(false);
	Super::EndPlay(EndPlayReason);
}

void ANPCCTVMapEvent::StartEventSounds()
{
	StopEventSounds();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	EventSoundTimerHandles.SetNum(EventSounds.Num());
	for (int32 SoundIndex = 0; SoundIndex < EventSounds.Num(); ++SoundIndex)
	{
		const FNPCCTVEventSoundSegment& Segment = EventSounds[SoundIndex];
		if (!IsValid(Segment.Sound))
		{
			continue;
		}

		const float StartDelay = FMath::Max(0.0f, Segment.EventStartDelay);
		if (StartDelay <= 0.0f)
		{
			PlayEventSound(SoundIndex);
			continue;
		}

		FTimerDelegate PlayDelegate = FTimerDelegate::CreateUObject(
			this,
			&ThisClass::PlayEventSound,
			SoundIndex);
		GetWorldTimerManager().SetTimer(
			EventSoundTimerHandles[SoundIndex],
			PlayDelegate,
			StartDelay,
			false);
	}
}

void ANPCCTVMapEvent::PlayEventSound(const int32 SoundIndex)
{
	if (!IsEventActive() || !EventSounds.IsValidIndex(SoundIndex))
	{
		return;
	}

	const FNPCCTVEventSoundSegment& Segment = EventSounds[SoundIndex];
	if (!IsValid(Segment.Sound))
	{
		return;
	}

	const float PlaybackStartTime = FMath::Max(0.0f, Segment.PlaybackStartTime);
	const float PlaybackEndTime = FMath::Max(0.0f, Segment.PlaybackEndTime);
	if (PlaybackEndTime > 0.0f && PlaybackEndTime <= PlaybackStartTime)
	{
		return;
	}

	UAudioComponent* AudioComponent = UGameplayStatics::SpawnSound2D(
		this,
		Segment.Sound,
		1.0f,
		1.0f,
		PlaybackStartTime);
	if (!IsValid(AudioComponent))
	{
		return;
	}

	if (PlaybackEndTime > PlaybackStartTime)
	{
		AudioComponent->StopDelayed(PlaybackEndTime - PlaybackStartTime);
	}
	AudioComponent->OnAudioFinished.AddDynamic(
		this,
		&ThisClass::HandleEventSoundFinished);
	SetBGMDucked(true);
	ActiveEventSounds.Add(AudioComponent);
}

void ANPCCTVMapEvent::StopEventSounds()
{
	for (FTimerHandle& TimerHandle : EventSoundTimerHandles)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle);
	}
	EventSoundTimerHandles.Reset();

	for (const TWeakObjectPtr<UAudioComponent>& AudioComponent : ActiveEventSounds)
	{
		if (AudioComponent.IsValid())
		{
			AudioComponent->OnAudioFinished.RemoveAll(this);
			AudioComponent->Stop();
		}
	}
	ActiveEventSounds.Reset();
	SetBGMDucked(false);
}

void ANPCCTVMapEvent::SetBGMDucked(const bool bDucked)
{
	if (bBGMDucked == bDucked)
	{
		return;
	}

	bBGMDucked = bDucked;
	if (UNPSoundSubsystem* SoundSubsystem = UNPSoundSubsystem::Get(this))
	{
		SoundSubsystem->SetBGMDuckMultiplier(
			bDucked ? EventSoundBGMDuckMultiplier : 1.0f,
			BGMDuckFadeDuration);
	}
}

void ANPCCTVMapEvent::HandleEventSoundFinished()
{
	ActiveEventSounds.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& AudioComponent)
	{
		return !AudioComponent.IsValid() || !AudioComponent->IsPlaying();
	});
	if (ActiveEventSounds.IsEmpty())
	{
		SetBGMDucked(false);
	}
}

void ANPCCTVMapEvent::SetRectLightsDisabled(const bool bDisabled)
{
	if (!bDisabled)
	{
		for (const auto& Entry : SavedRectLightIntensities)
		{
			if (URectLightComponent* RectLight = Entry.Key.Get())
			{
				RectLight->SetIntensity(Entry.Value);
			}
		}
		SavedRectLightIntensities.Reset();
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !SavedRectLightIntensities.IsEmpty())
	{
		return;
	}

	for (TActorIterator<ARectLight> Iterator(World); Iterator; ++Iterator)
	{
		ARectLight* RectLightActor = *Iterator;
		URectLightComponent* RectLight = IsValid(RectLightActor)
			? RectLightActor->RectLightComponent.Get()
			: nullptr;
		if (!IsValid(RectLight)
			|| RectLightActor->GetLevel() != World->PersistentLevel
			|| RectLightActor->ActorHasTag(ExcludedRectLightTag)
			|| RectLight->ComponentHasTag(ExcludedRectLightTag))
		{
			continue;
		}

		SavedRectLightIntensities.Add(RectLight, RectLight->Intensity);
		RectLight->SetIntensity(0.0f);
	}
}
