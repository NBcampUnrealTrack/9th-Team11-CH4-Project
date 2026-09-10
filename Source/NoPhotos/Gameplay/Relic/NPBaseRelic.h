#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "NPBaseRelic.generated.h"

class UGrabbableComponent;
class UNPRelicOwnershipComponent;
class UPrimitiveComponent;
class FLifetimeProperty;
struct FNPRelicTableRow;
class ANPBaseRelic;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FNPOnRelicValueChanged,
	ANPBaseRelic*);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FNPOnRelicReleasedFromDisplay,
	ANPBaseRelic*);

UCLASS(Abstract, Blueprintable)
class NOPHOTOS_API ANPBaseRelic : public AActor
{
	GENERATED_BODY()

public:
	ANPBaseRelic(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Relic")
	bool IsDisplayed() const { return bIsDisplayed; }

	UFUNCTION(BlueprintPure, Category="Relic")
	bool IsUnlocked() const { return bIsUnlocked; }

	UFUNCTION(BlueprintPure, Category="Relic|Physics")
	bool ShouldStartWithPhysicsEnabled() const { return bStartWithPhysicsEnabled; }

	UFUNCTION(BlueprintPure, Category="Relic")
	virtual FVector GetRelicWorldLocation() const;

	/** 반환 연출에서 원본 유물의 메쉬와 재질을 복사할 수 있도록 시각 컴포넌트를 반환합니다. */
	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	UPrimitiveComponent* GetRelicMeshComponent() const { return RelicMesh; }

	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	bool IsReturned() const { return bIsReturned; }

	UFUNCTION(BlueprintPure, Category="Relic|Bonus Quest")
	virtual bool IsBonusQuestResolved() const { return IsReturned(); }

	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	int32 GetBasePrice() const;

	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	int32 GetCurrentPrice() const;

	const FNPRelicTableRow* GetRelicTableData() const;
	void SetRelicTableData(const FDataTableRowHandle& InRelicTableData);

	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category="Relic|Delivery")
	int32 GetAccumulatedPhotoPenalty() const { return AccumulatedPhotoPenalty; }

	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category="Relic|Delivery")
	int32 GetSuccessfulEvidenceCaptureCount() const
	{
		return SuccessfulEvidenceCaptureCount;
	}

	/** 서버와 클라이언트에서 현재 유물 가치가 변경될 때 실행됩니다. */
	FNPOnRelicValueChanged OnRelicValueChanged;

	/** 서버에서 유물이 전시 상태를 벗어나는 최초 순간에 한 번 실행됩니다. */
	FNPOnRelicReleasedFromDisplay OnReleasedFromDisplay;

	UFUNCTION(BlueprintPure, Category="Relic|Ownership")
	UNPRelicOwnershipComponent* GetOwnershipComponent() const { return OwnershipComponent; }

	void SetUnlocked(bool bUnlocked);
	/** 성공 촬영 횟수를 증가시키고 기본 가격에 대한 누적 비율로 감점을 다시 계산합니다. */
	bool AddPhotoPenaltyCapture(float PenaltyRatePerCapture);
	/** 기본 가격의 지정 비율을 현재 가격에 누적하고 실제 변경 여부를 반환합니다. 서버에서만 적용합니다. */
	bool AddPriceBonus(double BonusRate);
	bool TryMarkReturned();

	/** 서버에서 전시 상태를 해제하고 물리를 활성화한 뒤 질량과 무관한 속도 충격을 적용합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Relic|Physics")
	bool ReleaseWithVelocityImpulse(FVector VelocityImpulse);

protected:
	static const FName RelicComponentName;

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsDisplayed();

	UFUNCTION()
	void OnRep_IsReturned();

	UFUNCTION()
	void OnRep_AccumulatedPhotoPenalty();

	UFUNCTION()
	void OnRep_AccumulatedPriceBonus();

	void ReleaseFromDisplay();
	void HandleGrabStarted(UPrimitiveComponent* GrabbedComponent);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPrimitiveComponent> RelicMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UGrabbableComponent> GrabbableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPRelicOwnershipComponent> OwnershipComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic", meta=(RowType="/Script/NoPhotos.NPRelicTableRow"))
	FDataTableRowHandle RelicTableData;

	/** 유물 Blueprint가 시작부터 전시 상태를 해제하고 물리를 적용할지 설정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic|Physics")
	bool bStartWithPhysicsEnabled = false;

	UPROPERTY(ReplicatedUsing=OnRep_IsDisplayed, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic")
	bool bIsDisplayed = true;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic")
	bool bIsUnlocked = true;

	UPROPERTY(ReplicatedUsing=OnRep_IsReturned, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic|Delivery")
	bool bIsReturned = false;

	/** 사진 판정으로 누적된 감점입니다. 현재 가격 UI 갱신을 위해 클라이언트에 복제합니다. */
	UPROPERTY(ReplicatedUsing=OnRep_AccumulatedPhotoPenalty, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic|Delivery")
	int32 AccumulatedPhotoPenalty = 0;

	/** 소수 금액은 누적하고 최종 가격에서 반올림합니다. */
	UPROPERTY(ReplicatedUsing=OnRep_AccumulatedPriceBonus)
	double AccumulatedPriceBonus = 0.0;

	/** 서버에서만 관리하는 유효한 유물 증거 사진의 누적 횟수입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic|Delivery")
	int32 SuccessfulEvidenceCaptureCount = 0;
};
