#include "UI/GameScreen/Relic/NPRelicHoverFocusComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "UI/GameScreen/Relic/NPRelicHoverInfoComponent.h"

UNPRelicHoverFocusComponent::UNPRelicHoverFocusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UNPRelicHoverFocusComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		SetComponentTickEnabled(false);
	}
}

void UNPRelicHoverFocusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetFocusedHoverInfoComponent(nullptr);

	Super::EndPlay(EndPlayReason);
}

void UNPRelicHoverFocusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ElapsedTraceTime += DeltaTime;
	if (ElapsedTraceTime >= TraceInterval)
	{
		ElapsedTraceTime = 0.0f;
		RefreshFocusedRelic();
	}
}

void UNPRelicHoverFocusComponent::RefreshFocusedRelic()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || !IsValid(World))
	{
		SetFocusedHoverInfoComponent(nullptr);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RelicHoverFocus), false);
	QueryParams.AddIgnoredActor(PlayerController->GetPawn());

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		TraceEnd,
		TraceChannel,
		QueryParams);

	ANPBaseRelic* HitRelic = bHit ? Cast<ANPBaseRelic>(Hit.GetActor()) : nullptr;
	UNPRelicHoverInfoComponent* NewFocusedComponent = IsValid(HitRelic)
		? HitRelic->FindComponentByClass<UNPRelicHoverInfoComponent>()
		: nullptr;
	SetFocusedHoverInfoComponent(NewFocusedComponent);
}

void UNPRelicHoverFocusComponent::SetFocusedHoverInfoComponent(
	UNPRelicHoverInfoComponent* NewFocusedComponent)
{
	if (FocusedHoverInfoComponent.Get() == NewFocusedComponent)
	{
		return;
	}

	if (FocusedHoverInfoComponent.IsValid())
	{
		FocusedHoverInfoComponent->SetFocusedByLocalPlayer(false);
	}

	FocusedHoverInfoComponent = NewFocusedComponent;
	if (FocusedHoverInfoComponent.IsValid())
	{
		FocusedHoverInfoComponent->SetFocusedByLocalPlayer(true);
	}
}

