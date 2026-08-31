#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Character/NPStablePhysicsCharacterProfile.h"
#include "NPStablePhysicsPawn.generated.h"

class UCameraComponent;
class UAnimMontage;
class UInputAction;
class UPhysicalAnimationComponent;
class UPhysicsControlComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USoundAttenuation;
class USoundBase;
class USpringArmComponent;
class UNPStablePhysicsDebugComponent;
class UNPStablePhysicsGrabComponent;
class UNPStablePhysicsMovementComponent;
struct FNPRelicSwingSettings;
struct FInputActionValue;

/** 물리 캐릭터 프로토타입을 위한 안정적인 힘 기반 Pawn입니다. */
UCLASS()
class NOPHOTOS_API ANPStablePhysicsPawn : public APawn
{
	GENERATED_BODY()
	friend class UNPStablePhysicsDebugComponent;

public:
	ANPStablePhysicsPawn();

	virtual FVector GetVelocity() const override;
	/** 외부 게임 규칙이 현재 이동 의도를 즉시 제거할 때 사용합니다. */
	void StopMovementInput();
	/** 점프대처럼 외부 게임 규칙이 물리 캐릭터 전체에 즉시 속도 변화를 적용할 때 사용합니다. */
	virtual void AddExternalVelocityChange(const FVector& VelocityChange);
	bool BeginRelicSwing(const FNPRelicSwingSettings& Settings);
	void EndRelicSwing();

	/** 설정된 사진 촬영 Montage를 한 번 재생합니다. */
	bool PlayPhotoShotMontage();

	/** 서버에서 승인한 사진 촬영의 3D 셔터음을 모든 클라이언트에 전달합니다. */
	void BroadcastPhotoShutterSound(const FVector& SoundLocation);

	/** 로컬 플레이어의 3인칭/사진 1인칭 카메라 전환을 시작합니다. */
	void SetPhotoViewActive(bool bActive);
	/** 로컬 플레이어의 조준 유물 카메라 전환을 시작하거나 종료합니다. */
	void SetRelicAimViewActive(bool bActive);

	UFUNCTION(BlueprintPure, Category="Photo")
	bool IsPhotoViewActive() const { return bPhotoViewActive; }

	UFUNCTION(BlueprintPure, Category="Photo")
	bool IsPhotoViewReady() const;

	UFUNCTION(BlueprintPure, Category="Relic|Aim")
	bool IsRelicAimViewActive() const { return bRelicAimViewActive; }

	UPROPERTY(BlueprintReadOnly, Transient, Category="Right Hand IK")
	FVector RightHandIKLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Right Hand IK")
	FVector RightElbowIKLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Right Hand IK")
	float RightHandIKAlpha = 0.0f;

	/** 카메라 Pitch를 허리 가동 범위로 제한하고 보간한 값입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Spine Control")
	float SpinePitch = 0.0f;

	/** 허리 매핑 전의 현재 시점 Pitch입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="Spine Control")
	float CurrentViewPitch = 0.0f;

	UFUNCTION(BlueprintPure, Category="Components")
	UNPStablePhysicsMovementComponent* GetStablePhysicsMovementComponent() const { return PhysicsMovement; }

	UFUNCTION(BlueprintPure, Category="Components")
	UNPStablePhysicsGrabComponent* GetRightHandGrabComponent() const { return RightHandGrab; }

	UFUNCTION(BlueprintPure, Category="Animation")
	float GetAnimationGroundSpeed() const;

	/** 보정된 캐릭터 정면을 기준으로 계산한 블렌드 스페이스용 이동 각도입니다. */
	UFUNCTION(BlueprintPure, Category="Animation")
	float GetAnimationMovementDirection() const;

	UFUNCTION(BlueprintPure, Category="Animation")
	FVector GetVisualForwardDirection() const;

	/** 로컬 또는 복제된 시점의 수평 정면 방향입니다. */
	FVector GetViewForwardDirection() const;

	UFUNCTION(BlueprintPure, Category="Animation")
	FRotator GetVisualFacingRotation() const;

protected:
	/** 현재 실행 환경의 캐릭터 물리에 속도 변화를 직접 적용합니다. */
	void ApplyExternalVelocityChangeLocal(const FVector& VelocityChange);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 계산된 월드 이동 입력을 실제 Movement Component에 전달합니다. */
	virtual void ApplyMoveInput(const FVector& WorldMoveInput);

	/** 점프 입력을 실제 물리 점프 요청으로 전달합니다. */
	virtual void ApplyJumpRequest();

	/** 오른손의 시각 상태와 실제 그랩 요청을 함께 변경합니다. */
	virtual void ApplyRightHandState(bool bActive);

	/** 그랩 판정과 관계없이 오른손 IK에 사용할 시각 상태만 변경합니다. */
	void SetRightHandVisualState(bool bActive);
	void SetRightHandIKWorldTarget(const FVector& WorldTarget);
	void ClearRightHandIKWorldTarget();

	/** 캐릭터 방향, 손 IK와 허리에 사용할 시점 회전을 반환합니다. */
	virtual FRotator GetTargetViewRotation() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	USkeletalMeshComponent* PhysicsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	USceneComponent* CameraRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UPhysicalAnimationComponent* PhysicalAnimation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UPhysicsControlComponent* PhysicsControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UNPStablePhysicsGrabComponent* RightHandGrab;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UNPStablePhysicsMovementComponent* PhysicsMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UNPStablePhysicsDebugComponent* PhysicsDebug;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* RightHandAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Photo")
	TObjectPtr<UAnimMontage> PhotoShotMontage;

