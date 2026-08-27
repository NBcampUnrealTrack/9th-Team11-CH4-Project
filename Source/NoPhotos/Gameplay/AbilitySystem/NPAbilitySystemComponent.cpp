#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"

#include "Core/GameplayTag/NPGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "GameplayAbilitySpec.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "GameplayEffect.h"
#include "InputAction.h"

UNPAbilitySystemComponent::UNPAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
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

	ClearHeldRelicAbility();
	if (!IsValid(Relic))
	{
		return;
	}

	const UNPUsableRelicComponent* UsableRelic =
		Relic->FindComponentByClass<UNPUsableRelicComponent>();
	if (!UsableRelic || !UsableRelic->GetUseAbilityClass())
	{
		return;
	}

	FGameplayAbilitySpec AbilitySpec(
		UsableRelic->GetUseAbilityClass(),
		1,
		INDEX_NONE,
		Relic);
	HeldRelicAbilityHandle = GiveAbility(AbilitySpec);
}

void UNPAbilitySystemComponent::ActivateRelicUseAbility()
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Input_Relic_Use);
	TryActivateAbilitiesByTag(AbilityTags);
}

void UNPAbilitySystemComponent::ClearHeldRelicAbility()
{
	if (!HeldRelicAbilityHandle.IsValid())
	{
		return;
	}

	CancelAbilityHandle(HeldRelicAbilityHandle);
	ClearAbility(HeldRelicAbilityHandle);
	HeldRelicAbilityHandle = FGameplayAbilitySpecHandle();
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
	FGameplayTagContainer EffectAssetTags;
	EffectSpec.GetAllAssetTags(EffectAssetTags);
	if (!EffectAssetTags.HasTag(NPGameplayTags::Effect_Knockback))
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NPGameplayTags::Ability_Relic);
	CancelAbilities(&AbilityTags);

	const FHitResult* Hit = EffectSpec.GetContext().GetHitResult();
	if (!Hit)
	{
		return;
	}

	FVector KnockbackDirection = Hit->TraceEnd - Hit->TraceStart;
	KnockbackDirection.Z = 0.0f;
	KnockbackDirection.Normalize();
	const float KnockbackMagnitude = EffectSpec.GetSetByCallerMagnitude(
		NPGameplayTags::Data_Knockback_Magnitude,
		false,
		0.0f);
	if (KnockbackDirection.IsNearlyZero()
		|| KnockbackMagnitude <= UE_SMALL_NUMBER)
	{
		return;
	}

	if (ANPStablePhysicsPawn* TargetPawn =
		Cast<ANPStablePhysicsPawn>(GetAvatarActor()))
	{
		TargetPawn->AddExternalVelocityChange(
			KnockbackDirection * KnockbackMagnitude);
	}
}
