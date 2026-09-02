#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "NPBreakableRelic.generated.h"

class UPrimitiveComponent;
class UGeometryCollectionComponent;
class UNPImpactReceiveComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPBreakableRelic : public ANPBaseRelic
{
	GENERATED_BODY()

public:
	ANPBreakableRelic(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FVector GetRelicWorldLocation() const override;

	UFUNCTION(BlueprintPure, Category = "Relic|Breakable")
	bool IsBroken() const { return bIsBroken; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsBroken();

	UFUNCTION()
	void HandleFullyDecayed();

	/** 논리적인 파괴 상태가 최초 적용될 때 한 번 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Relic|Breakable")
	void OnRelicBroken();

	/** 충돌 피해가 적용될 때 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Relic|Breakable")
	void OnRelicDamaged(int32 Damage, int32 CurrentHealth, int32 MaxHealth);

	/** 서버에서 확정된 파괴를 현재 접속 중인 모든 클라이언트에 전달합니다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBreakRelic(FVector_NetQuantize10 InBreakLocation);

	/** 부모의 공통 유물 컴포넌트를 Geometry Collection으로 교체해 보관합니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Relic|Destruction")
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
	bool bBrokenEventDispatched = false;
	bool bClusterBreakApplied = false;
};