	/** 모든 플레이어가 촬영 위치를 기준으로 듣는 셔터음입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio|Photo")
	TObjectPtr<USoundBase> PhotoShutterSound;

	/** 셔터음의 거리 감쇠 설정입니다. 지정하지 않으면 사운드 에셋의 설정을 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio|Photo")
	TObjectPtr<USoundAttenuation> PhotoShutterAttenuation;

	/** 지정하면 본 이름과 피지컬 애니메이션 설정을 프로필 값으로 덮어씁니다. */
	UPROPERTY(EditAnywhere, Category="Character Profile")
	TObjectPtr<UNPStablePhysicsCharacterProfile> CharacterProfile;

	UPROPERTY(EditAnywhere, Category="Physical Animation")
	FName FullBodyRootName = TEXT("pelvis");

	UPROPERTY(EditAnywhere, Category="Physical Animation")
	bool bLockPelvisTilt = true;

	/** 프로필의 의미 기반 수치를 런타임 Physical Animation 설정으로 변환한 결과입니다. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Physical Animation", meta=(DisplayName="프로필 미사용 시 Body 그룹"))
	TArray<FStablePhysicalBodyGroupSettings> PhysicalBodyGroups;

	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.0"))
	float CameraTargetHeight = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Photo", meta=(ClampMin="0.0"))
	float PhotoCameraArmLength = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Photo", meta=(ClampMin="0.0"))
	float PhotoCameraTargetHeight = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Photo", meta=(ClampMin="1.0", ClampMax="179.0"))
	float PhotoCameraFOV = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Photo", meta=(ClampMin="0.1"))
	float PhotoCameraBlendSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Relic Aim", meta=(ClampMin="0.0"))
	float RelicAimCameraArmLength = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Relic Aim", meta=(ClampMin="0.0"))
	float RelicAimCameraTargetHeight = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera|Relic Aim", meta=(ClampMin="1.0", ClampMax="179.0"))
	float RelicAimCameraFOV = 60.0f;

#pragma region Pawn Debug Settings
	UPROPERTY(EditAnywhere, Category="Facing Debug")
	bool bDrawFacingDebug = true;

	UPROPERTY(EditAnywhere, Category="Facing Debug", meta=(ClampMin="0.0"))
	float FacingDebugHeight = 100.0f;

	UPROPERTY(EditAnywhere, Category="Facing Debug", meta=(ClampMin="1.0"))
	float FacingDebugArrowLength = 150.0f;
#pragma endregion

	/** 메시의 로컬 +X축과 실제 캐릭터 정면 사이의 각도를 보정합니다. */
	UPROPERTY(EditAnywhere, Category="Facing", meta=(ClampMin="-180.0", ClampMax="180.0"))
	float CharacterForwardYawOffset = 90.0f;

	UPROPERTY(EditAnywhere, Category="Animation", meta=(ClampMin="0.0"))
	float AnimationSpeedDeadZone = 15.0f;

	UPROPERTY(EditAnywhere, Category="Right Hand IK")
	FName RightShoulderBoneName = TEXT("clavicle_r");

	UPROPERTY(EditAnywhere, Category="Right Hand IK")
	FName RightUpperArmBoneName = TEXT("upperarm_r");

	UPROPERTY(EditAnywhere, Category="Right Hand IK")
	FName RightHandBoneName = TEXT("hand_r");

private:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayPhotoShutterSound(FVector_NetQuantize10 SoundLocation);

	void ApplyCharacterProfile();
	void RefreshCharacterProfileIfChanged();
	void InitializePhysicalAnimation();
	void ApplyPhysicalAnimationGroups();
	void ApplyRelicSwingPhysicalAnimation(
		const FNPRelicSwingSettings& Settings);
	void ConfigurePelvisStability();
	void UpdateCameraTarget();
	void UpdatePhotoCamera(float DeltaSeconds);
	void UpdateFacingTarget();

	void UpdateRightHandIK(float DeltaSeconds);
	void UpdateSpinePitch(float DeltaSeconds);
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Jump();
	void StartRightHand();
	void StopRightHand();

	bool bRightHandActive = false;
	bool bRelicSwingActive = false;
	bool bHasRightHandIKWorldTarget = false;
	FVector RightHandIKWorldTarget = FVector::ZeroVector;
	float RightHandReachDistance = 120.0f;
	float RightElbowOutwardDistance = 40.0f;
	float RightHandIKBlendSpeed = 10.0f;
	float MaxSpineBendAngle = 120.0f;
	float MaxSpineLeanBackAngle = 20.0f;
	float SpineBendStartViewPitch = -40.0f;
	float SpineLeanBackStartViewPitch = 10.0f;
	float SpinePitchInterpSpeed = 8.0f;
	bool bPhotoViewActive = false;
	bool bRelicAimViewActive = false;
	float DefaultCameraArmLength = 400.0f;
	float DefaultCameraFOV = 90.0f;
	float CurrentCameraTargetHeight = 60.0f;
	uint32 AppliedCharacterProfileRevision = 0;
	TWeakObjectPtr<UNPStablePhysicsCharacterProfile> AppliedCharacterProfile;
};
