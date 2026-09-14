#include "Gameplay/Photo/Abilities/NPPhotoAimAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Core/Main/NPMainPlayerController.h"
#include "Core/Room/NPRoomPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"

UNPPhotoAimAbility::UNPPhotoAimAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(NPGameplayTags::Input_Photo_Aim);
	Tags.AddTag(NPGameplayTags::Ability_Photo);
	Tags.AddTag(NPGameplayTags::Ability_Photo_Aim);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(NPGameplayTags::State_Photo_Aiming);
	ActivationBlockedTags.AddTag(NPGameplayTags::State_Relic_Aiming);
	ActivationBlockedTags.AddTag(
		NPGameplayTags::State_CrowdControl_Stunned);
}

bool UNPPhotoAimAbility::CanActivateAbility(
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
	const UNPStablePhysicsGrabComponent* GrabComponent = Pawn
		? Pawn->FindComponentByClass<UNPStablePhysicsGrabComponent>()
		: nullptr;
	const UNPPhotoCaptureComponent* PhotoCapture =
		ResolvePhotoCaptureComponent(ActorInfo);
	return Pawn
		&& PhotoCapture
		&& !PhotoCapture->IsPhotoModeActive()
		&& (!GrabComponent || !GrabComponent->IsHoldingObject());
}

void UNPPhotoAimAbility::ActivateAbility(
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

	UNPPhotoCaptureComponent* PhotoCapture =
		ResolvePhotoCaptureComponent(ActorInfo);
	if (!PhotoCapture || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsLocallyControlled())
	{
		if (!PhotoCapture->EnterPhotoMode())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		ActivePhotoCaptureComponent = PhotoCapture;
	}

	// LocalPredicted Ability의 Prediction Key가 촬영자 클라이언트에서
	// 서버 Multicast Cue의 중복 재생을 제거합니다. 로컬에서도 즉시 실행해야
	// 촬영자 클라이언트가 준비음을 들을 수 있습니다.
	APawn* Pawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();
	if (Pawn && AbilitySystem)
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Pawn->GetActorLocation();
		CueParameters.Normal = Pawn->GetActorForwardVector();
		CueParameters.Instigator = Pawn;
		CueParameters.EffectCauser = Pawn;
		AbilitySystem->ExecuteGameplayCue(
			NPGameplayTags::GameplayCue_Photo_AimStart,
			CueParameters);
	}
}

void UNPPhotoAimAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (ActivePhotoCaptureComponent.IsValid())
	{
		ActivePhotoCaptureComponent->ExitPhotoMode();
	}
	ActivePhotoCaptureComponent.Reset();

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

UNPPhotoCaptureComponent* UNPPhotoAimAbility::ResolvePhotoCaptureComponent(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const APawn* Pawn = ActorInfo
		? Cast<APawn>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const ANPMainPlayerController* PlayerController = Pawn
		? Cast<ANPMainPlayerController>(Pawn->GetController())
		: nullptr;
	if (PlayerController)
	{
		return PlayerController->GetPhotoCaptureComponent();
	}

	const ANPRoomPlayerController* RoomPlayerController = Pawn
		? Cast<ANPRoomPlayerController>(Pawn->GetController())
		: nullptr;
	return RoomPlayerController
		? RoomPlayerController->GetPhotoCaptureComponent()
		: nullptr;
}
