#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPResultWidget.generated.h"

class UButton;
class USoundBase;
class ANPMainGameState;

UCLASS()
class NOPHOTOS_API UNPResultWidget : public UNPUserWidget
{
	GENERATED_BODY()
	
public:
	UNPResultWidget(const FObjectInitializer& ObjectInitializer);
	
protected:    	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 사진 한 장이 가능한 모든 좋아요를 받았을 때 각 클라이언트에서 재생합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Result|Like")
	TObjectPtr<USoundBase> FullyLikedSound;
    
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RetryButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ExitButton;
    
	UFUNCTION()
	void OnRetryClicked();
	UFUNCTION()
	void OnExitClicked();
	UFUNCTION()
	void HandlePhotoFullyLiked(FGuid PhotoId, int32 LikeCount);

	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedGameState;
};
