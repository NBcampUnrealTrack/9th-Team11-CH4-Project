#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/Components/NPImpactReceiveComponent.h"
#include "NPPinataComponent.generated.h"

class ANPBaseRelic;
class FLifetimeProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPPinataBrokenSignature,
	FVector,
	ImpactLocation);

/** 충격 내구도가 소진되면 설정된 유물을 서버에서 생성하는 피냐타 기믹입니다. */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPPinataComponent : public UNPImpactReceiveComponent
{
	GENERATED_BODY()

public:
	UNPPinataComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Pinata")
	bool IsPinataBroken() const { return bIsBroken; }

	/** 서버와 모든 클라이언트에서 피냐타가 깨질 때 한 번 호출됩니다. */
	UPROPERTY(BlueprintAssignable, Category="Pinata")
	FNPPinataBrokenSignature OnPinataBroken;

protected:
	virtual void BeginPlay() override;

	/** 배열의 각 항목을 한 개씩 생성합니다. 같은 클래스를 여러 번 넣으면 여러 개가 생성됩니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Rewards")
	TArray<TSubclassOf<ANPBaseRelic>> RelicsToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Rewards", meta=(Units="cm"))
	FVector SpawnOffset = FVector(0.0f, 0.0f, 30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Rewards", meta=(ClampMin="0.0", Units="cm"))
	float SpawnRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Rewards", meta=(ClampMin="0.0", Units="cm/s"))
	float MinimumLaunchSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Rewards", meta=(ClampMin="0.0", Units="cm/s"))
	float MaximumLaunchSpeed = 450.0f;

	/** None이면 Root Primitive가 충격 대상입니다. 값이 있으면 해당 Component Tag의 Primitive를 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Impact")
	FName ImpactComponentTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Lifetime")
	bool bDestroyOwnerAfterBreak = true;

	/** 0보다 크게 두면 파괴 연출이 실행될 시간을 확보한 뒤 액터를 제거합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pinata|Lifetime", meta=(ClampMin="0.0", Units="s"))
	float DestroyDelay = 0.15f;

private:
	UFUNCTION()
	void OnRep_IsBroken();

	void HandleDurabilityDepleted(const FVector& ImpactLocation);
	void SpawnConfiguredRelics();
	void DestroyOwner();

	UPROPERTY(ReplicatedUsing=OnRep_IsBroken, VisibleInstanceOnly, Category="Pinata")
	bool bIsBroken = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pinata")
	FVector_NetQuantize10 ReplicatedImpactLocation = FVector::ZeroVector;

	FTimerHandle DestroyTimer;
};
