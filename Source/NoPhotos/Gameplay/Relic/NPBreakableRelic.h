#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "NPBreakableRelic.generated.h"

class UPrimitiveComponent;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UNPImpactReceiveComponent;
struct FPropertyChangedEvent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPBreakableRelic : public ANPBaseRelic
{
	GENERATED_BODY()

public:
	ANPBreakableRelic(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Relic|Breakable")
	bool IsBroken() const { return bIsBroken; }

	virtual bool IsBonusQuestResolved() const override
	{
		return IsReturned() || IsBroken();
	}

	UFUNCTION(CallInEditor, Category = "Relic|Destruction")
	void RefreshGeometrySource();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnRep_IsBroken();

	UFUNCTION()
	void HandleFullyDecayed();

	/** 논리적인 파괴 상태가 최초 적용될 때 한 번 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Relic|Breakable")
	void OnRelicBroken();

	/** 충돌 피해가 적용될 때 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Relic|Breakable")
	void OnRelicDamaged(
		int32 Damage,
		int32 CurrentHealth,
		int32 MaxHealth,
		float RemainingHealthRatio);

	/** 서버에서 확정된 피해 연출을 현재 클라이언트에 전달합니다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRelicDamaged(
		int32 Damage,
		int32 CurrentHealth,
		int32 MaxHealth,
		float RemainingHealthRatio);

	/** 서버에서 확정된 파괴를 현재 접속 중인 모든 클라이언트에 전달합니다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBreakRelic(FVector_NetQuantize10 InBreakLocation);

	/** 파괴 시 활성화할 Geometry Collection입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Destruction")
	TObjectPtr<UGeometryCollection> BrokenGeometryAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Destruction", meta = (ClampMin = "0.0"))
	float MinImpactMassMultiplier = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Destruction", meta = (ClampMin = "0.0"))
	float MaxImpactMassMultiplier = 450.0f;

	/** 파괴 전에는 비활성 상태로 대기하고, 파괴 시 RelicMesh를 대체합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNPImpactReceiveComponent> ImpactReceiveComponent;

	UPROPERTY(ReplicatedUsing = OnRep_IsBroken, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic|Breakable")
	bool bIsBroken = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 BreakLocation = FVector::ZeroVector;

private:
	void BreakRelic(const FVector& ImpactLocation);
	void HandleDurabilityDamaged(int32 Damage, int32 CurrentHealth, int32 MaxHealth);
	void HandleDurabilityDepleted(const FVector& ImpactLocation);
	void HandleBreakableGrabStarted(UPrimitiveComponent* GrabbedComponent);
	void ApplyBrokenState();
	void BreakRootCluster();
	void SyncGeometrySource();
	void UpdateImpactThresholdsFromMass();
	bool bBrokenEventDispatched = false;
	bool bClusterBreakApplied = false;
};
