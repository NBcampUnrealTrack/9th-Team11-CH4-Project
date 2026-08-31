//유물 정보 띄우기용 라인 트레이스
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPRelicHoverFocusComponent.generated.h"

class UNPRelicHoverInfoComponent;

UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPRelicHoverFocusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPRelicHoverFocusComponent();

	UFUNCTION(BlueprintCallable, Category="Relic Hover Focus")
	void RefreshFocusedRelic();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relic Hover Focus", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float TraceDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relic Hover Focus")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relic Hover Focus", meta=(ClampMin="0.01", ClampMax="1.0"))
	float TraceInterval = 0.05f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetFocusedHoverInfoComponent(UNPRelicHoverInfoComponent* NewFocusedComponent);

	float ElapsedTraceTime = 0.0f;
	TWeakObjectPtr<UNPRelicHoverInfoComponent> FocusedHoverInfoComponent;
};
