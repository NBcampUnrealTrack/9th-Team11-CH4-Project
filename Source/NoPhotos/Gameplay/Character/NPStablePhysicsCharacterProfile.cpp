#include "Gameplay/Character/NPStablePhysicsCharacterProfile.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ReferenceSkeleton.h"
#include "UObject/UnrealType.h"

namespace StablePhysicsProfile
{
	constexpr int32 ExpectedPhysicalBodyCount = 9;

	constexpr float HumanLowerBodyRigidity = 90.0f;
	constexpr float HumanTorsoRigidity = 40.0f;
	constexpr float HumanHeadRigidity = 35.0f;
	constexpr float HumanArmRigidity = 25.0f;
	constexpr float HumanHandRigidity = 10.0f;
	constexpr float HumanGlobalDamping = 55.0f;

	FString NormalizeBoneName(FName BoneName)
	{
		FString NormalizedName = BoneName.ToString().ToLower();
		NormalizedName.ReplaceInline(TEXT("_"), TEXT(""));
		NormalizedName.ReplaceInline(TEXT("-"), TEXT(""));
		NormalizedName.ReplaceInline(TEXT(" "), TEXT(""));
		NormalizedName.ReplaceInline(TEXT("."), TEXT(""));
		return NormalizedName;
	}

	FName FindBoneByAliases(
		const FReferenceSkeleton& ReferenceSkeleton,
		const TArray<FString>& Aliases)
	{
		// IK나 보조 본의 접미사가 먼저 일치하지 않도록 정확한 이름을 우선합니다.
		for (const FString& Alias : Aliases)
		{
			for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetRawBoneNum(); ++BoneIndex)
			{
				const FName BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
				const FString NormalizedName = NormalizeBoneName(BoneName);
				if (NormalizedName == Alias)
				{
					return BoneName;
				}
			}
		}

		for (const FString& Alias : Aliases)
		{
			for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetRawBoneNum(); ++BoneIndex)
			{
				const FName BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
				const FString NormalizedName = NormalizeBoneName(BoneName);
				const bool bLooksLikeHelperBone = NormalizedName.StartsWith(TEXT("ik"))
					|| NormalizedName.Contains(TEXT("twist"))
					|| NormalizedName.Contains(TEXT("helper"))
					|| NormalizedName.Contains(TEXT("virtual"));
				if (!bLooksLikeHelperBone && NormalizedName.EndsWith(Alias))
				{
					return BoneName;
				}
			}
		}

		// 일반 본을 찾지 못했을 때만 보조 본까지 마지막 후보로 허용합니다.
		for (const FString& Alias : Aliases)
		{
			for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetRawBoneNum(); ++BoneIndex)
			{
				const FName BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
				if (NormalizeBoneName(BoneName).EndsWith(Alias))
				{
					return BoneName;
				}
			}
		}

		return NAME_None;
	}

	float GetRigidityScale(float Rigidity, float ReferenceRigidity)
	{
		return FMath::Clamp(Rigidity, 0.0f, 100.0f) / ReferenceRigidity;
	}

	bool HasPhysicalBody(const USkeletalMesh* SkeletalMesh, FName BoneName)
	{
		if (!SkeletalMesh)
		{
			return true;
		}

		const UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
		return PhysicsAsset && PhysicsAsset->FindBodyIndex(BoneName) != INDEX_NONE;
	}

	void AddPhysicalGroup(
		TArray<FStablePhysicalBodyGroupSettings>& Groups,
		FName BoneName,
		float BaseOrientationStrength,
		float BaseAngularVelocityStrength,
		float Rigidity,
		float ReferenceRigidity,
		float GlobalDamping)
	{
		if (BoneName.IsNone())
		{
			return;
		}

		const float RigidityScale = GetRigidityScale(Rigidity, ReferenceRigidity);
		const float DampingScale = FMath::Clamp(GlobalDamping, 0.0f, 100.0f)
			/ HumanGlobalDamping;
		Groups.Emplace(
			BoneName,
			BaseOrientationStrength * RigidityScale,
			BaseAngularVelocityStrength * DampingScale,
			0.0f);
	}

	void AddMissingBodyLabel(
		TArray<FString>& MissingBodyLabels,
		const USkeletalMesh* SkeletalMesh,
		FName BoneName,
		const TCHAR* Label)
	{
		if (!HasPhysicalBody(SkeletalMesh, BoneName))
		{
			MissingBodyLabels.Emplace(Label);
		}
	}
}

UNPStablePhysicsCharacterProfile::UNPStablePhysicsCharacterProfile()
{
	RebuildPhysicalBodyGroups();
}

void UNPStablePhysicsCharacterProfile::PostLoad()
{
	Super::PostLoad();

	// 이전 버전 프로필만 기존 배열의 본 이름을 새 고정 필드로 옮깁니다.
	if (!SkeletalMesh && Skeleton)
	{
		RestoreBoneMappingsFromPhysicalGroups();
	}
}

