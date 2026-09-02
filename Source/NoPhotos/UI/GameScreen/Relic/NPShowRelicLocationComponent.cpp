#include "UI/GameScreen/Relic/NPShowRelicLocationComponent.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Photo/NPPhotoCaptureComponent.h"
#include "Gameplay/Relic/Components/NPPlayerBonusQuestComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

namespace
{
	const TArray<FLinearColor> QuestRelicColors =
	{
		FLinearColor(0.93f, 0.20f, 0.20f), // Red
		FLinearColor(0.20f, 0.80f, 0.30f), // Green
		FLinearColor(0.25f, 0.50f, 1.00f), // Blue
		FLinearColor(1.00f, 0.55f, 0.15f), // Orange
		FLinearColor(0.65f, 0.35f, 0.90f), // Purple
		FLinearColor(0.20f, 0.20f, 0.65f)  // Navy
	};
}

UNPShowRelicLocationComponent::UNPShowRelicLocationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FLinearColor UNPShowRelicLocationComponent::GetQuestRelicColor(const int32 RelicIndex)
{
	return QuestRelicColors.IsEmpty()
		? FLinearColor::White
		: QuestRelicColors[FMath::Abs(RelicIndex) % QuestRelicColors.Num()];
}

void UNPShowRelicLocationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetMarkerVisibility(false);
		SetComponentTickEnabled(false);
		return;
	}

	if (const ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner()))
	{
		RelicLocationOffset =
			GetComponentLocation() - Relic->GetRelicWorldLocation();
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		BoundBonusQuestComponent = PlayerController->FindComponentByClass<UNPPlayerBonusQuestComponent>();
	}

	if (UUserWidget* MarkerWidget = GetUserWidgetObject())
	{
		PointText = Cast<UTextBlock>(MarkerWidget->GetWidgetFromName(TEXT("PointText")));
		LeftDistanceText = Cast<UTextBlock>(MarkerWidget->GetWidgetFromName(TEXT("LeftDistance")));
	}

	RefreshMarker();
}

void UNPShowRelicLocationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshMarker();
}

bool UNPShowRelicLocationComponent::GetAssignedRelicIndex(int32& OutRelicIndex) const
{
	ANPBaseRelic* OwnerRelic = Cast<ANPBaseRelic>(GetOwner());
	const UNPPlayerBonusQuestComponent* QuestComponent = BoundBonusQuestComponent.Get();
	if (!IsValid(OwnerRelic) || !IsValid(QuestComponent))
	{
		return false;
	}

	OutRelicIndex = QuestComponent->GetAssignedQuestRelics().IndexOfByKey(OwnerRelic);
	return OutRelicIndex != INDEX_NONE;
}

void UNPShowRelicLocationComponent::RefreshMarker()
{
	const ANPBaseRelic* OwnerRelic = Cast<ANPBaseRelic>(GetOwner());
	if (!IsValid(OwnerRelic))
	{
		SetMarkerVisibility(false);
		return;
	}

	const FVector RelicLocation = OwnerRelic->GetRelicWorldLocation();
	SetWorldLocation(RelicLocation + RelicLocationOffset);

	if (OwnerRelic->IsReturned())
	{
		if (!bWasReturned)
		{
			bWasReturned = true;
			OnRelicReturned.Broadcast();
		}

		SetMarkerVisibility(false);
		return;
	}

	bWasReturned = false;

	const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const UNPPhotoCaptureComponent* PhotoCaptureComponent = PlayerController
		? PlayerController->FindComponentByClass<UNPPhotoCaptureComponent>()
		: nullptr;
	if (IsValid(PhotoCaptureComponent) && PhotoCaptureComponent->IsPhotoModeActive())
	{
		SetMarkerVisibility(false);
		return;
	}

	if (!BoundBonusQuestComponent.IsValid())
	{
		if (APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			BoundBonusQuestComponent = LocalPlayerController->FindComponentByClass<UNPPlayerBonusQuestComponent>();
		}
	}

	int32 RelicIndex = INDEX_NONE;
	if (!GetAssignedRelicIndex(RelicIndex))
	{
		SetMarkerVisibility(false);
		return;
	}

	SetMarkerVisibility(true);

	if (!PointText.IsValid() || !LeftDistanceText.IsValid())
	{
		if (UUserWidget* MarkerWidget = GetUserWidgetObject())
		{
			PointText = Cast<UTextBlock>(MarkerWidget->GetWidgetFromName(TEXT("PointText")));
			LeftDistanceText = Cast<UTextBlock>(MarkerWidget->GetWidgetFromName(TEXT("LeftDistance")));
		}
	}

	const FLinearColor RelicColor = GetQuestRelicColor(RelicIndex);
	if (PointText.IsValid())
	{
		PointText->SetColorAndOpacity(FSlateColor(RelicColor));
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (LeftDistanceText.IsValid() && IsValid(PlayerPawn))
	{
		const int32 DistanceInMeters = FMath::RoundToInt(
			FVector::Distance(PlayerPawn->GetActorLocation(), RelicLocation) / 100.0f);
		LeftDistanceText->SetText(FText::FromString(FString::Printf(TEXT("%dm"), DistanceInMeters)));
	}
}

void UNPShowRelicLocationComponent::SetMarkerVisibility(const bool bShouldBeVisible)
{
	SetVisibility(bShouldBeVisible, true);
}
