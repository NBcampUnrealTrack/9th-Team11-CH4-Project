#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPLadderActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;

/** 가까운 캐릭터가 전진 입력만으로 오를 수 있는 사다리 영역입니다. */
UCLASS()
class NOPHOTOS_API ANPLadderActor : public AActor
{
	GENERATED_BODY()

public:
	ANPLadderActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> ClimbVolume;

private:
	UFUNCTION()
	void HandleClimbVolumeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleClimbVolumeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);
};
