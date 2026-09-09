#include "Core/Audio/NPSoundSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

namespace NPSoundSubsystemPrivate
{
	bool CanPlayAudio(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_DedicatedServer;
	}

	float SanitizeVolume(const float Volume)
	{
		return FMath::IsFinite(Volume) ? FMath::Max(0.0f, Volume) : 1.0f;
	}

	float SanitizePitch(const float Pitch)
	{
		return FMath::IsFinite(Pitch) ? FMath::Max(0.01f, Pitch) : 1.0f;
	}

	float SanitizeStartTime(const float StartTime)
	{
		return FMath::IsFinite(StartTime) ? FMath::Max(0.0f, StartTime) : 0.0f;
	}
}

void UNPSoundSubsystem::Deinitialize()
{
	if (IsValid(CurrentBGMComponent))
	{
		CurrentBGMComponent->Stop();
	}

	CurrentBGMComponent = nullptr;
	Super::Deinitialize();
}

UNPSoundSubsystem* UNPSoundSubsystem::Get(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNPSoundSubsystem>() : nullptr;
}

void UNPSoundSubsystem::PlaySFX(USoundBase* Sound, const float Volume, const float Pitch, const float StartTime)
{
	UWorld* World = GetWorld();
	if (!IsValid(Sound) || !NPSoundSubsystemPrivate::CanPlayAudio(World))
	{
		return;
	}

	UGameplayStatics::PlaySound2D(
		World,
		Sound,
		NPSoundSubsystemPrivate::SanitizeVolume(Volume) * SFXVolume * MasterVolume,
		NPSoundSubsystemPrivate::SanitizePitch(Pitch),
		NPSoundSubsystemPrivate::SanitizeStartTime(StartTime));
}

void UNPSoundSubsystem::PlaySFXAtLocation(
	USoundBase* Sound,
	const FVector Location,
	const FRotator Rotation,
	const float Volume,
	const float Pitch,
	const float StartTime,
	USoundAttenuation* AttenuationSettings,
	USoundConcurrency* ConcurrencySettings)
{
	UWorld* World = GetWorld();
	if (!IsValid(Sound) || !NPSoundSubsystemPrivate::CanPlayAudio(World))
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		World,
		Sound,
		Location,
		Rotation,
		NPSoundSubsystemPrivate::SanitizeVolume(Volume) * SFXVolume * MasterVolume,
		NPSoundSubsystemPrivate::SanitizePitch(Pitch),
		NPSoundSubsystemPrivate::SanitizeStartTime(StartTime),
		AttenuationSettings,
		ConcurrencySettings);
}

void UNPSoundSubsystem::PlayBGM(USoundBase* Sound, const float FadeDuration, const float Volume)
{
	UWorld* World = GetWorld();
	if (!IsValid(Sound) || !NPSoundSubsystemPrivate::CanPlayAudio(World))
	{
		return;
	}

	const float SafeFadeDuration = FMath::IsFinite(FadeDuration) ? FMath::Max(0.0f, FadeDuration) : 1.0f;
	const float SafeVolume = NPSoundSubsystemPrivate::SanitizeVolume(Volume);

	if (IsValid(CurrentBGMComponent) && CurrentBGMComponent->IsPlaying()
		&& CurrentBGMComponent->GetSound() == Sound)
	{
		CurrentBGMBaseVolume = SafeVolume;
		CurrentBGMComponent->SetVolumeMultiplier(CurrentBGMBaseVolume * BGMVolume * MasterVolume);
		return;
	}

	UAudioComponent* NewBGMComponent = UGameplayStatics::CreateSound2D(
		World,
		Sound,
		0.0f,
		1.0f,
		0.0f,
		nullptr,
		true,
		true);

	if (!IsValid(NewBGMComponent))
	{
		return;
	}

	UAudioComponent* PreviousBGMComponent = CurrentBGMComponent;
	CurrentBGMComponent = NewBGMComponent;
	CurrentBGMBaseVolume = SafeVolume;

	if (IsValid(PreviousBGMComponent) && PreviousBGMComponent->IsPlaying())
	{
		PreviousBGMComponent->FadeOut(SafeFadeDuration, 0.0f);
	}

	CurrentBGMComponent->FadeIn(SafeFadeDuration, CurrentBGMBaseVolume * BGMVolume * MasterVolume);
}

void UNPSoundSubsystem::StopBGM(const float FadeOutDuration)
{
	UAudioComponent* BGMComponentToStop = CurrentBGMComponent;
	CurrentBGMComponent = nullptr;

	if (!IsValid(BGMComponentToStop) || !BGMComponentToStop->IsPlaying())
	{
		return;
	}

	const float SafeFadeOutDuration = FMath::IsFinite(FadeOutDuration)
		? FMath::Max(0.0f, FadeOutDuration)
		: 1.0f;
	BGMComponentToStop->FadeOut(SafeFadeOutDuration, 0.0f);
}

void UNPSoundSubsystem::SetMasterVolume(const float InVolume)
{
	MasterVolume = FMath::IsFinite(InVolume) ? FMath::Clamp(InVolume, 0.0f, 1.0f) : 1.0f;

	if (IsValid(CurrentBGMComponent))
	{
		CurrentBGMComponent->SetVolumeMultiplier(CurrentBGMBaseVolume * BGMVolume * MasterVolume);
	}
}

void UNPSoundSubsystem::SetSFXVolume(const float InVolume)
{
	SFXVolume = FMath::IsFinite(InVolume) ? FMath::Clamp(InVolume, 0.0f, 1.0f) : 1.0f;
}

void UNPSoundSubsystem::SetBGMVolume(const float InVolume)
{
	BGMVolume = FMath::IsFinite(InVolume) ? FMath::Clamp(InVolume, 0.0f, 1.0f) : 1.0f;

	if (IsValid(CurrentBGMComponent))
	{
		CurrentBGMComponent->SetVolumeMultiplier(CurrentBGMBaseVolume * BGMVolume * MasterVolume);
	}
}
