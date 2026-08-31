#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NPVisionRestrictionComponent.generated.h"

class APlayerCameraManager;
class APawn;
class UAbilitySystemComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** SceneDepth와 동일한 카메라 전방 깊이(cm)를 사용하는 안개 설정입니다. */
USTRUCT(BlueprintType)
struct FNPVisionRestrictionSettings
{
	GENERATED_BODY()

	/** 카메라 전방 깊이가 이 값까지는 안개 없이 보입니다. 단위 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vision Restriction", meta=(ClampMin="0.0", Units="cm"))
	float FogStartDistance = 300.0f;

	/** 이 전방 깊이부터 완전히 안개색으로 덮습니다. 구형 반경이 아닙니다. 단위 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vision Restriction", meta=(ClampMin="1.0", Units="cm"))
	float MaxViewDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vision Restriction")
	FLinearColor FogColor = FLinearColor(0.12f, 0.14f, 0.16f, 1.0f);
};

/** 컴포넌트 전용. 기존 카메라 후처리/FOV를 덮어쓰지 않고 자신의 안개만 합성합니다. */
UCLASS(NotBlueprintable, Transient)
class NOPHOTOS_API UNPVisionRestrictionCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	void Initialize(APawn* InPawn, UMaterialInstanceDynamic* InMaterial);

protected:
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight,
		FPostProcessSettings& PostProcessSettings) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> RestrictedPawn;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FogMaterial;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPVisionRestrictionChangedSignature, bool, bRestricted);

/** 서버 GAS 상태를 관찰하고 해당 Pawn을 조작하는 로컬 카메라에만 안개를 적용합니다. */
UCLASS(ClassGroup=(MapEvent), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPVisionRestrictionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPVisionRestrictionComponent();

	UFUNCTION(BlueprintPure, Category="Vision Restriction")
	bool IsVisionRestricted() const { return bIsVisionRestricted; }

	/** 활성 여부와 별개의 설정값. 서버/사진 담당자도 동일한 Pawn 기본값을 조회합니다. */
	UFUNCTION(BlueprintPure, Category="Vision Restriction")
	FNPVisionRestrictionSettings GetVisionRestrictionSettings() const;

	/** 현재 최대 전방 가시거리(cm). 0은 이 컴포넌트에 의한 제한이 없다는 뜻입니다. */
	UFUNCTION(BlueprintPure, Category="Vision Restriction")
	float GetMaxViewDistance() const;

	/** 로컬 화면 전용 강도(0~1). 서버 제한 상태/촬영 판정용 값이 아닙니다. */
	UFUNCTION(BlueprintPure, Category="Vision Restriction")
	float GetVisionFogStrength() const { return CurrentFogStrength; }

	static float AdvanceFogStrength(float CurrentStrength, bool bRestricted, float DeltaTime,
		float FadeInDuration, float FadeOutDuration);

	/** 최초 연결 시에는 IsVisionRestricted()도 조회합니다. */
	UPROPERTY(BlueprintAssignable, Category="Vision Restriction")
	FNPVisionRestrictionChangedSignature OnVisionRestrictionChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 사용자가 생성할 Post Process 머티리얼. 누락되면 상태만 유지하고 경고합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vision Restriction|Fog")
	TObjectPtr<UMaterialInterface> VisionFogMaterial;

	/** 서버와 클라이언트가 공유하는 BP 기본값. 런타임 변경/복제는 지원하지 않습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vision Restriction|Fog")
	FNPVisionRestrictionSettings FogSettings;

	/** 안개 강도가 0에서 1까지 증가하는 시간. 0이면 즉시 제한합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vision Restriction|Transition", meta=(ClampMin="0.0", Units="s"))
	float FogFadeInDuration = 1.0f;

	/** 안개 강도가 1에서 0까지 감소하는 시간. 0이면 즉시 복원합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vision Restriction|Transition", meta=(ClampMin="0.0", Units="s"))
	float FogFadeOutDuration = 1.0f;

private:
	void HandleVisionTagChanged(FGameplayTag Tag, int32 NewCount);
	void RefreshPresentation(float DeltaTime = 0.0f);
	void RemovePresentation();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FogMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UNPVisionRestrictionCameraModifier> CameraModifier;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerCameraManager> AppliedCameraManager;

	FDelegateHandle VisionTagHandle;
	bool bIsVisionRestricted = false;
	bool bWarnedInvalidMaterial = false;
	float CurrentFogStrength = 0.0f;
};
