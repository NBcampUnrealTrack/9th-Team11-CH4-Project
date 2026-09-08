#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPImpactRotateComponent.generated.h"

class UPrimitiveComponent;
class AActor;

/** 충격의 방향과 세기에 따라 소유 액터를 좌우로 회전시키는 기믹 컴포넌트입니다. */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPImpactRotateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPImpactRotateComponent();

	/** 기본값은 소유 액터의 Root Primitive입니다. */
	UFUNCTION(BlueprintCallable, Category="Impact Rotation")
	void SetImpactTargetComponent(UPrimitiveComponent* InTargetComponent);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void HandleTargetHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	/** 이 값보다 작은 충격은 무시합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Strength", meta=(ClampMin="0.0"))
	float MinimumImpactImpulse = 100.0f;

	/** 이 값 이상의 충격은 최대 회전량으로 처리합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Strength", meta=(ClampMin="0.0"))
	float MaximumImpactImpulse = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Strength", meta=(ClampMin="0.0", Units="deg"))
	float MinimumRotationPerImpact = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Strength", meta=(ClampMin="0.0", Units="deg"))
	float MaximumRotationPerImpact = 45.0f;

	/** 초기 방향을 기준으로 누적할 수 있는 최대 좌우 회전 범위입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Movement", meta=(ClampMin="0.0", Units="deg"))
	float MaximumYawOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Movement", meta=(ClampMin="0.0"))
	float RotationInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Return")
	bool bReturnToInitialRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Return", meta=(ClampMin="0.0", Units="s"))
	float ReturnDelay = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Return", meta=(ClampMin="0.0"))
	float ReturnInterpSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation", meta=(ClampMin="0.0", Units="s"))
	float ImpactCooldown = 0.05f;

	/** 메시의 앞뒤 방향 때문에 회전 방향이 반대로 느껴질 때 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation")
	bool bInvertRotationDirection = false;

	/** 멀티플레이에서 서버가 액터 회전을 클라이언트에 전달하도록 설정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact Rotation|Networking")
	bool bEnableOwnerMovementReplication = true;

private:
	void BindImpactTarget();
	void UnbindImpactTarget();

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ImpactTargetComponent;

	FRotator InitialActorRotation = FRotator::ZeroRotator;
	float CurrentYawOffset = 0.0f;
	float TargetYawOffset = 0.0f;
	double NextImpactAllowedTime = 0.0;
	double ReturnStartTime = 0.0;
};
