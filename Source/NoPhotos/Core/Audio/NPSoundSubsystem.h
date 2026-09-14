#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NPSoundSubsystem.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * 레벨 전환 동안 유지되는 로컬 사운드 재생 창구입니다.
 * 네트워크 복제를 직접 수행하지 않으며, 멀티플레이 3D 사운드는 RPC를 받은 각 클라이언트가
 * PlaySFXAtLocation을 호출해야 합니다.
 */
UCLASS(BlueprintType)
class NOPHOTOS_API UNPSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sound|Subsystem", meta = (WorldContext = "WorldContextObject"))
	static UNPSoundSubsystem* Get(const UObject* WorldContextObject);

	/** UI, 로컬 셔터음처럼 위치가 필요 없는 사운드를 재생합니다. */
	UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
	void PlaySFX(USoundBase* Sound, float Volume = 1.0f, float Pitch = 1.0f, float StartTime = 0.0f);

	/** 촬영 위치처럼 월드 공간상의 위치에서 들리는 3D 사운드를 재생합니다. */
	UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
	void PlaySFXAtLocation(
		USoundBase* Sound,
		FVector Location,
		FRotator Rotation = FRotator::ZeroRotator,
		float Volume = 1.0f,
		float Pitch = 1.0f,
		float StartTime = 0.0f,
		USoundAttenuation* AttenuationSettings = nullptr,
		USoundConcurrency* ConcurrencySettings = nullptr);

	/** 기존 BGM을 페이드아웃하고 새 BGM을 페이드인합니다. 반복 여부는 사운드 에셋에서 설정합니다. */
	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void PlayBGM(USoundBase* Sound, float FadeDuration = 1.0f, float Volume = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void StopBGM(float FadeOutDuration = 1.0f);

	/** 기존 Ambient를 페이드아웃하고 새 Ambient를 페이드인합니다. BGM 볼륨을 사용합니다. */
	UFUNCTION(BlueprintCallable, Category = "Sound|Ambient")
	void PlayAmbient(USoundBase* Sound, float FadeDuration = 1.0f, float Volume = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Sound|Volume")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintCallable, Category = "Sound|Volume")
	void SetMasterVolume(float InVolume);

	UFUNCTION(BlueprintPure, Category = "Sound|Volume")
	float GetSFXVolume() const { return SFXVolume; }

	UFUNCTION(BlueprintCallable, Category = "Sound|Volume")
	void SetSFXVolume(float InVolume);

	UFUNCTION(BlueprintPure, Category = "Sound|Volume")
	float GetBGMVolume() const { return BGMVolume; }

	UFUNCTION(BlueprintCallable, Category = "Sound|Volume")
	void SetBGMVolume(float InVolume);

	/** 사용자 BGM 설정은 유지한 채 현재 BGM 재생 음량만 일시적으로 조절합니다. */
	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void SetBGMDuckMultiplier(float InMultiplier, float FadeDuration = 0.25f);

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CurrentBGMComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CurrentAmbientComponent;

	float MasterVolume = 1.0f;
	float SFXVolume = 1.0f;
	float BGMVolume = 1.0f;
	float BGMDuckMultiplier = 1.0f;
	float CurrentBGMBaseVolume = 1.0f;
	float CurrentAmbientBaseVolume = 1.0f;
};
