#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "NPScanComponent.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNPActorScannedSignature, AActor*, ScannedActor);

/** Sphere로 후보를 수집하고 Timeline의 전방 거리로 도착 여부를 판정합니다. */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPScanComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UNPScanComponent();

	UFUNCTION(BlueprintCallable, Category="Scan")
	void StartScan(float MaxDistance);

	UFUNCTION(BlueprintCallable, Category="Scan")
	void UpdateScanDistance(float Distance);

	UFUNCTION(BlueprintCallable, Category="Scan")
	void FinishScan();

	UFUNCTION(BlueprintPure, Category="Scan")
	bool IsActorInScanRange(const AActor* Actor) const;

	UPROPERTY(BlueprintAssignable, Category="Scan")
	FNPActorScannedSignature OnActorScanned;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Scan")
	FVector ScanOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Scan")
	FVector ScanForward = FVector::ForwardVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Scan")
	float CurrentScanDistance = 0.0f;

	/** 머티리얼 ConeCos에도 전달할 시작 시점의 각도 기준입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Scan")
	float ScanConeCos = 1.0f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan", meta=(ClampMin="0.0", ClampMax="89.0", Units="deg"))
	float ScanHalfAngle = 45.0f;

	/** 시작 시 후보 수집 반경에 더할 플레이어 이동 여유 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan", meta=(ClampMin="0.0", Units="cm"))
	float CandidateMovementMargin = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan|Debug")
	bool bDrawDebugRange = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scan|Debug", meta=(ClampMin="4", ClampMax="64"))
	int32 DebugSegments = 24;

private:
	bool UpdateScanView();

	bool bScanActive = false;
	float MaxScanDistance = 0.0f;
	TSet<TWeakObjectPtr<AActor>> PendingScanActors;
};
