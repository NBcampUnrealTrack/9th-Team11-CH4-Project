#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "NPNameplateComponent.generated.h"

class APlayerState;
class UNPUserNameWidget;

UCLASS(Blueprintable, ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPNameplateComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNPNameplateComponent();

	UFUNCTION(BlueprintCallable, Category="Nameplate")
	void SetNameplateVisible(bool bShouldBeVisible);
	UFUNCTION(BlueprintCallable, Category="Nameplate")
	bool RefreshNameplate();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool bHiddenByGameplay = false;

	void UpdateNameplateVisibility();
	void UpdateFacingCamera();

	UPROPERTY(Transient)
	TObjectPtr<UNPUserNameWidget> NameplateWidget;

	UPROPERTY(Transient)
	TObjectPtr<APlayerState> BoundPlayerState;
};
