#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPMainWorldLoadingWidget.generated.h"

class UTextBlock;

/** 맵 전환이 끝난 뒤 방 Level Instance와 네트워크 준비를 기다리는 일반 UMG 오버레이입니다. */
UCLASS()
class NOPHOTOS_API UNPMainWorldLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Main World Loading")
	void ShowLoading();

	UFUNCTION(BlueprintCallable, Category="Main World Loading")
	void ShowFailure();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadingStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Main World Loading")
	FText LoadingText = NSLOCTEXT("NoPhotos", "MainWorldLoading", "Loading rooms...");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Main World Loading")
	FText FailureText = NSLOCTEXT("NoPhotos", "MainWorldLoadingFailed", "Failed to load the game world.");
};
