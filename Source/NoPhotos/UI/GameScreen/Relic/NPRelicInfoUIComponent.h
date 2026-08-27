#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "NPRelicInfoUIComponent.generated.h"

class ANPBaseRelic;
class APawn;
class APlayerController;
class UPrimitiveComponent;
class UNPStablePhysicsGrabComponent;
class UNPRelicInfoWidget;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPRelicInfoUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPRelicInfoUIComponent();

	//애니메이션용 퇴장시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic Info UI")
	float PopAnimationDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relic Info UI")
	TSubclassOf<UNPRelicInfoWidget> RelicInfoWidgetClass;
	
	UFUNCTION(BlueprintCallable, Category="Relic Info UI")
	void ShowRelicInfo(ANPBaseRelic* Relic);
	UFUNCTION(BlueprintCallable, Category="Relic Info UI")
	void HideRelicInfo();

	UFUNCTION(BlueprintPure, Category="Relic Info UI")
	UNPRelicInfoWidget* GetRelicInfoWidget() const { return RelicInfoWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	bool CreateRelicInfoWidget();
	APlayerController* GetLocalPlayerController() const;
	void BindToGrabComponent(APawn* Pawn);
	void UnbindFromGrabComponent();
	void HandleGrabbedComponentChanged(UPrimitiveComponent* GrabbedComponent);
	void CompleteRelicInfoPop();
	void ClearPopTimer();

	UPROPERTY(Transient)
	TObjectPtr<UNPRelicInfoWidget> RelicInfoWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNPStablePhysicsGrabComponent> GrabComponent;

	FTimerHandle PopTimerHandle;
};
