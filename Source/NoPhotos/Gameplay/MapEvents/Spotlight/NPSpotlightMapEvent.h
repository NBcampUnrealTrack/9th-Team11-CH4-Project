#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "GameplayTagContainer.h"
#include "NPSpotlightMapEvent.generated.h"

class ANPBaseRelic;
class ANPEventSpotlight;
class ANPStablePhysicsPawn;
class UNPStablePhysicsGrabComponent;
class URectLightComponent;

USTRUCT()
struct FNPSpotlightCycle
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ANPEventSpotlight> ActiveSpotlight;

	UPROPERTY()
	float StartServerWorldTime = 0.0f;

	UPROPERTY()
	float EndServerWorldTime = 0.0f;
};

UCLASS(Blueprintable)
class NOPHOTOS_API ANPSpotlightMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPSpotlightMapEvent();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FNPSpotlightCycle& GetSpotlightCycle() const { return SpotlightCycle; }

protected:
	virtual void BeginPlay() override;
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight Event|Locations")
	FGameplayTag SpotlightSpawnGroup;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight Event")
	TSubclassOf<ANPEventSpotlight> SpotlightClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight Event|Timing", meta = (ClampMin = "0.1", Units = "s"))
	float LightDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight Event|Timing", meta = (ClampMin = "0.1", Units = "s"))
	float DarkDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight Event|Lighting", meta = (ClampMin = "0.0", Units = "s"))
	float RectLightFadeDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight Event|Price", meta = (ClampMin = "0.1", Units = "s"))
	float BonusInterval = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotlight Event|Price", meta = (ClampMin = "0.0"))
	double BonusRate = 0.1;

private:
	void SetMainRectLightsDimmed(bool bDimmed, bool bImmediate = false);
	void UpdateMainRectLightFade(float DeltaSeconds);
	void SpawnSpotlights();
	void StartLightCycle(float Now);
	void UpdatePriceBonuses(float Now);
	void ClearExposures();
	void CleanupEvent();
	float GetServerTime() const;

	UPROPERTY(Replicated)
	FNPSpotlightCycle SpotlightCycle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANPEventSpotlight>> SpawnedSpotlights;

	struct FPlayerExposure
	{
		TWeakObjectPtr<ANPBaseRelic> Relic;
		TWeakObjectPtr<UNPStablePhysicsGrabComponent> GrabComponent;
		FDelegateHandle GrabChangedHandle;
		float NextBonusTime = 0.0f;
	};

	TMap<TWeakObjectPtr<ANPStablePhysicsPawn>, FPlayerExposure> PlayerExposures;
	TMap<TWeakObjectPtr<ANPBaseRelic>, float> LastRelicBonusTimes;
	TMap<TWeakObjectPtr<URectLightComponent>, float> MainRectLights;
	float RectLightFadeElapsed = 0.0f;
	float RectLightTargetIntensity = 160.0f;
	bool bRectLightsFading = false;
};
