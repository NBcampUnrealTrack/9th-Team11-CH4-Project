#include "Gameplay/Photo/Abilities/NPPhotoShotAbility.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Core/Room/NPRoomPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/AbilitySystem/Effects/NPPhotoCooldownGameplayEffect.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Gameplay/Photo/NPPhotoLog.h"

UNPPhotoShotAbility::UNPPhotoShotAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	CooldownGameplayEffectClass =
		UNPPhotoCooldownGameplayEffect::StaticClass();

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Photo_Shot);
	Tags.AddTag(NPGameplayTags::Ability_Photo);
	Tags.AddTag(NPGameplayTags::Ability_Photo_Shot);
	SetAssetTags(Tags);

	ActivationRequiredTags.AddTag(NPGameplayTags::State_Photo_Aiming);
	ActivationBlockedTags.AddTag(
		NPGameplayTags::State_CrowdControl_Stunned);
	ActivationBlockedTags.AddTag(NPGameplayTags::Cooldown_Photo_Shot);
}

bool UNPPhotoShotAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	const ANPMainPlayerController* PlayerController = Pawn
		? Cast<ANPMainPlayerController>(Pawn->GetController())
		: nullptr;
	const ANPRoomPlayerController* RoomPlayerController = Pawn
		? Cast<ANPRoomPlayerController>(Pawn->GetController())
		: nullptr;
	const UNPPhotoCaptureComponent* PhotoCapture = nullptr;
	if (PlayerController)
	{
		PhotoCapture = PlayerController->GetPhotoCaptureComponent();
	}
	else if (RoomPlayerController)
	{
		PhotoCapture = RoomPlayerController->GetPhotoCaptureComponent();
	}
	if (!PhotoCapture)
	{
		return false;
	}

	return !ActorInfo->IsLocallyControlled()
		|| PhotoCapture->CanTakePhotoLocally();
}

void UNPPhotoShotAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData);

	if (!ActorInfo || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bool bPhotoStarted = true;
	if (ActorInfo->IsLocallyControlled())
	{
		const APawn* Pawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
		ANPMainPlayerController* PlayerController = Pawn
			? Cast<ANPMainPlayerController>(Pawn->GetController())
			: nullptr;
		ANPRoomPlayerController* RoomPlayerController = Pawn
			? Cast<ANPRoomPlayerController>(Pawn->GetController())
			: nullptr;
		UNPPhotoCaptureComponent* PhotoCapture = nullptr;
		if (PlayerController)
		{
			PhotoCapture = PlayerController->GetPhotoCaptureComponent();
		}
		else if (RoomPlayerController)
		{
			PhotoCapture = RoomPlayerController->GetPhotoCaptureComponent();
		}
		bPhotoStarted = PhotoCapture && PhotoCapture->TakePhoto();
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[PhotoAbility] Photo shot result=%s Controller=%s"),
			bPhotoStarted ? TEXT("success") : TEXT("failed"),
			*GetNameSafe(PlayerController));
	}

	if (ActorInfo->IsNetAuthority())
	{
		const APawn* Pawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
		if (ANPRoomPlayerController* RoomPlayerController = Pawn
			? Cast<ANPRoomPlayerController>(Pawn->GetController())
			: nullptr)
		{
			RoomPlayerController->PlayPresentationOnlyShutterCue();
		}
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		!bPhotoStarted);
}
