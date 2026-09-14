#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPCCTVMapEvent.generated.h"

class UAudioComponent;
class URectLightComponent;
class USoundBase;
class ANPStablePhysicsPawn;

USTRUCT(BlueprintType)
struct FNPCCTVEventSoundSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CCTV Event|Sound")
	TObjectPtr<USoundBase> Sound;

	/** 이벤트가 시작된 뒤 이 음원을 재생할 때까지 기다리는 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CCTV Event|Sound", meta=(ClampMin="0.0", Units="s"))
	float EventStartDelay = 0.0f;

	/** 음원 파일 내부에서 재생을 시작할 지점입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CCTV Event|Sound", meta=(ClampMin="0.0", Units="s"))
	float PlaybackStartTime = 0.0f;

	/** 음원 파일 내부에서 재생을 멈출 지점입니다. 0이면 음원 끝까지 재생합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CCTV Event|Sound", meta=(ClampMin="0.0", Units="s"))
	float PlaybackEndTime = 0.0f;
};

UCLASS(Blueprintable)
class NOPHOTOS_API ANPCCTVMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	bool IsTargetReserved(ANPStablePhysicsPawn* Target, float ServerTime) const;
	bool TryReserveTarget(ANPStablePhysicsPawn* Target, float ServerTime, float ReservationDuration);

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 태그가 붙은 Rect Light 액터 또는 컴포넌트는 이벤트 중에도 기존 밝기를 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV Event|Lighting")
	FName ExcludedRectLightTag = TEXT("CCTVKeepOn");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV Event|Sound")
	TArray<FNPCCTVEventSoundSegment> EventSounds;

	/** 이벤트 음원이 재생되는 동안 적용할 BGM 음량 배율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV Event|Sound",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float EventSoundBGMDuckMultiplier = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CCTV Event|Sound",
		meta=(ClampMin="0.0", Units="s"))
	float BGMDuckFadeDuration = 0.25f;

private:
	void SetRectLightsDisabled(bool bDisabled);
	void StartEventSounds();
	void PlayEventSound(int32 SoundIndex);
	void StopEventSounds();
	void SetBGMDucked(bool bDucked);

	UFUNCTION()
	void HandleEventSoundFinished();

	TMap<TWeakObjectPtr<URectLightComponent>, float> SavedRectLightIntensities;
	TMap<TWeakObjectPtr<ANPStablePhysicsPawn>, float> TargetReservationEndTimes;
	TArray<FTimerHandle> EventSoundTimerHandles;
	TArray<TWeakObjectPtr<UAudioComponent>> ActiveEventSounds;
	bool bBGMDucked = false;
};
