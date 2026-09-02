#include "UI/GameScreen/Relic/NPRelicHoverInfoComponent.h"

#include "Data/Structs/NPRelicData.h"
#include "Camera/PlayerCameraManager.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "UI/GameScreen/Relic/NPRelicHoverInfoWidget.h"

UNPRelicHoverInfoComponent::UNPRelicHoverInfoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetWidgetSpace(EWidgetSpace::World);
	SetDrawAtDesiredSize(true);
	SetTwoSided(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetVisibility(false);
}

void UNPRelicHoverInfoComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshRelicInfo();
	RefreshHoverVisibility();
}

void UNPRelicHoverInfoComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshHoverVisibility();
	UpdateFacingCamera();
}

void UNPRelicHoverInfoComponent::RefreshRelicInfo()
{
	InitWidget();

	UNPRelicHoverInfoWidget* HoverInfoWidget = Cast<UNPRelicHoverInfoWidget>(GetUserWidgetObject());
	if (!IsValid(HoverInfoWidget))
	{
		return;
	}

	const ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner());
	const FNPRelicTableRow* RelicData = Relic ? Relic->GetRelicTableData() : nullptr;

	if (RelicData)
	{
		HoverInfoWidget->SetRelicInfo(RelicData->DisplayName, RelicData->Price);
		return;
	}

	HoverInfoWidget->SetRelicInfo(FText::FromString(TEXT("이름 없는 유물")), 0);
}

void UNPRelicHoverInfoComponent::SetHoverInfoVisible(const bool bShouldBeVisible)
{
	bVisibleByGameplay = bShouldBeVisible;
	RefreshHoverVisibility();
}

void UNPRelicHoverInfoComponent::SetFocusedByLocalPlayer(const bool bIsFocused)
{
	bFocusedByLocalPlayer = bIsFocused;
	RefreshHoverVisibility();
}

void UNPRelicHoverInfoComponent::RefreshHoverVisibility()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetVisibility(false, true);
		return;
	}

	SetVisibility(
		bVisibleByGameplay && bFocusedByLocalPlayer,
		true);
}

void UNPRelicHoverInfoComponent::UpdateFacingCamera()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const APlayerCameraManager* CameraManager =
		UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!IsValid(CameraManager))
	{
		return;
	}

	const FVector HoverLocation = GetComponentLocation();
	SetWorldRotation(UKismetMathLibrary::FindLookAtRotation(
		HoverLocation,
		CameraManager->GetCameraLocation()));
}
