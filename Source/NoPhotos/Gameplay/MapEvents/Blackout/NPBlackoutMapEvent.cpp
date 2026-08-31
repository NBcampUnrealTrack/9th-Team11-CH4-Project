#include "NPBlackoutMapEvent.h"

#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

ANPBlackoutMapEvent::ANPBlackoutMapEvent()
{
	Duration = 8.0f;
}

void ANPBlackoutMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	if (bNewActive)
	{
		ApplyBlackout();
	}
	else
	{
		RestoreLights();
	}
}

void ANPBlackoutMapEvent::ApplyBlackout()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SavedLightStates.Reset();
	for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (!IsValid(Actor))
		{
			continue;
		}

		TInlineComponentArray<ULightComponent*> LightComponents(Actor);
		for (ULightComponent* LightComponent : LightComponents)
		{
			if (!IsValid(LightComponent)
				|| (!Actor->ActorHasTag(LightTag) && !LightComponent->ComponentHasTag(LightTag)))
			{
				continue;
			}

			FLightVisibilityState& SavedState = SavedLightStates.AddDefaulted_GetRef();
			SavedState.LightComponent = LightComponent;
			SavedState.bWasVisible = LightComponent->IsVisible();
			LightComponent->SetVisibility(false, true);
		}
	}
}

void ANPBlackoutMapEvent::RestoreLights()
{
	for (const FLightVisibilityState& SavedState : SavedLightStates)
	{
		if (ULightComponent* LightComponent = SavedState.LightComponent.Get())
		{
			LightComponent->SetVisibility(SavedState.bWasVisible, true);
		}
	}

	SavedLightStates.Reset();
}
