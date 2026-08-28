#include "UI/GameScreen/NPNameplateComponent.h"

#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Gameplay/Character/Component/NPInvisibilityComponent.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "UI/GameScreen/NPUserNameWidget.h"

UNPNameplateComponent::UNPNameplateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetWidgetSpace(EWidgetSpace::World);
	SetTwoSided(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UNPNameplateComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshNameplate();
}

void UNPNameplateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshNameplate();
	UpdateFacingCamera();
}

void UNPNameplateComponent::SetNameplateVisible(bool bShouldBeVisible)
{
	bHiddenByGameplay = !bShouldBeVisible;
	UpdateNameplateVisibility();
}

bool UNPNameplateComponent::RefreshNameplate()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		return false;
	}

	UpdateNameplateVisibility();

	UNPUserNameWidget* CurrentWidget = Cast<UNPUserNameWidget>(GetUserWidgetObject());
	APlayerState* CurrentPlayerState = OwnerPawn->GetPlayerState<APlayerState>();
	if (!IsValid(CurrentWidget) || !IsValid(CurrentPlayerState))
	{
		return false;
	}

	if (NameplateWidget != CurrentWidget || BoundPlayerState != CurrentPlayerState)
	{
		NameplateWidget = CurrentWidget;
		BoundPlayerState = CurrentPlayerState;
		NameplateWidget->SetTargetPlayerState(BoundPlayerState);
	}

	return true;
}

void UNPNameplateComponent::UpdateNameplateVisibility()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	//GAS를 통해 서버에서 가시여부 결정 후 클라이언트에 복제
	const UNPInvisibilityComponent* InvisibilityComponent = OwnerPawn->FindComponentByClass<UNPInvisibilityComponent>();
	const bool bHiddenByInvisibility = IsValid(InvisibilityComponent) && InvisibilityComponent->IsInvisible();
	const bool bShouldBeVisible = !OwnerPawn->IsLocallyControlled() && !bHiddenByGameplay && !bHiddenByInvisibility;
	SetVisibility(bShouldBeVisible, true);
}

void UNPNameplateComponent::UpdateFacingCamera()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!IsValid(CameraManager))
	{
		return;
	}

	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
		GetComponentLocation(),
		CameraManager->GetCameraLocation());

	SetWorldRotation(LookAtRotation);
}
