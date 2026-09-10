#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPPersonalResultWidget.generated.h"

class APlayerState;
class UHorizontalBox;
class UTextBlock;
class UNPResultPictureButton;

UCLASS()
class NOPHOTOS_API UNPPersonalResultWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	void SetupResult(int32 InRank, const FString& InPlayerName,	int32 InScore,	APlayerState* InPlayerState);
	void RefreshPictureButtons();

private:
	void CreatePictureButtons();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RankText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PictureList;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ScoreText;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNPResultPictureButton> PictureButtonWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<APlayerState> ResultPlayerState;
};