void UNPStablePhysicsCharacterProfile::GenerateFromSkeleton()
{
#if WITH_EDITOR
	Modify();
#endif

	const FReferenceSkeleton* ReferenceSkeleton = nullptr;
	if (SkeletalMesh)
	{
		Skeleton = SkeletalMesh->GetSkeleton();
		ReferenceSkeleton = &SkeletalMesh->GetRefSkeleton();
	}
	else if (Skeleton)
	{
		ReferenceSkeleton = &Skeleton->GetReferenceSkeleton();
	}

	if (!ReferenceSkeleton)
	{
		PhysicalBodyMappingStatus = TEXT("Skeletal Mesh 미지정");
		return;
	}

	PelvisBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("pelvis"), TEXT("hips"), TEXT("hip")});
	LeftFootBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("footl"), TEXT("lfoot"), TEXT("leftfoot")});
	RightFootBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("footr"), TEXT("rfoot"), TEXT("rightfoot")});
	RightShoulderBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("clavicler"), TEXT("rclavicle"), TEXT("rightclavicle"), TEXT("shoulderr"), TEXT("rshoulder")});
	RightHandBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("handr"), TEXT("rhand"), TEXT("righthand")});
	LeftThighBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("thighl"), TEXT("lthigh"), TEXT("leftthigh"), TEXT("lupleg"), TEXT("leftupleg")});
	RightThighBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("thighr"), TEXT("rthigh"), TEXT("rightthigh"), TEXT("rupleg"), TEXT("rightupleg")});
	SpineBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("spine02"), TEXT("spine2"), TEXT("upperchest"), TEXT("chest"), TEXT("spine01"), TEXT("spine1"), TEXT("spine")});
	NeckBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("neck01"), TEXT("neck1"), TEXT("neck")});
	LeftUpperArmBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("upperarml"), TEXT("lupperarm"), TEXT("leftupperarm"), TEXT("arml"), TEXT("leftarm")});
	RightUpperArmBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("upperarmr"), TEXT("rupperarm"), TEXT("rightupperarm"), TEXT("armr"), TEXT("rightarm")});
	LeftHandBoneName = StablePhysicsProfile::FindBoneByAliases(
		*ReferenceSkeleton,
		{TEXT("handl"), TEXT("lhand"), TEXT("lefthand")});

	RebuildPhysicalBodyGroups();

#if WITH_EDITOR
	MarkPackageDirty();
#endif
}

void UNPStablePhysicsCharacterProfile::ApplyPresetValues()
{
	switch (Preset)
	{
	case EStablePhysicsFeelPreset::HumanFallFlat:
		LowerBodyRigidity = StablePhysicsProfile::HumanLowerBodyRigidity;
		TorsoRigidity = StablePhysicsProfile::HumanTorsoRigidity;
		HeadRigidity = StablePhysicsProfile::HumanHeadRigidity;
		ArmRigidity = StablePhysicsProfile::HumanArmRigidity;
		HandRigidity = StablePhysicsProfile::HumanHandRigidity;
		GlobalDamping = StablePhysicsProfile::HumanGlobalDamping;
		break;

	case EStablePhysicsFeelPreset::Stable:
		LowerBodyRigidity = 100.0f;
		TorsoRigidity = 80.0f;
		HeadRigidity = 65.0f;
		ArmRigidity = 50.0f;
		HandRigidity = 30.0f;
		GlobalDamping = 80.0f;
		break;

	case EStablePhysicsFeelPreset::Floppy:
		LowerBodyRigidity = 45.0f;
		TorsoRigidity = 25.0f;
		HeadRigidity = 20.0f;
		ArmRigidity = 10.0f;
		HandRigidity = 5.0f;
		GlobalDamping = 30.0f;
		break;

	case EStablePhysicsFeelPreset::Custom:
	default:
		break;
	}
}

