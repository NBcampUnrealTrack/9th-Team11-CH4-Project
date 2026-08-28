#pragma once

#include "CoreMinimal.h"
#include "Data/Interfaces/NPLockable.h"
#include "GameFramework/Actor.h"
#include "NPRelicCase.generated.h"

class FLifetimeProperty;
class UGeometryCollectionComponent;
class UNPImpactReceiveComponent;
class UNPRelicCaseSlotComponent;
class USceneComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPRelicCase
	: public AActor,
	  public INPLockable
{
	GENERATED_BODY()

public:
	ANPRelicCase();

	/** 슬롯에 지정된 유물을 에디터 레벨에 다시 생성합니다. */
	UFUNCTION(CallInEditor, Category = "Relic Case|Editor", meta = (DisplayName = "지정 유물 생성"))
	void CreateConfiguredRelics();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Relic Case")
	bool IsBroken() const { return bIsBroken; }

	UFUNCTION(BlueprintPure, Category = "Relic Case")
	bool IsUnlocked() const { return bIsUnlocked; }

	UFUNCTION(BlueprintPure, Category = "Relic Case")
	bool IsAccessible() const { return bIsBroken || bIsUnlocked; }

	/** 외부 기믹이 서버에서 케이스를 해금할 때 호출합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relic Case")
	bool UnlockCase();

	/** 외부 기믹이 서버에서 케이스를 다시 잠글 때 호출합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relic Case")
	bool LockCase();

	virtual bool TrySetLocked_Implementation(bool bLocked) override;
	virtual bool IsLocked_Implementation() const override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsBroken();

	UFUNCTION()
	void OnRep_IsUnlocked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Relic Case")
	void OnCaseBroken();

	UFUNCTION(BlueprintImplementableEvent, Category = "Relic Case")
	void OnCaseUnlocked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Relic Case")
	void OnCaseLocked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Relic Case")
	void OnCaseDamaged(float RemainingHealthRatio);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBreakCase(FVector_NetQuantize10 InBreakLocation);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic Case|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 파괴할 Geometry Collection들을 배치하는 기준 컴포넌트입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic Case|Components")
	TObjectPtr<USceneComponent> GeometryScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic Case|Components")
	TObjectPtr<UNPImpactReceiveComponent> ImpactReceiveComponent;

	/** 유물 슬롯들을 배치하는 기준 컴포넌트입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic Case|Components")
	TObjectPtr<USceneComponent> RelicScene;

	/** GeometryScene 아래에서 수집된 Geometry Collection들입니다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Relic Case|Components")
	TArray<TObjectPtr<UGeometryCollectionComponent>> CaseGeometryCollections;

	/** RelicScene 아래에서 수집된 유물 슬롯들입니다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Relic Case|Components")
	TArray<TObjectPtr<UNPRelicCaseSlotComponent>> RelicSlots;

	UPROPERTY(ReplicatedUsing = OnRep_IsBroken, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic Case|State")
	bool bIsBroken = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsUnlocked, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic Case|State")
	bool bIsUnlocked = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 BreakLocation = FVector::ZeroVector;

private:
	void CollectCaseGeometryCollections();
	void CollectRelicSlots();
	void InitializeGeometryCollections();
	void HandleDurabilityDamaged(
		int32 Damage,
		int32 CurrentHealth,
		int32 MaxHealth);
	void HandleDurabilityDepleted(const FVector& ImpactLocation);
	void BreakCase(const FVector& ImpactLocation);
	void ApplyCaseState();
	void ApplyBrokenState();
	void SpawnContainedRelics();
	void ReleaseContainedRelics();

	bool bBrokenStateApplied = false;
	bool bBrokenEventDispatched = false;
	bool bUnlockedEventDispatched = false;
	bool bLockedEventDispatched = true;
};
