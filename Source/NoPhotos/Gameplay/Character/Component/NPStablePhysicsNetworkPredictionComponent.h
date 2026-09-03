#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "NPStablePhysicsNetworkPredictionComponent.generated.h"

class FLifetimeProperty;
class UNPStablePhysicsGrabComponent;
class UNPStablePhysicsMovementComponent;
class USkeletalMeshComponent;

USTRUCT()
struct FNPStablePhysicsRootState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Position = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 LinearVelocity = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 AngularVelocity = FVector::ZeroVector;

	UPROPERTY()
	float ServerWorldTime = 0.0f;
};

/**
 * 평상시에는 서버가 소유 클라이언트의 Root 상태를 따라갑니다.
 * 공유 Grab 중에는 서버 권한으로 전환하여 소유 클라이언트를 서버 상태에 맞춥니다.
 */
UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPStablePhysicsNetworkPredictionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPStablePhysicsNetworkPredictionComponent();

	void Initialize(
		USkeletalMeshComponent* InPhysicsMesh,
		UNPStablePhysicsMovementComponent* InMovement,
		UNPStablePhysicsGrabComponent* InGrab,
		FName InRootBodyName);

	void SendMoveInput(const FVector& WorldMoveInput, float InputViewYaw);
	void SendStopMove();
	void SendJumpRequest();
	void SetExternalGrabActive(bool bActive) { bExternalGrabActive = bActive; }
	void SetServerAuthoritativeInteraction(bool bActive);

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION(Server, Unreliable)
	void ServerSetMoveInput(
		uint16 InputSequence,
		FVector_NetQuantizeNormal WorldMoveInput,
		float InputViewYaw);

	UFUNCTION(Server, Reliable)
	void ServerStopMove(uint16 InputSequence);

	UFUNCTION(Server, Reliable)
	void ServerRequestJump();

	UFUNCTION(Server, Unreliable)
	void ServerSetClientRootState(
		uint16 StateSequence,
		FNPStablePhysicsRootState NewRootState);

	UFUNCTION()
	void OnRep_ServerRootState();

	UFUNCTION()
	void OnRep_ServerAuthoritativeInteraction();

	void CaptureServerRootState();
	void ApplyOwnerCorrection(float DeltaTime);
	void ApplyServerCorrection(float DeltaTime);
	void ApplyCorrection(
		const FNPStablePhysicsRootState& TargetRootState,
		float DeltaTime,
		bool bUseFullBodyCorrection);
	void SendPendingMoveInput(float DeltaTime);
	void SendClientRootState(float DeltaTime);
	bool IsNearUnheldDynamicBody(const FVector& RootPosition) const;
	bool FindBlockingCorrectionNormal(
		const FVector& Start,
		const FVector& End,
		FVector& OutBlockingNormal) const;
	bool IsFullBodyCorrectionBlocked(const FVector& CorrectionDelta) const;
	bool IsRecoveryTargetClear(const FVector& TargetPosition) const;
	bool ShouldRecoverFromBlockedCorrection(
		float PositionErrorSize,
		const FVector& PositionError,
		const FVector& CurrentVelocity,
		const FVector& TargetVelocity,
		float DeltaTime);
	void RecoverFullBody(
		const FVector& TargetPosition,
		const FNPStablePhysicsRootState& TargetRootState);
	void ResetBlockedCorrectionTracking();
	bool AcceptInputSequence(uint16 InputSequence);
	float GetEstimatedServerWorldTime() const;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> PhysicsMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNPStablePhysicsMovementComponent> Movement = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNPStablePhysicsGrabComponent> Grab = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_ServerRootState)
	FNPStablePhysicsRootState ServerRootState;

	UPROPERTY(ReplicatedUsing=OnRep_ServerAuthoritativeInteraction)
	bool bServerAuthoritativeInteraction = false;

	FNPStablePhysicsRootState ClientRootState;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.0"))
	float PositionErrorTolerance = 2.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.01"))
	float PositionCorrectionTime = 0.2f;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.0"))
	float MaximumCorrectionSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.0"))
	float MaximumCorrectionAcceleration = 1800.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.0"))
	float HardSnapDistance = 250.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.0"))
	float MaximumExtrapolationTime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.0"))
	float MaximumServerStateAge = 0.4f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Dynamic Contact", meta=(ClampMin="0.0"))
	float DynamicBodyDetectionRadius = 80.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Dynamic Contact", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DynamicBodyCorrectionScale = 0.2f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float CorrectionSweepRadius = 25.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float RecoveryTargetCheckHeightOffset = 20.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction")
	bool bDrawRecoveryTargetDebug = true;

	/** 이보다 작은 수직 오차에서는 바닥과 천장 Hit를 막힘 판정에서 제외합니다. */
	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float VerticalLayerSeparationDistance = 50.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float BlockedCorrectionDistance = 30.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float BlockedCorrectionDelay = 0.35f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float MinimumCorrectionProgressSpeed = 3.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float MinimumCorrectionClosingSpeed = 1.0f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float MinimumBlockedErrorDirectionDot = 0.9f;

	UPROPERTY(EditAnywhere, Category="Network Prediction|Blocked Correction", meta=(ClampMin="0.0"))
	float RecoveryCooldown = 0.15f;

	UPROPERTY(EditAnywhere, Category="Network Prediction", meta=(ClampMin="0.01"))
	float InputSendInterval = 1.0f / 30.0f;

	FName RootBodyName = NAME_None;
	FVector PendingMoveInput = FVector::ZeroVector;
	float PendingMoveViewYaw = 0.0f;
	uint16 LocalInputSequence = 0;
	uint16 LastServerInputSequence = 0;
	uint16 LocalRootStateSequence = 0;
	uint16 LastServerRootStateSequence = 0;
	float InputSendAccumulator = 1.0f / 30.0f;
	float RootStateSendAccumulator = 1.0f / 30.0f;
	float PreviousCorrectionErrorSize = 0.0f;
	FVector PreviousCorrectionErrorDirection = FVector::ZeroVector;
	float BlockedCorrectionTime = 0.0f;
	float RecoveryCooldownRemaining = 0.0f;
	bool bHasReceivedInput = false;
	bool bHasServerRootState = false;
	bool bHasReceivedClientRootState = false;
	bool bExternalGrabActive = false;
};