void UNPStablePhysicsCharacterProfile::RebuildPhysicalBodyGroups()
{
	++SettingsRevision;
	PhysicalBodyGroups.Reset(StablePhysicsProfile::ExpectedPhysicalBodyCount);

	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, PelvisBoneName,
		2500.0f, 300.0f, LowerBodyRigidity,
		StablePhysicsProfile::HumanLowerBodyRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, LeftThighBoneName,
		6000.0f, 700.0f, LowerBodyRigidity,
		StablePhysicsProfile::HumanLowerBodyRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, RightThighBoneName,
		6000.0f, 700.0f, LowerBodyRigidity,
		StablePhysicsProfile::HumanLowerBodyRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, SpineBoneName,
		700.0f, 100.0f, TorsoRigidity,
		StablePhysicsProfile::HumanTorsoRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, NeckBoneName,
		350.0f, 50.0f, HeadRigidity,
		StablePhysicsProfile::HumanHeadRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, LeftUpperArmBoneName,
		250.0f, 35.0f, ArmRigidity,
		StablePhysicsProfile::HumanArmRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, RightUpperArmBoneName,
		250.0f, 35.0f, ArmRigidity,
		StablePhysicsProfile::HumanArmRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, LeftHandBoneName,
		80.0f, 15.0f, HandRigidity,
		StablePhysicsProfile::HumanHandRigidity, GlobalDamping);
	StablePhysicsProfile::AddPhysicalGroup(
		PhysicalBodyGroups, RightHandBoneName,
		80.0f, 15.0f, HandRigidity,
		StablePhysicsProfile::HumanHandRigidity, GlobalDamping);

	if (SkeletalMesh && !SkeletalMesh->GetPhysicsAsset())
	{
		PhysicalBodyMappingStatus = TEXT("Physics Asset 없음");
	}
	else if (SkeletalMesh)
	{
		TArray<FString> MissingBodyLabels;
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, PelvisBoneName, TEXT("Pelvis"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, LeftThighBoneName, TEXT("Left Thigh"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, RightThighBoneName, TEXT("Right Thigh"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, SpineBoneName, TEXT("Spine"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, NeckBoneName, TEXT("Neck"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, LeftUpperArmBoneName, TEXT("Left Arm"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, RightUpperArmBoneName, TEXT("Right Arm"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, LeftHandBoneName, TEXT("Left Hand"));
		StablePhysicsProfile::AddMissingBodyLabel(MissingBodyLabels, SkeletalMesh, RightHandBoneName, TEXT("Right Hand"));

		const int32 ValidBodyCount = StablePhysicsProfile::ExpectedPhysicalBodyCount
			- MissingBodyLabels.Num();
		PhysicalBodyMappingStatus = MissingBodyLabels.IsEmpty()
			? FString::Printf(TEXT("%d / %d Body"), ValidBodyCount, StablePhysicsProfile::ExpectedPhysicalBodyCount)
			: FString::Printf(
				TEXT("%d / %d Body · 누락: %s"),
				ValidBodyCount,
				StablePhysicsProfile::ExpectedPhysicalBodyCount,
				*FString::Join(MissingBodyLabels, TEXT(", ")));
	}
	else if (Skeleton)
	{
		PhysicalBodyMappingStatus = FString::Printf(
			TEXT("%d개 그룹 · Physics Asset 미검증"),
			PhysicalBodyGroups.Num());
	}
	else
	{
		PhysicalBodyMappingStatus = TEXT("Skeletal Mesh 미지정");
	}
}

void UNPStablePhysicsCharacterProfile::RestoreBoneMappingsFromPhysicalGroups()
{
	if (PhysicalBodyGroups.Num() < StablePhysicsProfile::ExpectedPhysicalBodyCount)
	{
		return;
	}

	PelvisBoneName = PhysicalBodyGroups[0].RootBodyName;
	LeftThighBoneName = PhysicalBodyGroups[1].RootBodyName;
	RightThighBoneName = PhysicalBodyGroups[2].RootBodyName;
	SpineBoneName = PhysicalBodyGroups[3].RootBodyName;
	NeckBoneName = PhysicalBodyGroups[4].RootBodyName;
	LeftUpperArmBoneName = PhysicalBodyGroups[5].RootBodyName;
	RightUpperArmBoneName = PhysicalBodyGroups[6].RootBodyName;
	LeftHandBoneName = PhysicalBodyGroups[7].RootBodyName;
	RightHandBoneName = PhysicalBodyGroups[8].RootBodyName;
}

#if WITH_EDITOR
void UNPStablePhysicsCharacterProfile::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, SkeletalMesh))
	{
		Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
		GenerateFromSkeleton();
		return;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, Preset))
	{
		ApplyPresetValues();
		RebuildPhysicalBodyGroups();
		return;
	}

	const bool bFeelValueChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, LowerBodyRigidity)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, TorsoRigidity)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, HeadRigidity)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, ArmRigidity)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, HandRigidity)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, GlobalDamping);
	if (bFeelValueChanged)
	{
		Preset = EStablePhysicsFeelPreset::Custom;
		RebuildPhysicalBodyGroups();
		return;
	}

	const bool bBoneMappingChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, PelvisBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, LeftFootBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, RightFootBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, RightShoulderBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, LeftThighBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, RightThighBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, SpineBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, NeckBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, LeftUpperArmBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, RightUpperArmBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, LeftHandBoneName)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UNPStablePhysicsCharacterProfile, RightHandBoneName);
	if (bBoneMappingChanged)
	{
		RebuildPhysicalBodyGroups();
		return;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(
		UNPStablePhysicsCharacterProfile,
		CharacterForwardYawOffset)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			FacingAngularStrength)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			FacingAngularDampingRatio)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			MaxFacingTorque)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			MaxFacingTargetSpeed)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			PelvisHeight)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			MaxMoveSpeed)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			GravityScale)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			JumpVelocityChange)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			JumpCooldown)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			WalkableSlopeAngle)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			GrabLinearBreakThreshold)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			ReplicatedGrabFrameBlendDuration)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			GrabRetryCooldown)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			RightHandReachDistance)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			RightElbowOutwardDistance)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			RightHandIKBlendSpeed)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			MaxSpineBendAngle)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			MaxSpineLeanBackAngle)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			SpineBendStartViewPitch)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			SpineLeanBackStartViewPitch)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(
			UNPStablePhysicsCharacterProfile,
			SpinePitchInterpSpeed))
	{
		++SettingsRevision;
	}
}
#endif
