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
class ANPBaseRelic; //델리게이트 때문에 추가

DECLARE_MULTICAST_DELEGATE_OneParam(FNPOnRelicValueChanged, ANPBaseRelic*);
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

	FNPOnRelicValueChanged OnRelicValueChanged;

	UFUNCTION(BlueprintPure, Category="Relic|Ownership")
	UNPRelicOwnershipComponent* GetOwnershipComponent() const { return OwnershipComponent; }

	void SetUnlocked(bool bUnlocked);
	bool AddPhotoPenalty(int32 PenaltyAmount);
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

	/** 사진 판정으로 누적된 감점. 들고 있는 모든 클라이언트 UI에 현재 가치가 보이도록 복제합니다. */
	UPROPERTY(ReplicatedUsing=OnRep_AccumulatedPhotoPenalty, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic|Delivery")
	int32 AccumulatedPhotoPenalty = 0;
};
