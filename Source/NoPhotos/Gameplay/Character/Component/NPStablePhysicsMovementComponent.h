#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPStablePhysicsMovementComponent.generated.h"

class USkeletalMeshComponent;
class UPhysicsControlComponent;

DECLARE_MULTICAST_DELEGATE(FOnStablePhysicsJumpApplied);

struct FNPStablePhysicsLocomotionInput
{
	FVector MoveInput = FVector::ZeroVector;
	FVector FacingDirection = FVector::ForwardVector;
	bool bHasFacingDirection = false;
	bool bJumpRequested = false;
};

/** 물리 시뮬레이션 중인 스켈레탈 메시에 힘 기반 이동과 자세 제어를 적용합니다. */
UCLASS(ClassGroup=(Physics), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPStablePhysicsMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPStablePhysicsMovementComponent();

	/** 물리 메시와 캐릭터 정면 계산에 사용할 메시 축 보정값을 설정합니다. */
	void Initialize(USkeletalMeshComponent* InPhysicsMesh, float InCharacterForwardYawOffset);

	/** 캐릭터 프로필에서 찾은 이동 중심과 발 본 이름을 적용합니다. */
	void ConfigureBoneNames(FName InPelvisBodyName, FName InLeftFootBoneName, FName InRightFootBoneName);
	void SetTargetPelvisHeight(float InTargetPelvisHeight);
	void SetMaxMoveSpeed(float InMaxMoveSpeed);
	void SetJumpVelocityChange(float InJumpVelocityChange);
	void SetFacingControlSettings(
		float InAngularStrength,
		float InAngularDampingRatio,
		float InMaxTorque,
		float InMaxTargetSpeed);

	/** 카메라 기준으로 계산된 월드 공간 이동 방향을 전달받습니다. */
	void SetMoveInput(const FVector& InMoveInput);

	/** 캐릭터가 따라볼 월드 공간의 수평 방향을 설정합니다. */
	void SetFacingDirection(const FVector& InFacingDirection);
	void InitializeFacingControl(UPhysicsControlComponent* InPhysicsControl);
	void SetFacingControlEnabled(bool bEnabled);
	void BeginRelicSwingRotation(
		float Torque,
		float MaxAngularSpeedDegrees);
	void EndRelicSwingRotation();

	/** false이면 상태 조회만 수행하고 실제 물리 Force와 회전은 적용하지 않습니다. */
	void SetPhysicsUpdatesEnabled(bool bEnabled) { bPhysicsUpdatesEnabled = bEnabled; }
	void SetAnimationStateOverride(
		bool bEnabled,
		const FVector& InVelocity,
		const FVector& InAcceleration,
		bool bInIsFalling);

	void RequestJump();
	FOnStablePhysicsJumpApplied OnJumpApplied;

	bool HasFacingDirection() const { return PendingInput.bHasFacingDirection; }
	FVector GetFacingDirection() const { return PendingInput.FacingDirection; }

	/** 수평면을 기준으로 메시가 실제 바라보는 정면 방향을 반환합니다. */
	FVector GetCurrentFacingDirection() const;

	UFUNCTION(BlueprintPure, Category="Stable Physics Movement")
	FVector GetVelocity() const { return bUseAnimationStateOverride ? AnimationVelocity : Velocity; }

	UFUNCTION(BlueprintPure, Category="Stable Physics Movement")
	FVector GetCurrentAcceleration() const { return bUseAnimationStateOverride ? AnimationAcceleration : CurrentAcceleration; }

	UFUNCTION(BlueprintPure, Category="Stable Physics Movement")
	bool GetIsFalling() const { return bUseAnimationStateOverride ? bAnimationIsFalling : bIsFalling; }

	UFUNCTION(BlueprintPure, Category="Stable Physics Movement")
	bool GetIsGrounded() const { return bUseAnimationStateOverride ? !bAnimationIsFalling : bGrounded; }

	UFUNCTION(BlueprintPure, Category="Stable Physics Movement")
	bool GetOrientRotationToMovement() const { return bOrientRotationToMovement; }

	UFUNCTION(BlueprintPure, Category="Stable Physics Movement")
	bool IsFacingRotationEnabled() const { return bOrientRotationToMovement; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category="Bones")
	FName PelvisBodyName = TEXT("pelvis");

	UPROPERTY(EditAnywhere, Category="Bones")
	FName LeftFootBoneName = TEXT("foot_l");

	UPROPERTY(EditAnywhere, Category="Bones")
	FName RightFootBoneName = TEXT("foot_r");

	float MaxMoveSpeed = 350.0f;

	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin="0.0"))
	float MoveStrength = 350.0f;

	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin="0.0"))
	float MoveDamping = 50.0f;

	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin="0.0"))
	float MaxMoveForce = 120000.0f;

	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AirControlMultiplier = 0.15f;

	UPROPERTY(EditAnywhere, Category="Turning")
	bool bOrientRotationToMovement = true;

	float FacingAngularStrength = 3.0f;

	float FacingAngularDampingRatio = 1.0f;

	float MaxFacingTorque = 750000.0f;

	/** Physics Control 목표가 카메라 방향을 따라가는 최대 속도(도/초)입니다. */
	float MaxFacingTargetSpeed = 180.0f;

	UPROPERTY(EditAnywhere, Category="Balance", meta=(ClampMin="0.0"))
	float BalanceStrength = 500000.0f;

	UPROPERTY(EditAnywhere, Category="Balance", meta=(ClampMin="0.0"))
	float BalanceDamping = 100000.0f;

	UPROPERTY(EditAnywhere, Category="Balance", meta=(ClampMin="0.0"))
	float MaxBalanceTorque = 750000.0f;

	float TargetPelvisHeight = 100.0f;

	UPROPERTY(EditAnywhere, Category="Ground Support", meta=(ClampMin="0.0"))
	float SupportStrength = 60.0f;

	UPROPERTY(EditAnywhere, Category="Ground Support", meta=(ClampMin="0.0"))
	float SupportDamping = 15.0f;

	UPROPERTY(EditAnywhere, Category="Ground Support", meta=(ClampMin="0.0"))
	float MaxSupportAcceleration = 5000.0f;

	UPROPERTY(EditAnywhere, Category="Ground Support", meta=(ClampMin="0.0"))
	float GroundTraceDistance = 150.0f;

	UPROPERTY(EditAnywhere, Category="Ground Support", meta=(ClampMin="0.0"))
	float GroundProbeRadius = 10.0f;

	float JumpVelocityChange = 350.0f;

