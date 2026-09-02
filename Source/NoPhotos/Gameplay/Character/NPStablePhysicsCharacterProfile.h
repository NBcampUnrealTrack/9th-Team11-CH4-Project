#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NPStablePhysicsCharacterProfile.generated.h"

class USkeletalMesh;
class USkeleton;

UENUM(BlueprintType)
enum class EStablePhysicsFeelPreset : uint8
{
	HumanFallFlat UMETA(DisplayName="Human: Fall Flat 느낌"),
	Stable UMETA(DisplayName="안정적"),
	Floppy UMETA(DisplayName="흐물흐물"),
	Custom UMETA(DisplayName="사용자 설정")
};

/** 지정한 본과 그 하위 본에 적용할 피지컬 애니메이션 설정입니다. */
USTRUCT(BlueprintType)
struct FStablePhysicalBodyGroupSettings
{
	GENERATED_BODY()

	FStablePhysicalBodyGroupSettings() = default;

	FStablePhysicalBodyGroupSettings(
		FName InRootBodyName,
		float InOrientationStrength,
		float InAngularVelocityStrength,
		float InMaxAngularForce)
		: RootBodyName(InRootBodyName)
		, OrientationStrength(InOrientationStrength)
		, AngularVelocityStrength(InAngularVelocityStrength)
		, MaxAngularForce(InMaxAngularForce)
	{
	}

	UPROPERTY(EditAnywhere, Category="Physical Animation")
	FName RootBodyName = NAME_None;

	UPROPERTY(EditAnywhere, Category="Physical Animation", meta=(ClampMin="0.0"))
	float OrientationStrength = 500.0f;

	UPROPERTY(EditAnywhere, Category="Physical Animation", meta=(ClampMin="0.0"))
	float AngularVelocityStrength = 50.0f;

	UPROPERTY(EditAnywhere, Category="Physical Animation", meta=(ClampMin="0.0"))
	float MaxAngularForce = 0.0f;
};

