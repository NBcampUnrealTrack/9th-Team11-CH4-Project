#include "NPSpotlightMapEvent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/RectLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/MapEvents/NPMapEventManager.h"
#include "Gameplay/MapEvents/NPMapEventSpawnPoint.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "NPEventSpotlight.h"

ANPSpotlightMapEvent::ANPSpotlightMapEvent()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	LocationSource = ENPMapEventLocationSource::Point;
	SpotlightSpawnGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("Spotlight")), false);
	SpotlightClass = ANPEventSpotlight::StaticClass();
}

void ANPSpotlightMapEvent::BeginPlay()
{
	Super::BeginPlay();
	if (IsEventActive())
	{
		SetMainRectLightsDimmed(true);
	}
}

void ANPSpotlightMapEvent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPSpotlightMapEvent, SpotlightCycle);
}

float ANPSpotlightMapEvent::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void ANPSpotlightMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	SetMainRectLightsDimmed(bNewActive);
	if (!HasAuthority())
	{
		return;
	}

	if (bNewActive)
	{
		SpawnSpotlights();
		StartLightCycle(GetServerTime());
		SetActorTickEnabled(true);
	}
	else
	{
		// 전체 Duration과 마지막 보상 시각이 같아도 완료된 2초는 반영합니다.
		if (IsValid(SpotlightCycle.ActiveSpotlight))
		{
			UpdatePriceBonuses(FMath::Min(GetServerTime(), SpotlightCycle.EndServerWorldTime));
		}
		CleanupEvent();
	}
}

void ANPSpotlightMapEvent::SetMainRectLightsDimmed(const bool bDimmed, const bool bImmediate)
{
	UWorld* World = GetWorld();
	if (bDimmed && World)
	{
		for (TActorIterator<ARectLight> Iterator(World); Iterator; ++Iterator)
		{
			ARectLight* Light = *Iterator;
			if (IsValid(Light) && Light->GetLevel() == World->PersistentLevel
				&& IsValid(Light->RectLightComponent))
			{
				MainRectLights.FindOrAdd(TWeakObjectPtr<URectLightComponent>(Light->RectLightComponent.Get()));
			}
		}
	}

	for (auto& Entry : MainRectLights)
	{
		if (URectLightComponent* Light = Entry.Key.Get())
		{
			Light->SetIntensityUnits(ELightUnits::Candelas);
			Entry.Value = Light->Intensity;
		}
	}
	RectLightTargetIntensity = bDimmed ? 5.0f : 160.0f;
	RectLightFadeElapsed = 0.0f;
	bRectLightsFading = !MainRectLights.IsEmpty();
	SetActorTickEnabled(bRectLightsFading || (HasAuthority() && IsEventActive()));
	UpdateMainRectLightFade(bImmediate ? FMath::Max(0.0f, RectLightFadeDuration) : 0.0f);
}

void ANPSpotlightMapEvent::UpdateMainRectLightFade(const float DeltaSeconds)
{
	if (!bRectLightsFading)
	{
		return;
	}

	RectLightFadeElapsed += DeltaSeconds;
	const float Alpha = RectLightFadeDuration > 0.0f
		? FMath::Clamp(RectLightFadeElapsed / RectLightFadeDuration, 0.0f, 1.0f) : 1.0f;
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	for (const auto& Entry : MainRectLights)
	{
		if (URectLightComponent* Light = Entry.Key.Get())
		{
			Light->SetIntensity(FMath::Lerp(Entry.Value, RectLightTargetIntensity, SmoothAlpha));
		}
	}
	if (Alpha >= 1.0f)
	{
		bRectLightsFading = false;
		if (RectLightTargetIntensity == 160.0f)
		{
			MainRectLights.Reset();
		}
		SetActorTickEnabled(HasAuthority() && IsEventActive());
	}
}

void ANPSpotlightMapEvent::SpawnSpotlights()
{
	const UNPMapEventManagerComponent* Manager = GetOwner()
		? GetOwner()->FindComponentByClass<UNPMapEventManagerComponent>() : nullptr;
	if (!Manager || !SpotlightClass)
	{
		return;
	}

	TArray<ANPMapEventSpawnPoint*> Points;
	Manager->GetSpawnPointsForGroup(SpotlightSpawnGroup, Points);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const ANPMapEventSpawnPoint* Point : Points)
	{
		FTransform SpawnTransform = Point->GetActorTransform();
		SpawnTransform.SetScale3D(FVector::OneVector);
		if (ANPEventSpotlight* Light = GetWorld()->SpawnActor<ANPEventSpotlight>(
			SpotlightClass, SpawnTransform, SpawnParameters))
		{
			Light->SetBeamScaleMultiplier(Point->GetActorScale3D());
			SpawnedSpotlights.Add(Light);
		}
	}
}

void ANPSpotlightMapEvent::StartLightCycle(const float Now)
{
	ClearExposures();
	SpawnedSpotlights.RemoveAll([](const ANPEventSpotlight* Light) { return !IsValid(Light); });
	if (SpawnedSpotlights.IsEmpty())
	{
		// StartEvent의 시작 델리게이트가 끝난 다음 Tick에서 정상 종료합니다.
		SpotlightCycle = FNPSpotlightCycle();
		return;
	}

	SpotlightCycle.ActiveSpotlight = SpawnedSpotlights[FMath::RandRange(0, SpawnedSpotlights.Num() - 1)];
	SpotlightCycle.StartServerWorldTime = Now;
	SpotlightCycle.EndServerWorldTime = Now + FMath::Max(0.1f, LightDuration);
	UpdatePriceBonuses(Now);
	ForceNetUpdate();
}

