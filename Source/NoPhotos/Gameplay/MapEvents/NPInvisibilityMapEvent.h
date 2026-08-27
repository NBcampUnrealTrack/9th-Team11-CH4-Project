#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "NPMapEvent.h"
#include "NPInvisibilityMapEvent.generated.h"

class UAbilitySystemComponent;
class ANPMapEventSpawnVolume;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/** 서버 판정 박스의 복사본. 비복제 SpawnVolume/서버 전용 위치 레벨에 의존하지 않습니다. */
USTRUCT()
struct FNPInvisibilityFogRegion
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform BoundsTransform = FTransform::Identity;

	UPROPERTY()
	FVector BoxExtent = FVector::ZeroVector;
};

/** 서버가 지정 SpawnVolume 안의 플레이어에게만 GAS 투명화 상태를 유지합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPInvisibilityMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPInvisibilityMapEvent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void ApplyEventState_Implementation(bool bNewActive) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 그룹의 볼륨 중 하나라도 Pawn의 Actor 위치를 포함하면 투명화합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Volumes")
	FGameplayTag InvisibilitySpawnGroup;

	/** 활성 이벤트의 서버 영역 판정 주기입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Volumes",
		meta=(ClampMin="0.02", Units="s"))
	float VolumeCheckInterval = 0.1f;

	/** 투명화 이벤트에서만 별도 표시 메시를 생성합니다. SpawnVolume은 수정하지 않습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog")
	bool bShowRegionFog = true;

	/** 깊이 적분용 Surface / Translucent / Two Sided / Disable Depth Test 머티리얼입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog")
	TObjectPtr<UMaterialInterface> RegionFogMaterial;

	/** 미터당 안개 밀도. 최종 불투명도는 시선이 안개를 통과한 길이에 따라 달라집니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float RegionFogOpacity = 0.2f;

	/** 경계에서 내부로 들어가며 농도가 증가하는 폭. 실제 판정 박스는 변경하지 않습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog",
		meta=(ClampMin="1.0", Units="cm"))
	float RegionFogEdgeFadeDistance = 150.0f;

	/** 표시용 안개의 모서리 둥글기입니다. 실제 판정 박스는 그대로 유지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog",
		meta=(ClampMin="0.0", Units="cm"))
	float RegionFogCornerRadius = 150.0f;

	/** 흐르는 농도 무늬의 공간 주파수. 작은 값일수록 큰 덩어리입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog", meta=(ClampMin="0.0001"))
	float RegionFogNoiseScale = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float RegionFogNoiseStrength = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility Event|Region Fog")
	FLinearColor RegionFogColor = FLinearColor(0.3f, 0.4f, 0.45f, 1.0f);

private:
	void RefreshAffectedPlayers();
	void RemoveAppliedEffects();
	void UpdateFogRegions(const TArray<ANPMapEventSpawnVolume*>& ActiveVolumes);
	void RefreshRegionFog();
	void ClearRegionFog();

	UFUNCTION()
	void OnRep_FogRegions();

	UPROPERTY(ReplicatedUsing=OnRep_FogRegions)
	TArray<FNPInvisibilityFogRegion> FogRegions;

	// Cube는 안개 계산 범위만 지정하는 프록시입니다. 표면 자체의 색을 표시하지 않습니다.
	UPROPERTY()
	TObjectPtr<UStaticMesh> RegionFogMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RegionFogMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RegionFogComponents;

	bool bWarnedRegionFogSetup = false;

	// 다른 투명화 효과를 지우지 않도록 이 이벤트가 생성한 핸들만 보관합니다.
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> AppliedEffects;
	FTimerHandle PlayerRefreshTimer;
	bool bWarnedMissingVolumes = false;
};
