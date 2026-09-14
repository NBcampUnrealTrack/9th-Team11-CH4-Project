#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPRelicUsePromptUIComponent.generated.h"

class UNPRelicUsePromptWidget;

/** Room과 Main PlayerController에서 공통으로 사용하는 유물 사용 안내 HUD입니다. */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class NOPHOTOS_API UNPRelicUsePromptUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPRelicUsePromptUIComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateRelicUsePrompt();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Use Prompt",
		meta=(AllowPrivateAccess="true"))
	TSubclassOf<UNPRelicUsePromptWidget> RelicUsePromptWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Relic Use Prompt",
		meta=(AllowPrivateAccess="true", ClampMin="0.02", UIMin="0.02", Units="s"))
	float UpdateInterval = 0.05f;

	UPROPERTY(Transient)
	TObjectPtr<UNPRelicUsePromptWidget> RelicUsePromptWidget;

	FTimerHandle UpdateTimerHandle;
};
