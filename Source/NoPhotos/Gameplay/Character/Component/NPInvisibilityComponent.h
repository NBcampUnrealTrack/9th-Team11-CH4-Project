#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NPInvisibilityComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPInvisibilityChangedSignature, bool, bInvisible);

/** 이 컴포넌트가 변경한 외형만 종료 시 복원하기 위한 로컬 스냅샷입니다. */
USTRUCT()
struct FNPInvisibilityMeshState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<UMeshComponent> Mesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> AppliedMaterials;

	bool bWasHiddenInGame = false;
	bool bDidHideMesh = false;
	bool bCastShadow = false;
};

/**
 * GAS의 State.Invisible을 관찰합니다. 상태는 서버에서 결정하고 외형은 각 머신에서 적용합니다.
 * 조작 중인 Pawn은 반투명, 그 외 Pawn은 메시만 숨깁니다. 유물/UI/카메라/사진 판정은 건드리지 않습니다.
 * 현재 프로젝트의 클라이언트당 단일 로컬 플레이어를 기준으로 합니다.
 */
UCLASS(ClassGroup=(MapEvent), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPInvisibilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPInvisibilityComponent();

	/** 로컬에서 반투명하게 보여도 게임 규칙상으로는 투명화 상태입니다. */
	UFUNCTION(BlueprintPure, Category="Invisibility")
	bool IsInvisible() const { return bIsInvisible; }

	/** UI 등의 후속 연결점. 최초 바인딩 시에는 IsInvisible()도 조회해야 합니다. */
	UPROPERTY(BlueprintAssignable, Category="Invisibility")
	FNPInvisibilityChangedSignature OnInvisibilityChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 조작 중인 자기 캐릭터의 불투명도. 0은 투명, 1은 불투명입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility|Appearance",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float SelfOpacity = 0.15f;

	/** Translucent의 Opacity 또는 Masked 디더에 연결된 Scalar Parameter 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility|Appearance")
	FName OpacityParameterName = TEXT("InvisibilityOpacity");

	/**
	 * 선택적 자기 캐릭터 전용 머티리얼. 지정하면 모든 대상 메시 슬롯에 사용합니다.
	 * 비워 두면 각 기존 머티리얼이 위 파라미터와 투명화 Blend Mode를 지원해야 합니다.
	 * 에셋이 준비되지 않은 슬롯은 원본을 유지하며 경고를 출력합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Invisibility|Appearance")
	TObjectPtr<UMaterialInterface> SelfMaterialOverride;

private:
	void HandleInvisibilityTagChanged(FGameplayTag Tag, int32 NewCount);
	void RefreshPresentation();
	void ApplyToMesh(UMeshComponent* Mesh);
	void RestoreMeshes();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(Transient)
	TArray<FNPInvisibilityMeshState> MeshStates;

	FDelegateHandle InvisibilityTagHandle;
	bool bIsInvisible = false;
	bool bLastLocallyControlled = false;
};
