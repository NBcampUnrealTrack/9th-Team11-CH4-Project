#include "Gameplay/Photo/Abilities/NPPhotoShotAbility.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "Core/Main/NPMainPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Gameplay/Photo/NPPhotoLog.h"

UNPPhotoShotAbility::UNPPhotoShotAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Photo_Shot);
	Tags.AddTag(NPGameplayTags::Ability_Photo);
	Tags.AddTag(NPGameplayTags::Ability_Photo_Shot);
	SetAssetTags(Tags);

	ActivationRequiredTags.AddTag(NPGameplayTags::State_Photo_Aiming);
	ActivationBlockedTags.AddTag(
		NPGameplayTags::State_CrowdControl_Stunned);
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
	const UNPPhotoCaptureComponent* PhotoCapture = PlayerController
		? PlayerController->GetPhotoCaptureComponent()
		: nullptr;
	return PhotoCapture != nullptr;
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
		UNPPhotoCaptureComponent* PhotoCapture = PlayerController
			? PlayerController->GetPhotoCaptureComponent()
			: nullptr;
		bPhotoStarted = PhotoCapture && PhotoCapture->TakePhoto();
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[PhotoAbility] Photo shot result=%s Controller=%s"),
			bPhotoStarted ? TEXT("success") : TEXT("failed"),
			*GetNameSafe(PlayerController));
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		!bPhotoStarted);
}
