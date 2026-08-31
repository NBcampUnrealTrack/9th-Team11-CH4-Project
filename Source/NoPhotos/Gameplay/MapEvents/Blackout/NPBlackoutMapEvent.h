#pragma once

#include "CoreMinimal.h"
#include "Gameplay/MapEvents/NPMapEvent.h"
#include "NPBlackoutMapEvent.generated.h"

class ULightComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPBlackoutMapEvent : public ANPMapEvent
{
	GENERATED_BODY()

public:
	ANPBlackoutMapEvent();

protected:
	virtual void ApplyEventState_Implementation(bool bNewActive) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blackout")
	FName LightTag = TEXT("EventLight");

private:
	struct FLightVisibilityState
	{
		TWeakObjectPtr<ULightComponent> LightComponent;
		bool bWasVisible = true;
	};

	void ApplyBlackout();
	void RestoreLights();

	TArray<FLightVisibilityState> SavedLightStates;
};
