#include "NPRelicBonusCountdownActor.h"

#include "NPRelicBonusCountdownWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ANPRelicBonusCountdownActor::ANPRelicBonusCountdownActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CountdownWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("CountdownWidget"));
	CountdownWidgetComponent->SetupAttachment(SceneRoot);
	CountdownWidgetComponent->SetWidgetClass(UNPRelicBonusCountdownWidget::StaticClass());
	CountdownWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	CountdownWidgetComponent->SetDrawSize(FVector2D(180.0f, 60.0f));
	CountdownWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	CountdownWidgetComponent->SetTwoSided(true);
	CountdownWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANPRelicBonusCountdownActor::BeginPlay()
{
	Super::BeginPlay();
	UpdateCountdown();
}

void ANPRelicBonusCountdownActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FaceLocalPlayerCamera();
	UpdateCountdown();
}

void ANPRelicBonusCountdownActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPRelicBonusCountdownActor, EndServerWorldTime);
}

void ANPRelicBonusCountdownActor::SetCountdownDuration(const float DurationSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	SetCountdownEndServerWorldTime(
		GetSynchronizedWorldTime() + FMath::Max(0.0f, DurationSeconds));
}

void ANPRelicBonusCountdownActor::SetCountdownEndServerWorldTime(
	const float InEndServerWorldTime)
{
	if (!HasAuthority())
	{
		return;
	}

	EndServerWorldTime = FMath::Max(0.0f, InEndServerWorldTime);
	ForceNetUpdate();
	UpdateCountdown();
}

void ANPRelicBonusCountdownActor::OnRep_EndServerWorldTime()
{
	LastDisplayedSeconds = INDEX_NONE;
	UpdateCountdown();
}

void ANPRelicBonusCountdownActor::UpdateCountdown()
{
	const int32 RemainingSeconds = FMath::Max(
		0,
		FMath::CeilToInt(EndServerWorldTime - GetSynchronizedWorldTime()));
	if (RemainingSeconds == LastDisplayedSeconds)
	{
		return;
	}

	UNPRelicBonusCountdownWidget* CountdownWidget = Cast<UNPRelicBonusCountdownWidget>(
		CountdownWidgetComponent->GetUserWidgetObject());
	if (!CountdownWidget)
	{
		return;
	}

	LastDisplayedSeconds = RemainingSeconds;
	CountdownWidget->SetRemainingSeconds(RemainingSeconds);
}

void ANPRelicBonusCountdownActor::FaceLocalPlayerCamera()
{
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return;
	}

	const FVector ToCamera = CameraManager->GetCameraLocation() - GetActorLocation();
	if (!ToCamera.IsNearlyZero())
	{
		SetActorRotation(ToCamera.Rotation());
	}
}

float ANPRelicBonusCountdownActor::GetSynchronizedWorldTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}
