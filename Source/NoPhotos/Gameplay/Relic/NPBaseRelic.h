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

	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	bool IsReturned() const { return bIsReturned; }

	UFUNCTION(BlueprintPure, Category="Relic|Delivery")
	int32 GetBasePrice() const;

	const FNPRelicTableRow* GetRelicTableData() const;
	void SetRelicTableData(const FDataTableRowHandle& InRelicTableData);

	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category="Relic|Delivery")
	int32 GetAccumulatedPhotoPenalty() const { return AccumulatedPhotoPenalty; }

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

	void ReleaseFromDisplay();
	void HandleGrabStarted(UPrimitiveComponent* GrabbedComponent);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPrimitiveComponent> RelicMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UGrabbableComponent> GrabbableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPRelicOwnershipComponent> OwnershipComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Relic", meta=(RowType="/Script/NoPhotos.NPRelicTableRow"))
	FDataTableRowHandle RelicTableData;

	UPROPERTY(ReplicatedUsing=OnRep_IsDisplayed, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic")
	bool bIsDisplayed = true;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic")
	bool bIsUnlocked = true;

	UPROPERTY(ReplicatedUsing=OnRep_IsReturned, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic|Delivery")
	bool bIsReturned = false;

	/** 서버에서만 누적되는 사진 판정 감점입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Relic|Delivery")
	int32 AccumulatedPhotoPenalty = 0;
};
