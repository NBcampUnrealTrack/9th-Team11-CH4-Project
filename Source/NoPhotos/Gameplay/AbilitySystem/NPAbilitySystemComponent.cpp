#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "GameplayAbilitySpec.h"
#include "Gameplay/Character/Abilities/NPScanAbility.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Photo/Abilities/NPPhotoAimAbility.h"
#include "Gameplay/Photo/Abilities/NPPhotoShotAbility.h"
#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "GameplayEffect.h"
#include "InputAction.h"

UNPAbilitySystemComponent::UNPAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	ScanAbilityClass = UNPScanAbility::StaticClass();
}

void UNPAbilitySystemComponent::InitializeForOwner()
{
	AActor* OwningActor = GetOwner();
	if (OwningActor)
	{
		InitAbilityActorInfo(OwningActor, OwningActor);
		if (!bGameplayEffectDelegateBound)
		{
			OnGameplayEffectAppliedDelegateToSelf.AddUObject(
				this,
				&UNPAbilitySystemComponent::HandleGameplayEffectApplied);
			bGameplayEffectDelegateBound = true;
		}

		if (OwningActor->HasAuthority() && !bDefaultAbilitiesGranted)
		{
			DefaultAbilityHandles.Add(GiveAbility(
				FGameplayAbilitySpec(UNPPhotoAimAbility::StaticClass(), 1)));
			DefaultAbilityHandles.Add(GiveAbility(
				FGameplayAbilitySpec(UNPPhotoShotAbility::StaticClass(), 1)));
			if (ScanAbilityClass)
			{
				DefaultAbilityHandles.Add(GiveAbility(
					FGameplayAbilitySpec(ScanAbilityClass, 1)));
			}
			bDefaultAbilitiesGranted = true;
		}
	}
}

void UNPAbilitySystemComponent::BindRelicUseInput(
	UEnhancedInputComponent* EnhancedInputComponent,
	UInputAction* RelicUseAction)
{
	if (!EnhancedInputComponent || !RelicUseAction)
	{
		return;
	}

	EnhancedInputComponent->BindAction(
		RelicUseAction,
		ETriggerEvent::Started,
		this,
		&UNPAbilitySystemComponent::ActivateRelicUseAbility);
}

void UNPAbilitySystemComponent::SetHeldRelic(AActor* Relic)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	const bool bWasCarryingRelic = HasMatchingGameplayTag(
		NPGameplayTags::State_Relic_Carrying);
	const bool bIsCarryingRelic = IsValid(Relic);
	const UNPUsableRelicComponent* UsableRelic = bIsCarryingRelic
		? Relic->FindComponentByClass<UNPUsableRelicComponent>()
		: nullptr;
	const bool bWasCarryingUsableRelic = HasMatchingGameplayTag(
		NPGameplayTags::State_Relic_Carrying_Usable);
	const bool bIsCarryingUsableRelic = IsValid(UsableRelic);
	if (bWasCarryingRelic != bIsCarryingRelic)
	{
		if (bIsCarryingRelic)
		{
			AddLooseGameplayTag(
				NPGameplayTags::State_Relic_Carrying,
				1,
				EGameplayTagReplicationState::TagOnly);
		}
		else
		{
			RemoveLooseGameplayTag(
				NPGameplayTags::State_Relic_Carrying,
				1,
				EGameplayTagReplicationState::TagOnly);
		}
	}
	if (bWasCarryingUsableRelic != bIsCarryingUsableRelic)
	{
		if (bIsCarryingUsableRelic)
		{
			AddLooseGameplayTag(
				NPGameplayTags::State_Relic_Carrying_Usable,
				1,
				EGameplayTagReplicationState::TagOnly);
		}
		else
		{
			RemoveLooseGameplayTag(
				NPGameplayTags::State_Relic_Carrying_Usable,
				1,
				EGameplayTagReplicationState::TagOnly);
		}
	}

	ClearHeldRelicAbilities();
	if (!UsableRelic)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass :
		UsableRelic->GetUseAbilityClasses())
	{
		if (!AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(
			AbilityClass,
			1,
			INDEX_NONE,
			Relic);
		HeldRelicAbilityHandles.Add(GiveAbility(AbilitySpec));
	}
}

