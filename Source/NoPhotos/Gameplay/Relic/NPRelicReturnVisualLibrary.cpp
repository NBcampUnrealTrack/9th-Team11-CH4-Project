#include "Gameplay/Relic/NPRelicReturnVisualLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

void UNPRelicReturnVisualLibrary::ConfigureAndAnimateRelicReturnVisual(
    AActor* VisualActor,
    UStaticMeshComponent* VisualMesh,
    UStaticMeshComponent* SourceMeshComponent,
    const FVector StartLocation,
    const FVector TargetLocation,
    const float Duration)
{
    if (!IsValid(VisualActor) || !IsValid(VisualMesh) || !IsValid(SourceMeshComponent))
    {
        return;
    }

    VisualMesh->SetStaticMesh(SourceMeshComponent->GetStaticMesh());
    for (int32 MaterialIndex = 0; MaterialIndex < SourceMeshComponent->GetNumMaterials(); ++MaterialIndex)
    {
        VisualMesh->SetMaterial(MaterialIndex, SourceMeshComponent->GetMaterial(MaterialIndex));
    }

    VisualActor->SetActorLocation(StartLocation);
    VisualMesh->SetWorldRotation(SourceMeshComponent->GetComponentRotation());
    VisualMesh->SetWorldScale3D(SourceMeshComponent->GetComponentScale());
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetSimulatePhysics(false);
    VisualMesh->SetVisibility(true, true);

    UWorld* World = VisualActor->GetWorld();
    if (!IsValid(World) || Duration <= KINDA_SMALL_NUMBER)
    {
        VisualActor->SetActorLocation(TargetLocation);
        VisualActor->Destroy();
        return;
    }

    const TWeakObjectPtr<AActor> WeakActor(VisualActor);
    const TSharedRef<float> ElapsedTime = MakeShared<float>(0.0f);
    const TSharedRef<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();
    const float TickInterval = 1.0f / 60.0f;

    FTimerDelegate MoveDelegate;
    MoveDelegate.BindLambda([WeakActor, StartLocation, TargetLocation, Duration, ElapsedTime, TimerHandle]()
    {
        AActor* Actor = WeakActor.Get();
        if (!IsValid(Actor))
        {
            return;
        }

        UWorld* ActorWorld = Actor->GetWorld();
        if (!IsValid(ActorWorld))
        {
            return;
        }

        *ElapsedTime += ActorWorld->GetDeltaSeconds();
        const float Alpha = FMath::Clamp(*ElapsedTime / Duration, 0.0f, 1.0f);
        const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
        Actor->SetActorLocation(FMath::Lerp(StartLocation, TargetLocation, EasedAlpha));

        if (Alpha >= 1.0f)
        {
            ActorWorld->GetTimerManager().ClearTimer(*TimerHandle);
            Actor->Destroy();
        }
    });

    World->GetTimerManager().SetTimer(*TimerHandle, MoveDelegate, TickInterval, true);
}
