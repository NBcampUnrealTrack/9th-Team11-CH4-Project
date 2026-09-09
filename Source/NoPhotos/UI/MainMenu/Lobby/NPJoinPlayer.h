#pragma once

#include "CoreMinimal.h"
#include "UI/NPUserWidget.h"
#include "NPJoinPlayer.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;
class UWidgetAnimation;
class UNPJoinPlayer;
class APlayerState;

DECLARE_MULTICAST_DELEGATE_OneParam(FNPOnJoinPlayerLeaveAnimationFinished, UNPJoinPlayer*);

UCLASS()
class NOPHOTOS_API UNPJoinPlayer : public UNPUserWidget
{
	GENERATED_BODY()
	
public:
	void SetupResult(const FString& InPlayerName, APlayerState* InPlayerState);
	void PlayJoinAnimation(float Delay);
	void PlayLeaveAnimation();
	bool IsLeaving() const { return bIsLeaving; }

	FNPOnJoinPlayerLeaveAnimationFinished OnLeaveAnimationFinished;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void StartJoinAnimation();
	void TryLoadSteamAvatar();
	void ScheduleAvatarLoadRetry();

	UFUNCTION()
	void HandleLeaveAnimationFinished();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PlayerAvatarImage;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PlayerAvatarTexture;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> JoinPoster;
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> LeavePoster;

	FTimerHandle JoinAnimationTimer;
	FTimerHandle AvatarLoadTimer;
	TWeakObjectPtr<APlayerState> TargetPlayerState;
	int32 AvatarLoadAttemptCount = 0;
	bool bIsLeaving = false;
};
