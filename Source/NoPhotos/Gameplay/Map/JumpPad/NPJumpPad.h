#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPJumpPad.generated.h"

class APawn;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class NOPHOTOS_API ANPJumpPad : public AActor
{
	GENERATED_BODY()

public:
	ANPJumpPad();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleLaunchOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
	TObjectPtr<UBoxComponent> LaunchVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump Pad", meta = (ClampMin = "0.0"))
	float Cooldown = 1.0f;

private:
	bool IsOnCooldown(const APawn* Pawn) const;
	void RecordLaunch(APawn* Pawn);
	bool LaunchPawn(APawn* Pawn) const;

	TMap<TWeakObjectPtr<APawn>, double> LastLaunchTimes;
};
