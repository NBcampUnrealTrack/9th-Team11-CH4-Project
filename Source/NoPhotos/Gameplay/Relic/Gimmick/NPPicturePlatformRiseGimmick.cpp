#include "Gameplay/Relic/Gimmick/NPPicturePlatformRiseGimmick.h"

#include "Components/SceneComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "NoPhotos.h"

ANPPicturePlatformRiseGimmick::ANPPicturePlatformRiseGimmick()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(false);
}

void ANPPicturePlatformRiseGimmick::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPPicturePlatformRiseGimmick, bRiseStarted);
	DOREPLIFETIME(ANPPicturePlatformRiseGimmick, RiseStartServerTime);
}

void ANPPicturePlatformRiseGimmick::BeginPlay()
{
	Super::BeginPlay();

	InitializePlatforms();

	if (HasAuthority())
	{
		if (!IsValid(TriggerRelic))
		{
			UE_LOG(
				LogNoPhotos,
				Error,
				TEXT("[PicturePlatformRise] TriggerRelic is not assigned. Gimmick=%s"),
				*GetNameSafe(this));
		}
		else
		{
			TriggerRelic->OnReleasedFromDisplay.AddUObject(
				this,
				&ThisClass::HandleRelicReleasedFromDisplay);

			if (!TriggerRelic->IsDisplayed())
			{
				StartPlatformRise();
			}
		}
	}

	if (bRiseStarted)
	{
		BeginLocalRisePlayback();
	}
}

void ANPPicturePlatformRiseGimmick::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(TriggerRelic))
	{
		TriggerRelic->OnReleasedFromDisplay.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ANPPicturePlatformRiseGimmick::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyPlatformRise(GetServerWorldTimeSeconds());
}

void ANPPicturePlatformRiseGimmick::InitializePlatforms()
{
	PlatformRuntimeStates.Reset(Platforms.Num());
	PlatformRuntimeStates.SetNum(Platforms.Num());

	for (int32 Index = 0; Index < Platforms.Num(); ++Index)
	{
		const FNPPlatformRiseEntry& Entry = Platforms[Index];
		FPlatformRuntimeState& RuntimeState = PlatformRuntimeStates[Index];

		if (!IsValid(Entry.PlatformActor))
		{
			UE_LOG(
				LogNoPhotos,
				Warning,
				TEXT("[PicturePlatformRise] Platform is not assigned. Gimmick=%s Index=%d"),
				*GetNameSafe(this),
				Index);
			continue;
		}

		RuntimeState.StartLocation = Entry.PlatformActor->GetActorLocation();
		RuntimeState.TargetLocation = RuntimeState.StartLocation
			+ FVector(0.0f, 0.0f, Entry.RiseHeight);

		const USceneComponent* PlatformRootComponent =
			Entry.PlatformActor->GetRootComponent();
		if (PlatformRootComponent
			&& PlatformRootComponent->GetMobility() != EComponentMobility::Movable)
		{
			UE_LOG(
				LogNoPhotos,
				Warning,
				TEXT("[PicturePlatformRise] Platform root must be Movable. Gimmick=%s Platform=%s"),
				*GetNameSafe(this),
				*GetNameSafe(Entry.PlatformActor));
		}
	}
}

void ANPPicturePlatformRiseGimmick::HandleRelicReleasedFromDisplay(
	ANPBaseRelic* ReleasedRelic)
{
	if (!HasAuthority() || ReleasedRelic != TriggerRelic || bRiseStarted)
	{
		return;
	}

	StartPlatformRise();
}

void ANPPicturePlatformRiseGimmick::StartPlatformRise()
{
	if (!HasAuthority() || bRiseStarted)
	{
		return;
	}

	RiseStartServerTime = GetServerWorldTimeSeconds();
	bRiseStarted = true;
	BeginLocalRisePlayback();
	ForceNetUpdate();

	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[PicturePlatformRise] Rise started. Gimmick=%s Relic=%s Platforms=%d ServerTime=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(TriggerRelic),
		Platforms.Num(),
		RiseStartServerTime);
}

void ANPPicturePlatformRiseGimmick::BeginLocalRisePlayback()
{
	if (!bRiseStarted)
	{
		return;
	}

	if (!bRiseStartedEventBroadcast)
	{
		bRiseStartedEventBroadcast = true;
		OnPlatformRiseStarted.Broadcast();
	}

	ApplyPlatformRise(GetServerWorldTimeSeconds());
	if (!bRiseCompleted)
	{
		SetActorTickEnabled(true);
	}
}

void ANPPicturePlatformRiseGimmick::ApplyPlatformRise(
	const float CurrentServerTime)
{
	if (!bRiseStarted || bRiseCompleted
		|| PlatformRuntimeStates.Num() != Platforms.Num())
	{
		return;
	}

	const float ElapsedTime = FMath::Max(
		0.0f,
		CurrentServerTime - RiseStartServerTime);
	bool bAllPlatformsReachedTarget = true;

	for (int32 Index = 0; Index < Platforms.Num(); ++Index)
	{
		const FNPPlatformRiseEntry& Entry = Platforms[Index];
		FPlatformRuntimeState& RuntimeState = PlatformRuntimeStates[Index];
		if (!IsValid(Entry.PlatformActor))
		{
			continue;
		}

		const float LocalTime = ElapsedTime - FMath::Max(0.0f, Entry.StartDelay);
		const float SafeDuration = FMath::Max(UE_SMALL_NUMBER, Entry.RiseDuration);
		const float Alpha = FMath::Clamp(LocalTime / SafeDuration, 0.0f, 1.0f);
		const float SmoothedAlpha = FMath::InterpEaseInOut(
			0.0f,
			1.0f,
			Alpha,
			FMath::Max(1.0f, EaseExponent));

		Entry.PlatformActor->SetActorLocation(
			FMath::Lerp(
				RuntimeState.StartLocation,
				RuntimeState.TargetLocation,
				SmoothedAlpha),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		if (Alpha < 1.0f)
		{
			bAllPlatformsReachedTarget = false;
		}
		else if (!RuntimeState.bReachedTarget)
		{
			RuntimeState.bReachedTarget = true;
			OnPlatformReachedTarget.Broadcast(Index);
		}
	}

	if (bAllPlatformsReachedTarget)
	{
		FinishPlatformRise();
	}
}

void ANPPicturePlatformRiseGimmick::FinishPlatformRise()
{
	if (bRiseCompleted)
	{
		return;
	}

	bRiseCompleted = true;
	SetActorTickEnabled(false);

	if (!bRiseCompletedEventBroadcast)
	{
		bRiseCompletedEventBroadcast = true;
		OnPlatformRiseCompleted.Broadcast();
	}
}

float ANPPicturePlatformRiseGimmick::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState
		? GameState->GetServerWorldTimeSeconds()
		: (World ? World->GetTimeSeconds() : 0.0f);
}

void ANPPicturePlatformRiseGimmick::OnRep_RiseStarted()
{
	if (bRiseStarted && HasActorBegunPlay())
	{
		BeginLocalRisePlayback();
	}
}
