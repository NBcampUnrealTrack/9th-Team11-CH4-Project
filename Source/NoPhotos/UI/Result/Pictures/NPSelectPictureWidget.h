//사진 선택창 최종 HUD

#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPSelectPictureWidget.generated.h"

class ANPMainGameState;
class UButton;
class UTextBlock;
class UTexture2D;
class UNPPhotoCaptureComponent;
class UNPPictureList;
class UNPSelectedPictureListWidget;
class UNPShowPicture;

UCLASS()
class NOPHOTOS_API UNPSelectPictureWidget : public UNPUserWidget
{
	GENERATED_BODY()

public:
	//테스트용 직접 초기화 함수입니다.
	UFUNCTION(BlueprintCallable, Category = "Picture")
	void InitializePictures(const TArray<UTexture2D*>& InPictures);

	//정산 서버 전송에 사용하는 실제 PhotoId 목록
	UFUNCTION(BlueprintPure, Category = "Picture")
	TArray<FGuid> GetSelectedPhotoIds() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Picture")
	void BP_OnSelectionConfirmed(const TArray<int32>& SelectedPictureIndices);

private:
	UFUNCTION()
	void HandlePictureClicked(int32 PictureIndex);
	UFUNCTION()
	void HandleSelectRequested(int32 PictureIndex);
	UFUNCTION()
	void HandleSelectedPictureClicked(int32 PictureIndex);
	UFUNCTION()
	void HandleSelectedPictureRemoveRequested(int32 PictureIndex);
	UFUNCTION()
	void HandleNextButtonClicked();
	UFUNCTION()
	void HandlePhotoEvidenceChanged();
	UFUNCTION()
	void HandlePictureSelectionStateChanged();

	void RequestOwnedPictures();
	void ShowPicture(int32 PictureIndex);
	void SetPictureSelectionEnabled(bool bEnabled);
	void UpdateSelectedPictureCountText();
	void UpdateNextPlayerText();
	TArray<int32> GetSelectedPictureIndices() const;

	UPROPERTY(EditDefaultsOnly, Category = "Picture", meta = (ClampMin = "1"))
	int32 MaxSelectedPictureCount = 5;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNPShowPicture> ShowPictureWidget;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNPPictureList> PictureListWidget;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNPSelectedPictureListWidget> SelectedPictureListWidget;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SelectedPictureCountText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> NextButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> IsNextPlayer;

	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedGameState;
	UPROPERTY(Transient)
	TObjectPtr<ANPMainGameState> ObservedMainGameState;
	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoCaptureComponent> PhotoCaptureComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> PictureTextures;

	UPROPERTY(Transient)
	TArray<FGuid> PicturePhotoIds;
	//중복처리 방지
	TSet<FGuid> RequestedPhotoIds;

	bool bWaitingForOtherPlayers = false;
	UPROPERTY(Transient)
	int32 CurrentPictureIndex = INDEX_NONE;
};
