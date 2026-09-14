#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPMainMenuWidget.generated.h"

class UButton;
class UBorder;
class UImage;
class UTexture2D;
class UWidgetAnimation;

UCLASS()
class NOPHOTOS_API UNPMainMenuWidget : public UNPUserWidget
{
	GENERATED_BODY()
	
public:
	UNPMainMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UNPUserWidget> RoomListWidgetClass;
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HostButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> JoinButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ExitButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image;
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> ImageAnim;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Image Carousel")
	TArray<TObjectPtr<UTexture2D>> CarouselImages;

	int32 CurrentCarouselImageIndex = 0;

	UFUNCTION()
	void OnHostGameClicked();
	UFUNCTION()
	void OnJoinGameClicked();
	UFUNCTION()
	void OnExitClicked();
	void PlayNextCarouselImage();
	void ShowConnectionFailureMessage(const FText& Message);
	void HideConnectionFailureMessage();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ConnectionFailureBanner;
	FTimerHandle ConnectionFailureMessageTimer;
};
