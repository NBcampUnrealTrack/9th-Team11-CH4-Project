#include "UI/GameScreen/Relic/NPRelicUsePromptUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Relic/Components/NPUsableRelicComponent.h"
#include "TimerManager.h"
#include "UI/GameScreen/Relic/NPRelicUsePromptWidget.h"

UNPRelicUsePromptUIComponent::UNPRelicUsePromptUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPRelicUsePromptUIComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !RelicUsePromptWidgetClass)
	{
		return;
	}

	RelicUsePromptWidget = CreateWidget<UNPRelicUsePromptWidget>(
		PlayerController,
		RelicUsePromptWidgetClass);
	if (!IsValid(RelicUsePromptWidget))
	{
		return;
	}

	RelicUsePromptWidget->AddToPlayerScreen(45);
	RelicUsePromptWidget->SetRelicUseDisplay(
		false,
		1.0f,
		0.0f,
		0.0f);
	UpdateRelicUsePrompt();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			UpdateTimerHandle,
			this,
			&ThisClass::UpdateRelicUsePrompt,
			FMath::Max(0.02f, UpdateInterval),
			true);
	}
}

void UNPRelicUsePromptUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	if (IsValid(RelicUsePromptWidget))
	{
		RelicUsePromptWidget->RemoveFromParent();
	}
	RelicUsePromptWidget = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UNPRelicUsePromptUIComponent::UpdateRelicUsePrompt()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(RelicUsePromptWidget))
	{
		return;
	}

	const ANPReplicatedStablePhysicsPawn* StablePawn =
		PlayerController->GetPawn<ANPReplicatedStablePhysicsPawn>();
	AActor* HeldRelic = StablePawn
		? StablePawn->GetHeldRelic_Implementation()
		: nullptr;
	const UNPUsableRelicComponent* UsableRelic = HeldRelic
		? HeldRelic->FindComponentByClass<UNPUsableRelicComponent>()
		: nullptr;
	if (!IsValid(UsableRelic))
	{
		RelicUsePromptWidget->SetRelicUseDisplay(
			false,
			1.0f,
			0.0f,
			0.0f);
		return;
	}

	float CooldownProgress = 1.0f;
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	UsableRelic->GetCooldownDisplay(
		CooldownProgress,
		RemainingTime,
		Duration);
	RelicUsePromptWidget->SetRelicUseDisplay(
		true,
		CooldownProgress,
		RemainingTime,
		Duration);
}
