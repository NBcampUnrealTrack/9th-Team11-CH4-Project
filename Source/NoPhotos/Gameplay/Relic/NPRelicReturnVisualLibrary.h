#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NPRelicReturnVisualLibrary.generated.h"

class UStaticMeshComponent;

/** Runtime helpers used by the generated BP_RelicReturnVisual graph. */
UCLASS()
class NOPHOTOS_API UNPRelicReturnVisualLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "NoPhotos|Relic", meta = (DefaultToSelf = "VisualActor"))
    static void ConfigureAndAnimateRelicReturnVisual(
        AActor* VisualActor,
        UStaticMeshComponent* VisualMesh,
        UStaticMeshComponent* SourceMeshComponent,
        FVector StartLocation,
        FVector TargetLocation,
        float Duration = 1.0f);
};
