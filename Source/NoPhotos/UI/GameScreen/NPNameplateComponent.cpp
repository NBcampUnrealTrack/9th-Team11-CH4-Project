#include "UI/GameScreen/NPNameplateComponent.h"

#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
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

	const bool bShouldBeVisible = !OwnerPawn->IsLocallyControlled()	&& !bHiddenByGameplay;
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

	// 위·아래에서 볼 때도 읽히도록 Pitch와 Yaw 모두 카메라를 향하게 합니다.
	SetWorldRotation(LookAtRotation);
}
