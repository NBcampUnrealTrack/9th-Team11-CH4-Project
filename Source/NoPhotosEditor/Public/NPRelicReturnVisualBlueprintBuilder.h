#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NPRelicReturnVisualBlueprintBuilder.generated.h"

/** One-shot editor automation for BP_RelicReturnVisual. */
UCLASS()
class NOPHOTOSEDITOR_API UNPRelicReturnVisualBlueprintBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "NoPhotos|Editor")
    static bool BuildRelicReturnVisualBlueprint();
};
