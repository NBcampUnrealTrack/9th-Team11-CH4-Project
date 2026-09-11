#include "Gameplay/MapEvents/CCTV/NPCCTVMapEvent.h"

#include "Components/RectLightComponent.h"
#include "Engine/RectLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Map/CCTV/NPCCTVSensor.h"

void ANPCCTVMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	SetRectLightsDisabled(bNewActive);

	if (!HasAuthority())
	{
		return;
	}
	TargetReservationEndTimes.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ANPCCTVSensor> Iterator(World); Iterator; ++Iterator)
	{
		if (ANPCCTVSensor* Sensor = *Iterator; IsValid(Sensor))
		{
			Sensor->SetOwningMapEvent(bNewActive ? this : nullptr);
			Sensor->SetCCTVActive(bNewActive);
		}
	}
}

bool ANPCCTVMapEvent::IsTargetReserved(
	ANPStablePhysicsPawn* Target,
	const float ServerTime) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	const float* ReservationEndTime = TargetReservationEndTimes.Find(Target);
	return ReservationEndTime && ServerTime < *ReservationEndTime;
}

bool ANPCCTVMapEvent::TryReserveTarget(
	ANPStablePhysicsPawn* Target,
	const float ServerTime,
	const float ReservationDuration)
{
	if (!HasAuthority() || !IsEventActive() || !IsValid(Target)
		|| IsTargetReserved(Target, ServerTime))
	{
		return false;
	}

	TargetReservationEndTimes.Add(
		Target,
		ServerTime + FMath::Max(0.0f, ReservationDuration));
	return true;
}

void ANPCCTVMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetRectLightsDisabled(false);
	Super::EndPlay(EndPlayReason);
}

void ANPCCTVMapEvent::SetRectLightsDisabled(const bool bDisabled)
{
	if (!bDisabled)
	{
		for (const auto& Entry : SavedRectLightIntensities)
		{
			if (URectLightComponent* RectLight = Entry.Key.Get())
			{
				RectLight->SetIntensity(Entry.Value);
			}
		}
		SavedRectLightIntensities.Reset();
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !SavedRectLightIntensities.IsEmpty())
	{
		return;
	}

	for (TActorIterator<ARectLight> Iterator(World); Iterator; ++Iterator)
	{
		ARectLight* RectLightActor = *Iterator;
		URectLightComponent* RectLight = IsValid(RectLightActor)
			? RectLightActor->RectLightComponent.Get()
			: nullptr;
		if (!IsValid(RectLight)
			|| RectLightActor->GetLevel() != World->PersistentLevel
			|| RectLightActor->ActorHasTag(ExcludedRectLightTag)
			|| RectLight->ComponentHasTag(ExcludedRectLightTag))
		{
			continue;
		}

		SavedRectLightIntensities.Add(RectLight, RectLight->Intensity);
		RectLight->SetIntensity(0.0f);
	}
}
