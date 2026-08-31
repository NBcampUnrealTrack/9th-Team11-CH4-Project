#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Photo/NPRelicHolderInterface.h"
#include "NPReplicatedStablePhysicsPawn.generated.h"

class UPrimitiveComponent;
class UAbilitySystemComponent;
class AController;
class FLifetimeProperty;
class UNPAbilitySystemComponent;
class UNPInvisibilityComponent;
class UNPControlReversalComponent;
class UNPVisionRestrictionComponent;
class UNPStablePhysicsNetworkPredictionComponent;
class UNPPhotoWorldFeedbackComponent;
class ANPBaseRelic;

USTRUCT()
struct FReplicatedStableGrabState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> GrabbedActor = nullptr;

	UPROPERTY()
	FName GrabbedComponentName = NAME_None;

	UPROPERTY()
	FName GrabbedBoneName = NAME_None;

	UPROPERTY()
	FTransform ConstraintFrame1 = FTransform::Identity;

	UPROPERTY()
	FTransform ConstraintFrame2 = FTransform::Identity;
};

/** 평상시에는 소유 클라이언트, 공유 Grab 중에는 서버가 이동 물리 기준을 결정합니다. */
UCLASS()
class NOPHOTOS_API ANPReplicatedStablePhysicsPawn
	: public ANPStablePhysicsPawn
	, public INPRelicHolderInterface
	, public IAbilitySystemInterface
{
	GENERATED_BODY()
	friend class UNPStablePhysicsDebugComponent;

public:
	ANPReplicatedStablePhysicsPawn();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void AddExternalVelocityChange(
		const FVector& VelocityChange) override;

	UFUNCTION(BlueprintPure, Category="Network|Grab")
	bool IsReplicatedRightHandActive() const { return bReplicatedRightHandActive; }

	UFUNCTION(BlueprintPure, Category="Network|Grab")
	bool IsReplicatedGrabActive() const { return IsValid(ReplicatedGrabState.GrabbedActor); }

	/** 서버 사진 검증 등에서 소유 클라이언트가 복제한 최신 시점 회전을 조회합니다. */
	FRotator GetServerViewRotation() const { return GetTargetViewRotation(); }

	virtual AActor* GetHeldRelic_Implementation() const override;

	/** 로컬 실제 카메라 정보를 서버에 전달하여 조준 유물 발사를 요청합니다. */
	UFUNCTION(Server, Reliable)
	void ServerRequestAimableRelicFire(
		FVector_NetQuantize10 CameraLocation,
		FVector_NetQuantizeNormal CameraForward);

	/** 서버가 확정한 촬영자 표시를 현재 관련된 모든 클라이언트에서 재생합니다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayPhotographerFeedback();

	/** 서버가 확정한 피촬영자 표시를 현재 관련된 모든 클라이언트에서 재생합니다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayPhotographedFeedback();

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void ApplyMoveInput(const FVector& WorldMoveInput) override;
	virtual void ApplyJumpRequest() override;
	virtual void ApplyRightHandState(bool bActive) override;
	virtual void OnRep_PlayerState() override;
	virtual FRotator GetTargetViewRotation() const override;

private:
	static constexpr float ViewRotationSendInterval = 0.05f;

	/** 현재 카메라 회전을 서버 권한 캐릭터 제어에 전달합니다. */
	UFUNCTION(Server, Unreliable)
	void ServerSetViewRotation(uint16 CompressedYaw, uint16 CompressedPitch);

	UFUNCTION(Server, Reliable)
	void ServerSetRightHandActive(bool bActive);

	UFUNCTION(Client, Reliable)
	void ClientApplyExternalVelocityChange(
		FVector_NetQuantize10 VelocityChange);

	UFUNCTION()
	void OnRep_RightHandActive();

	UFUNCTION()
	void OnRep_GrabState();

	UFUNCTION()
	void OnRep_ExternallyGrabbed();

	void HandleGrabbedComponentChanged(UPrimitiveComponent* NewGrabbedComponent);
	void AddExternalGrabber();
	void RemoveExternalGrabber();
	UPrimitiveComponent* ResolveReplicatedGrabbedComponent() const;
	void UpdateClientSimulationState();
	void UpdateReplicatedGrabVisualTarget();
	void UpdateLocalPredictedGrab(float DeltaSeconds);
	void UpdateServerReplicatedState();
	void UpdateViewRotationReplication(float DeltaSeconds);
	void SetReplicatedViewRotation(const FRotator& NewViewRotation);
	void SetServerRightHandState(bool bActive);

	/** 다른 클라이언트에서도 오른손 IK 상태를 동일하게 표시하기 위한 값입니다. */
	UPROPERTY(ReplicatedUsing=OnRep_RightHandActive)
	bool bReplicatedRightHandActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_GrabState)
	FReplicatedStableGrabState ReplicatedGrabState;

	/** 다른 캐릭터에게 잡힌 동안에만 소유 클라이언트의 전신 보정을 활성화합니다. */
	UPROPERTY(ReplicatedUsing=OnRep_ExternallyGrabbed)
	bool bExternallyGrabbed = false;

	/** 서버와 클라이언트 손 위치 차이를 확인하기 위한 디버그 값입니다. */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 ReplicatedServerHandWorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Network|Grab Debug")
	bool bDrawGrabNetworkDebug = true;

	// 클라이언트 애니메이션에 사용할 서버의 이동 속도입니다.
	UPROPERTY(Replicated)
	FVector_NetQuantize10 ReplicatedAnimationVelocity = FVector::ZeroVector;

	// 클라이언트 애니메이션에 사용할 서버의 이동 가속도입니다.
	UPROPERTY(Replicated)
	FVector_NetQuantize10 ReplicatedAnimationAcceleration = FVector::ZeroVector;

	// 클라이언트 애니메이션에 사용할 서버의 낙하 상태입니다.
	UPROPERTY(Replicated)
	bool bReplicatedAnimationIsFalling = true;

	// 클라이언트 애니메이션에 사용할 서버의 정면 방향입니다.
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal ReplicatedAnimationForwardDirection = FVector::ForwardVector;

	/** 서버 물리와 비소유 클라이언트의 손/허리 표현에 사용할 시점 회전입니다. */
	UPROPERTY(Replicated)
	FRotator ReplicatedViewRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, Category="Network")
	TObjectPtr<UNPStablePhysicsNetworkPredictionComponent> NetworkPrediction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ability System", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNPAbilitySystemComponent> AbilitySystem;

	/** GAS 상태를 관찰하여 자기 반투명/타인 비표시를 로컬에서 적용합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Invisibility", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNPInvisibilityComponent> Invisibility;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vision Restriction", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNPVisionRestrictionComponent> VisionRestriction;

	/** GAS 상태에 따라 원본 이동 입력의 전후/좌우 성분을 반전합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Control Reversal", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNPControlReversalComponent> ControlReversal;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> RelicUseAction;

	/** 사진 촬영/피촬영 상태를 발밑 데칼로 표시합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Photo|World Feedback", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNPPhotoWorldFeedbackComponent> PhotoWorldFeedback;

	/** 서버에서 이 캐릭터가 현재 잡고 있는 다른 캐릭터를 추적합니다. */
	UPROPERTY(Transient)
	TObjectPtr<ANPReplicatedStablePhysicsPawn> ExternallyGrabbedTargetPawn = nullptr;

	/** 서버에서 RightHandGrab을 소유자로 등록한 현재 Relic입니다. */
	UPROPERTY(Transient)
	TObjectPtr<ANPBaseRelic> RegisteredGrabbedRelic = nullptr;

	UPROPERTY(EditAnywhere, Category="Network|Grab Prediction", meta=(ClampMin="0.0"))
	float LocalGrabPredictionTimeout = 0.35f;

	bool bClientWasMoving = false;
	bool bLocalRightHandActive = false;
	bool bAwaitingServerGrabConfirmation = false;
	float LocalGrabPredictionTimeRemaining = 0.0f;
	float ViewRotationSendAccumulator = ViewRotationSendInterval;
	
	int32 ExternalGrabberCount = 0;
};
