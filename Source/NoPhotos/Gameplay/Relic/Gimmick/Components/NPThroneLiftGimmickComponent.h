#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Relic/Gimmick/Components/NPRelicGimmickComponent.h"
#include "NPThroneLiftGimmickComponent.generated.h"

class ANPBaseRelic;
class ANPWallLever;
class UCameraShakeBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnThroneLiftStarted);

/** Lifts the owning throne once and keeps an assigned relic aligned with it. */
UCLASS(ClassGroup=(Relic), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPThroneLiftGimmickComponent
	: public UNPRelicGimmickComponent
{
	GENERATED_BODY()

public:
	UNPThroneLiftGimmickComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 지정한 레버 4개가 모두 내려갔을 때 서버에서 한 번만 상승한다. */
	UFUNCTION(BlueprintCallable, Category="Throne Lift")
	void RaiseChair();

	UFUNCTION(BlueprintPure, Category="Throne Lift")
	bool IsChairRaised() const { return bIsRaised; }

	UPROPERTY(BlueprintAssignable, Category="Throne Lift")
	FOnThroneLiftStarted OnLiftStarted;

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, EditFixedSize, Category="Throne Lift|Levers")
	TArray<TObjectPtr<ANPWallLever>> RequiredLevers;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, EditFixedSize, Category="Throne Lift|Relics")
	TArray<TObjectPtr<ANPBaseRelic>> Relics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift")
	float LoweredRootZ = 103.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift")
	float RaisedRootZ = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift", meta=(ClampMin="0.01", Units="s"))
	float LiftDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift|Camera Shake")
	TSubclassOf<UCameraShakeBase> LiftCameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift|Camera Shake", meta=(ClampMin="0.0"))
	float CameraShakeScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift|Camera Shake", meta=(ClampMin="0.0", Units="cm"))
	float CameraShakeInnerRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift|Camera Shake", meta=(ClampMin="0.0", Units="cm"))
	float CameraShakeOuterRadius = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throne Lift|Camera Shake", meta=(ClampMin="0.0"))
	float CameraShakeFalloff = 1.0f;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayLiftCameraShake();

	UFUNCTION()
	void OnRep_LiftState();

	void AttachRelic();
	void DetachRelic();
	void SetOwnerSceneComponentsMovable() const;
	void ApplyLift(float Alpha) const;
	float GetSynchronizedWorldTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_LiftState)
	FVector LiftStartLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing=OnRep_LiftState)
	FVector LiftEndLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing=OnRep_LiftState)
	float LiftStartTime = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_LiftState)
	bool bIsLifting = false;

	UPROPERTY(ReplicatedUsing=OnRep_LiftState)
	bool bIsRaised = false;
};