private:
	FNPStablePhysicsLocomotionInput ConsumePendingInput();
	void SimulateLocomotion(
		float DeltaTime,
		const FNPStablePhysicsLocomotionInput& Input);
	void UpdateMovementState();
	void UpdateGroundedState();
	bool IsFootGrounded(FName FootBoneName) const;
	bool FindPelvisGroundDistance(float& OutGroundDistance) const;
	void UpdateGroundSupportPhysics();
	void UpdateMovementPhysics(const FVector& InMoveInput);
	void UpdateFacingPhysicsControl(
		float DeltaTime,
		const FVector& InFacingDirection,
		bool bInHasFacingDirection);
	void ResetFacingControlTarget();
	void UpdateRelicSwingRotation();
	void UpdateBalancePhysics();
	void UpdateJumpPhysics(bool bInJumpRequested);

	UPROPERTY(Transient)
	USkeletalMeshComponent* PhysicsMesh = nullptr;

	UPROPERTY(Transient)
	UPhysicsControlComponent* PhysicsControl = nullptr;

	FNPStablePhysicsLocomotionInput PendingInput;
	FQuat FacingTargetOrientation = FQuat::Identity;
	FName FacingControlName = TEXT("PelvisFacing");
	float FacingTargetVisualYaw = 0.0f;
	float RelicSwingTorque = 0.0f;
	float MaxRelicSwingAngularSpeed = 0.0f;
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentAcceleration = FVector::ZeroVector;
	float CharacterForwardYawOffset = 90.0f;
	bool bPhysicsUpdatesEnabled = true;
	bool bFacingControlCreated = false;
	bool bFacingControlEnabled = false;
	bool bFacingControlSuppressed = false;
	bool bRelicSwingRotationActive = false;
	bool bGrounded = false;
	bool bIsFalling = true;
	bool bUseAnimationStateOverride = false;
	FVector AnimationVelocity = FVector::ZeroVector;
	FVector AnimationAcceleration = FVector::ZeroVector;
	bool bAnimationIsFalling = true;
};