void ANPSpotlightMapEvent::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMainRectLightFade(DeltaSeconds);
	if (!HasAuthority() || !IsEventActive())
	{
		return;
	}
	if (SpawnedSpotlights.IsEmpty())
	{
		FinishEvent();
		return;
	}

	const float Now = GetServerTime();
	if (IsValid(SpotlightCycle.ActiveSpotlight))
	{
		UpdatePriceBonuses(FMath::Min(Now, SpotlightCycle.EndServerWorldTime));
		if (Now >= SpotlightCycle.EndServerWorldTime)
		{
			SpotlightCycle.ActiveSpotlight->UpdateBeam(0.0f, false);
			SpotlightCycle.ActiveSpotlight = nullptr;
			SpotlightCycle.StartServerWorldTime = Now;
			SpotlightCycle.EndServerWorldTime = Now + FMath::Max(0.1f, DarkDuration);
			ClearExposures();
			ForceNetUpdate();
		}
	}
	else if (Now >= SpotlightCycle.EndServerWorldTime)
	{
		StartLightCycle(Now);
	}
}

void ANPSpotlightMapEvent::UpdatePriceBonuses(const float Now)
{
	ANPEventSpotlight* Light = SpotlightCycle.ActiveSpotlight;
	if (!IsValid(Light))
	{
		return;
	}
	Light->UpdateBeam(Now - SpotlightCycle.StartServerWorldTime, true);
	const float Interval = FMath::Max(0.1f, BonusInterval);
	TSet<TWeakObjectPtr<ANPStablePhysicsPawn>> EligiblePawns;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* Controller = Iterator->Get();
		ANPStablePhysicsPawn* Pawn = Controller ? Cast<ANPStablePhysicsPawn>(Controller->GetPawn()) : nullptr;
		UNPStablePhysicsGrabComponent* Grab = IsValid(Pawn) ? Pawn->GetRightHandGrabComponent() : nullptr;
		UPrimitiveComponent* HeldComponent = IsValid(Grab) ? Grab->GetGrabbedComponent() : nullptr;
		ANPBaseRelic* Relic = IsValid(HeldComponent) ? Cast<ANPBaseRelic>(HeldComponent->GetOwner()) : nullptr;
		if (!IsValid(Relic) || Relic->IsReturned() || !Light->IsIlluminatingPawn(Pawn, Relic))
		{
			continue;
		}

		const TWeakObjectPtr<ANPStablePhysicsPawn> PawnKey(Pawn);
		EligiblePawns.Add(PawnKey);
		FPlayerExposure& Exposure = PlayerExposures.FindOrAdd(PawnKey);
		if (!Exposure.GrabChangedHandle.IsValid())
		{
			Exposure.GrabComponent = Grab;
			Exposure.GrabChangedHandle = Grab->OnGrabbedComponentChanged.AddWeakLambda(
				this, [this, PawnKey](UPrimitiveComponent*)
				{
					if (FPlayerExposure* Existing = PlayerExposures.Find(PawnKey))
					{
						Existing->Relic.Reset();
					}
				});
		}
		if (Exposure.Relic.Get() != Relic)
		{
			Exposure.Relic = Relic;
			Exposure.NextBonusTime = Now + Interval;
		}
		if (Now + KINDA_SMALL_NUMBER < Exposure.NextBonusTime)
		{
			continue;
		}

		const TWeakObjectPtr<ANPBaseRelic> RelicKey(Relic);
		const float* LastBonusTime = LastRelicBonusTimes.Find(RelicKey);
		if (!LastBonusTime || Exposure.NextBonusTime + KINDA_SMALL_NUMBER >= *LastBonusTime + Interval)
		{
			Relic->AddPriceBonus(BonusRate);
			LastRelicBonusTimes.Add(RelicKey, Exposure.NextBonusTime);
		}
		Exposure.NextBonusTime += Interval;
	}

	for (auto Iterator = PlayerExposures.CreateIterator(); Iterator; ++Iterator)
	{
		if (!EligiblePawns.Contains(Iterator.Key()))
		{
			if (UNPStablePhysicsGrabComponent* Grab = Iterator.Value().GrabComponent.Get())
			{
				Grab->OnGrabbedComponentChanged.Remove(Iterator.Value().GrabChangedHandle);
			}
			Iterator.RemoveCurrent();
		}
	}
}

void ANPSpotlightMapEvent::ClearExposures()
{
	for (const auto& Entry : PlayerExposures)
	{
		if (UNPStablePhysicsGrabComponent* Grab = Entry.Value.GrabComponent.Get())
		{
			Grab->OnGrabbedComponentChanged.Remove(Entry.Value.GrabChangedHandle);
		}
	}
	PlayerExposures.Reset();
	LastRelicBonusTimes.Reset();
}

void ANPSpotlightMapEvent::CleanupEvent()
{
	SetActorTickEnabled(bRectLightsFading);
	ClearExposures();
	SpotlightCycle = FNPSpotlightCycle();
	for (ANPEventSpotlight* Light : SpawnedSpotlights)
	{
		if (IsValid(Light))
		{
			Light->Destroy();
		}
	}
	SpawnedSpotlights.Reset();
}

void ANPSpotlightMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetMainRectLightsDimmed(false, true);
	CleanupEvent();
	Super::EndPlay(EndPlayReason);
}
