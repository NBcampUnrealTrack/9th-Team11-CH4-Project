#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UNPUsableRelicComponent::UNPUsableRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPUsableRelicComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CooldownEndServerTime);
	DOREPLIFETIME(ThisClass, CooldownDuration);
}

bool UNPUsableRelicComponent::GetCooldownDisplay(
	float& OutProgress,
	float& OutRemainingTime,
	float& OutDuration) const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const float CurrentServerTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: (World ? World->GetTimeSeconds() : 0.0f);

	OutDuration = FMath::Max(0.0f, CooldownDuration);
	OutRemainingTime = FMath::Clamp(
		CooldownEndServerTime - CurrentServerTime,
		0.0f,
		OutDuration);
	OutProgress = OutDuration > UE_SMALL_NUMBER
		? FMath::Clamp(1.0f - OutRemainingTime / OutDuration, 0.0f, 1.0f)
		: 1.0f;
	return OutRemainingTime > UE_SMALL_NUMBER;
}

void UNPUsableRelicComponent::StartUseCooldown(const float Duration)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(World))
	{
		return;
	}

	const AGameStateBase* GameState = World->GetGameState();
	const float CurrentServerTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
	CooldownDuration = FMath::IsFinite(Duration)
		? FMath::Max(0.0f, Duration)
		: 0.0f;
	CooldownEndServerTime = CurrentServerTime + CooldownDuration;
	OwnerActor->ForceNetUpdate();
}

void UNPUsableRelicComponent::SetUseAbilityClass(
	TSubclassOf<UGameplayAbility> InAbilityClass)
{
	UseAbilityClasses.Reset();
	if (InAbilityClass)
	{
		UseAbilityClasses.Add(InAbilityClass);
	}
}

void UNPUsableRelicComponent::SetUseAbilityClasses(
	const TArray<TSubclassOf<UGameplayAbility>>& InAbilityClasses)
{
	UseAbilityClasses = InAbilityClasses;
}