void UNPAbilitySystemComponent::ActivateRelicUseAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Input_Relic_Use);
	TryActivateAbilitiesByTag(AbilityTags);
}

void UNPAbilitySystemComponent::ActivateRelicAimAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Input_Relic_Aim);
	TryActivateAbilitiesByTag(AbilityTags);
}

void UNPAbilitySystemComponent::CancelRelicAimAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Ability_Relic_Aim);
	CancelAbilities(&AbilityTags);
}

void UNPAbilitySystemComponent::ActivateRelicFireAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Input_Relic_Fire);
	TryActivateAbilitiesByTag(AbilityTags);
}

void UNPAbilitySystemComponent::TogglePhotoAimAbility()
{
	if (HasMatchingGameplayTag(NPGameplayTags::State_Photo_Aiming))
	{
		CancelPhotoAimAbility();
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Input_Photo_Aim);
	TryActivateAbilitiesByTag(AbilityTags);
}

void UNPAbilitySystemComponent::CancelPhotoAimAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Ability_Photo_Aim);
	CancelAbilities(&AbilityTags);
}

void UNPAbilitySystemComponent::ActivatePhotoShotAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Input_Photo_Shot);
	TryActivateAbilitiesByTag(AbilityTags);
}

void UNPAbilitySystemComponent::ActivateScanAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Input_Scan);
	TryActivateAbilitiesByTag(AbilityTags);
}

void UNPAbilitySystemComponent::ClearHeldRelicAbilities()
{
	for (const FGameplayAbilitySpecHandle& AbilityHandle : HeldRelicAbilityHandles)
	{
		if (AbilityHandle.IsValid())
		{
			CancelAbilityHandle(AbilityHandle);
			ClearAbility(AbilityHandle);
		}
	}
	HeldRelicAbilityHandles.Reset();
}

void UNPAbilitySystemComponent::HandleGameplayEffectApplied(
	UAbilitySystemComponent*,
	const FGameplayEffectSpec& EffectSpec,
	FActiveGameplayEffectHandle)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor
		|| !OwningActor->HasAuthority()
		|| !EffectSpec.Def)
	{
		return;
	}
	FGameplayTagContainer GrantedTags;
	EffectSpec.GetAllGrantedTags(GrantedTags);
	if (GrantedTags.HasTagExact(NPGameplayTags::State_LavaBurning))
	{
		if (ANPStablePhysicsPawn* TargetPawn = Cast<ANPStablePhysicsPawn>(GetAvatarActor()))
		{
			TargetPawn->SetExternalVerticalVelocity(FMath::Max(0.0f, LavaJumpVelocity));
		}
	}
	FGameplayTagContainer EffectAssetTags;
	EffectSpec.GetAllAssetTags(EffectAssetTags);
	if (!EffectAssetTags.HasTag(NPGameplayTags::Effect_Knockback))
	{
		return;
	}
	HandleKnockbackEffect(EffectSpec);
}

void UNPAbilitySystemComponent::HandleKnockbackEffect(
	const FGameplayEffectSpec& EffectSpec)
{
	const FHitResult* Hit = EffectSpec.GetContext().GetHitResult();
	if (!Hit)
	{
		return;
	}

	const FVector KnockbackDirection =
		(Hit->TraceEnd - Hit->TraceStart).GetSafeNormal();
	const float KnockbackMagnitude = EffectSpec.GetSetByCallerMagnitude(
		NPGameplayTags::Data_Knockback_Magnitude,
		false,
		0.0f);
	if (KnockbackDirection.IsNearlyZero()
		|| KnockbackMagnitude <= UE_SMALL_NUMBER)
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Ability_Relic);
	CancelAbilities(&AbilityTags);

	if (ANPStablePhysicsPawn* TargetPawn =
		Cast<ANPStablePhysicsPawn>(GetAvatarActor()))
	{
		TargetPawn->StartTemporaryRagdoll();
		TargetPawn->AddExternalVelocityChange(
			KnockbackDirection * KnockbackMagnitude);
	}
}