/** 스켈레톤별 본 매핑과 피지컬 애니메이션 강도를 보관하는 프로필입니다. */
UCLASS(BlueprintType)
class NOPHOTOS_API UNPStablePhysicsCharacterProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UNPStablePhysicsCharacterProfile();
	virtual void PostLoad() override;

	/** 지정한 Skeletal Mesh의 본 이름을 분석해 본 매핑을 다시 생성합니다. */
	UFUNCTION(CallInEditor, Category="캐릭터 설정", meta=(DisplayName="본 자동 매핑 다시 생성"))
	void GenerateFromSkeleton();

	const TArray<FStablePhysicalBodyGroupSettings>& GetPhysicalBodyGroups() const
	{
		return PhysicalBodyGroups;
	}
	uint32 GetSettingsRevision() const { return SettingsRevision; }

	/** 본 탐색과 Physics Asset 검증에 사용할 메시입니다. Pawn의 메시를 직접 변경하지는 않습니다. */
	UPROPERTY(EditAnywhere, Category="캐릭터 설정", meta=(DisplayName="Skeletal Mesh"))
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(VisibleAnywhere, Category="캐릭터 설정", meta=(DisplayName="물리 Body 매핑"))
	FString PhysicalBodyMappingStatus;

	/** 메시의 로컬 +X축과 실제 캐릭터 정면 사이의 각도입니다. */
	UPROPERTY(EditAnywhere, Category="캐릭터 설정", meta=(DisplayName="캐릭터 정면 보정", ClampMin="-180.0", ClampMax="180.0"))
	float CharacterForwardYawOffset = 90.0f;

	UPROPERTY(EditAnywhere, Category="조작|회전", meta=(DisplayName="회전 강도", ClampMin="0.0"))
	float FacingAngularStrength = 3.0f;

	UPROPERTY(EditAnywhere, Category="조작|회전", meta=(DisplayName="회전 감쇠비", ClampMin="0.0"))
	float FacingAngularDampingRatio = 1.0f;

	UPROPERTY(EditAnywhere, Category="조작|회전", meta=(DisplayName="최대 회전 토크", ClampMin="0.0"))
	float MaxFacingTorque = 750000.0f;

	UPROPERTY(EditAnywhere, Category="조작|회전", meta=(DisplayName="최대 목표 회전 속도", ClampMin="0.0"))
	float MaxFacingTargetSpeed = 180.0f;

	// 지면에서 pelvis까지 유지할 높이입니다.
	UPROPERTY(EditAnywhere, Category="캐릭터 설정", meta=(DisplayName="허리 높이", ClampMin="0.0", Units="cm"))
	float PelvisHeight = 100.0f;

	UPROPERTY(EditAnywhere, Category="이동", meta=(DisplayName="최대 이동 속도", ClampMin="0.0"))
	float MaxMoveSpeed = 350.0f;

	UPROPERTY(EditAnywhere, Category="이동", meta=(DisplayName="사다리 상승 가속도", ClampMin="0.0", Units="cm/s^2"))
	float ClimbAcceleration = 2000.0f;

	/** 월드 중력에 적용할 캐릭터 전용 배율입니다. */
	UPROPERTY(EditAnywhere, Category="이동", meta=(DisplayName="중력 배율", ClampMin="0.0"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, Category="이동", meta=(DisplayName="점프 속도 변화량", ClampMin="0.0"))
	float JumpVelocityChange = 350.0f;

	UPROPERTY(EditAnywhere, Category="이동", meta=(DisplayName="점프 쿨다운", ClampMin="0.0", Units="s"))
	float JumpCooldown = 0.2f;

	UPROPERTY(EditAnywhere, Category="이동", meta=(DisplayName="걸을 수 있는 최대 경사각", ClampMin="0.0", ClampMax="90.0", Units="deg"))
	float WalkableSlopeAngle = 45.0f;

	UPROPERTY(EditAnywhere, Category="잡기", meta=(DisplayName="Grab 선형 힘 파괴 임계값", ClampMin="0.0"))
	float GrabLinearBreakThreshold = 200000.0f;

	UPROPERTY(EditAnywhere, Category="잡기", meta=(DisplayName="복제 Grab Frame 보간 시간", ClampMin="0.0", Units="s"))
	float ReplicatedGrabFrameBlendDuration = 0.15f;

	UPROPERTY(EditAnywhere, Category="잡기", meta=(DisplayName="Grab 파괴 후 재시도 대기시간", ClampMin="0.0", Units="s"))
	float GrabRetryCooldown = 0.5f;

	UPROPERTY(EditAnywhere, Category="조작|팔", meta=(DisplayName="손 뻗기 거리", ClampMin="0.0", Units="cm"))
	float RightHandReachDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category="조작|팔", meta=(DisplayName="팔꿈치 바깥 거리", ClampMin="0.0", Units="cm"))
	float RightElbowOutwardDistance = 40.0f;

	UPROPERTY(EditAnywhere, Category="조작|팔", meta=(DisplayName="오른손 IK 전환 속도", ClampMin="0.0"))
	float RightHandIKBlendSpeed = 10.0f;

	UPROPERTY(EditAnywhere, Category="조작|허리", meta=(DisplayName="최대 숙임 각도", ClampMin="0.0", ClampMax="150.0"))
	float MaxSpineBendAngle = 120.0f;

	UPROPERTY(EditAnywhere, Category="조작|허리", meta=(DisplayName="최대 젖힘 각도", ClampMin="0.0", ClampMax="90.0"))
	float MaxSpineLeanBackAngle = 20.0f;

	UPROPERTY(EditAnywhere, Category="조작|허리", meta=(DisplayName="숙이기 시작 Pitch", ClampMin="-89.9", ClampMax="89.9"))
	float SpineBendStartViewPitch = -40.0f;

	UPROPERTY(EditAnywhere, Category="조작|허리", meta=(DisplayName="젖히기 시작 Pitch", ClampMin="-89.9", ClampMax="89.9"))
	float SpineLeanBackStartViewPitch = 10.0f;

	UPROPERTY(EditAnywhere, Category="조작|허리", meta=(DisplayName="Pitch 반응 속도", ClampMin="0.0"))
	float SpinePitchInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, Category="물리 느낌", meta=(DisplayName="프리셋"))
	EStablePhysicsFeelPreset Preset = EStablePhysicsFeelPreset::HumanFallFlat;

	UPROPERTY(EditAnywhere, Category="물리 느낌", meta=(DisplayName="하체 단단함", ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float LowerBodyRigidity = 90.0f;

	UPROPERTY(EditAnywhere, Category="물리 느낌", meta=(DisplayName="몸통 단단함", ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float TorsoRigidity = 40.0f;

	UPROPERTY(EditAnywhere, Category="물리 느낌", meta=(DisplayName="목 / 머리 단단함", ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float HeadRigidity = 35.0f;

	UPROPERTY(EditAnywhere, Category="물리 느낌", meta=(DisplayName="팔 단단함", ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float ArmRigidity = 25.0f;

	UPROPERTY(EditAnywhere, Category="물리 느낌", meta=(DisplayName="손 단단함", ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float HandRigidity = 10.0f;

	UPROPERTY(EditAnywhere, Category="물리 느낌", meta=(DisplayName="전체 흔들림 감쇠", ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float GlobalDamping = 55.0f;

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Pelvis"))
	FName PelvisBoneName = TEXT("pelvis");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Left Foot"))
	FName LeftFootBoneName = TEXT("foot_l");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Right Foot"))
	FName RightFootBoneName = TEXT("foot_r");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Right Shoulder"))
	FName RightShoulderBoneName = TEXT("clavicle_r");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Right Hand"))
	FName RightHandBoneName = TEXT("hand_r");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Left Thigh"))
	FName LeftThighBoneName = TEXT("thigh_l");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Right Thigh"))
	FName RightThighBoneName = TEXT("thigh_r");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Spine"))
	FName SpineBoneName = TEXT("spine_02");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Neck"))
	FName NeckBoneName = TEXT("neck_01");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Left Upper Arm"))
	FName LeftUpperArmBoneName = TEXT("upperarm_l");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Right Upper Arm"))
	FName RightUpperArmBoneName = TEXT("upperarm_r");

	UPROPERTY(EditAnywhere, Category="고급|본 매핑", AdvancedDisplay, meta=(DisplayName="Left Hand"))
	FName LeftHandBoneName = TEXT("hand_l");

	UPROPERTY(EditAnywhere, Category="디버그", meta=(DisplayName="PIE에서 영역 색상 표시"))
	bool bDrawPhysicalRegionDebug = true;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void ApplyPresetValues();
	void RebuildPhysicalBodyGroups();
	void RestoreBoneMappingsFromPhysicalGroups();

	/** 이전 버전 프로필의 Skeleton 참조를 읽기 위해 직렬화만 유지합니다. */
	UPROPERTY()
	TObjectPtr<USkeleton> Skeleton;

	/** 사용자가 조정한 의미 기반 수치를 런타임 Physical Animation 값으로 변환한 결과입니다. */
	UPROPERTY()
	TArray<FStablePhysicalBodyGroupSettings> PhysicalBodyGroups;

	uint32 SettingsRevision = 0;
};
