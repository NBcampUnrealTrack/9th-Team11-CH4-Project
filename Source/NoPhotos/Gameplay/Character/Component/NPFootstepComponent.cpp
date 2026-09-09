#include "Gameplay/Character/Component/NPFootstepComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Audio/NPSoundSubsystem.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"
#include "Sound/SoundBase.h"

UNPFootstepComponent::UNPFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPFootstepComponent::UpdateFootsteps(UAnimInstance* AnimInstance)
{
	if (!IsValid(AnimInstance) || SyncGroupName.IsNone() || !CanPlayFootstep())
	{
		return;
	}

	USkeletalMeshComponent* Mesh = AnimInstance->GetSkelMeshComponent();
	if (!IsValid(Mesh) || Mesh->GetOwner() != GetOwner())
	{
		return;
	}

	if (!LeftFootMarkerName.IsNone()
		&& AnimInstance->HasMarkerBeenHitThisFrame(SyncGroupName, LeftFootMarkerName))
	{
		PlayFootstep(Mesh, LeftFootBoneName);
	}
	if (!RightFootMarkerName.IsNone()
		&& AnimInstance->HasMarkerBeenHitThisFrame(SyncGroupName, RightFootMarkerName))
	{
		PlayFootstep(Mesh, RightFootBoneName);
	}
}

bool UNPFootstepComponent::CanPlayFootstep() const
{
	const ANPStablePhysicsPawn* Pawn = Cast<ANPStablePhysicsPawn>(GetOwner());
	if (!IsValid(Pawn) || Pawn->GetNetMode() == NM_DedicatedServer
		|| FootstepSounds.IsEmpty() || Pawn->IsTemporaryRagdollOrRecovering()
		|| Pawn->GetAnimationGroundSpeed() <= 0.0f)
	{
		return false;
	}

	const UNPStablePhysicsMovementComponent* Movement = Pawn->GetStablePhysicsMovementComponent();
	return IsValid(Movement) && !Movement->GetIsFalling();
}

void UNPFootstepComponent::PlayFootstep(USkeletalMeshComponent* Mesh, FName FootBoneName)
{
	if (FootBoneName.IsNone() || Mesh->GetBoneIndex(FootBoneName) == INDEX_NONE)
	{
		return;
	}

	const ANPStablePhysicsPawn* Pawn = Cast<ANPStablePhysicsPawn>(GetOwner());
	if (UNPSoundSubsystem* SoundSubsystem = UNPSoundSubsystem::Get(this))
	{
		TArray<USoundBase*> ValidSounds;
		for (const TObjectPtr<USoundBase>& Sound : FootstepSounds)
		{
			if (IsValid(Sound))
			{
				ValidSounds.AddUnique(Sound.Get());
			}
		}
		if (ValidSounds.IsEmpty())
		{
			return;
		}

		if (ValidSounds.Num() > 1)
		{
			ValidSounds.Remove(LastFootstepSound.Get());
		}

		USoundBase* FootstepSound = ValidSounds[FMath::RandRange(0, ValidSounds.Num() - 1)];
		const float Volume = Pawn->IsLocallyControlled() ? LocalFootstepVolume : RemoteFootstepVolume;
		const float SafeMinPitch = FMath::IsFinite(MinPitch) ? FMath::Max(0.01f, MinPitch) : 1.0f;
		const float SafeMaxPitch = FMath::IsFinite(MaxPitch) ? FMath::Max(SafeMinPitch, MaxPitch) : SafeMinPitch;
		const float Pitch = FMath::FRandRange(SafeMinPitch, SafeMaxPitch);
		SoundSubsystem->PlaySFXAtLocation(FootstepSound, Mesh->GetSocketLocation(FootBoneName),
			FRotator::ZeroRotator, Volume, Pitch, 0.0f, FootstepAttenuation);
		LastFootstepSound = FootstepSound;
	}
}
