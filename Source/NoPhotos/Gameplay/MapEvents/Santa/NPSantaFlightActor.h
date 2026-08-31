#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPSantaFlightTypes.h"
#include "NPSantaFlightActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** 외형은 BP에서 지정합니다. 서버가 확정한 비행 계획만 복제하고 각 화면에서 같은 시간으로 이동합니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPSantaFlightActor : public AActor
{
	GENERATED_BODY()

public:
	ANPSantaFlightActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버의 deferred spawn 중 한 번만 호출합니다. */
	bool InitializeFlight(const FNPSantaFlightPlan& InPlan);

	UFUNCTION(BlueprintPure, Category="Santa Flight")
	FNPSantaFlightPlan GetFlightPlan() const { return FlightPlan; }

	UFUNCTION(BlueprintPure, Category="Santa Flight")
	float GetFlightProgress() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Flight")
	TObjectPtr<USceneComponent> FlightRoot;

	/** 모델이 반대 방향을 향하면 액터 대신 이 컴포넌트의 상대 회전을 수정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Flight|Visual")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Flight|Visual")
	TObjectPtr<UStaticMeshComponent> SleighMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Flight|Visual")
	TObjectPtr<USkeletalMeshComponent> SantaMesh;

private:
	UFUNCTION()
	void OnRep_FlightPlan();
	bool TryGetServerTime(float& OutTime) const;
	void UpdateFlight();

	UPROPERTY(ReplicatedUsing=OnRep_FlightPlan)
	FNPSantaFlightPlan FlightPlan;
};
