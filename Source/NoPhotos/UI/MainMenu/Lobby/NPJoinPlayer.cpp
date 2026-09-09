#include "UI/MainMenu/Lobby/NPJoinPlayer.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerState.h"
#include "OnlineSubsystemNames.h"
#include "TimerManager.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

void UNPJoinPlayer::SetupResult(const FString& InPlayerName, APlayerState* InPlayerState)
{
	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(FText::FromString(InPlayerName));
	}

	if (TargetPlayerState.Get() == InPlayerState)
	{
		return;
	}

	TargetPlayerState = InPlayerState;
	AvatarLoadAttemptCount = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AvatarLoadTimer);
	}
	TryLoadSteamAvatar();
}

void UNPJoinPlayer::PlayJoinAnimation(const float Delay)
{
	if (!IsValid(JoinPoster))
	{
		return;
	}

	if (Delay <= 0.0f)
	{
		StartJoinAnimation();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			JoinAnimationTimer,
			this,
			&UNPJoinPlayer::StartJoinAnimation,
			Delay,
			false);
	}
}

void UNPJoinPlayer::PlayLeaveAnimation()
{
	if (bIsLeaving)
	{
		return;
	}

	bIsLeaving = true;
	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(FText::FromString(TEXT("Unknown")));
	}

	if (IsValid(LeavePoster))
	{
		PlayAnimation(LeavePoster);
		return;
	}

	HandleLeaveAnimationFinished();
}

void UNPJoinPlayer::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(LeavePoster))
	{
		FWidgetAnimationDynamicEvent LeaveAnimationFinishedDelegate;
		LeaveAnimationFinishedDelegate.BindDynamic(this, &UNPJoinPlayer::HandleLeaveAnimationFinished);
		BindToAnimationFinished(LeavePoster, LeaveAnimationFinishedDelegate);
	}
}

void UNPJoinPlayer::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JoinAnimationTimer);
		World->GetTimerManager().ClearTimer(AvatarLoadTimer);
	}
	if (IsValid(LeavePoster))
	{
		UnbindAllFromAnimationFinished(LeavePoster);
	}

	Super::NativeDestruct();
}

void UNPJoinPlayer::StartJoinAnimation()
{
	if (IsValid(JoinPoster))
	{
		PlayAnimation(JoinPoster);
	}
}

void UNPJoinPlayer::TryLoadSteamAvatar()
{
	if (!IsValid(PlayerAvatarImage) || !TargetPlayerState.IsValid())
	{
		return;
	}

	const FUniqueNetIdRepl& UniqueId = TargetPlayerState->GetUniqueId();
	if (!UniqueId.IsValid())
	{
		ScheduleAvatarLoadRetry();
		return;
	}
	if (UniqueId.GetType() != STEAM_SUBSYSTEM)
	{
		return;
	}

	ISteamFriends* SteamFriendsInterface = SteamFriends();
	ISteamUtils* SteamUtilsInterface = SteamUtils();
	if (!SteamFriendsInterface || !SteamUtilsInterface)
	{
		ScheduleAvatarLoadRetry();
		return;
	}

	const uint64 SteamIdValue = FCString::Strtoui64(*UniqueId.ToString(), nullptr, 10);
	const CSteamID SteamId(SteamIdValue);
	if (!SteamId.IsValid())
	{
		return;
	}

	const int32 AvatarHandle = SteamFriendsInterface->GetMediumFriendAvatar(SteamId);
	if (AvatarHandle <= 0)
	{
		SteamFriendsInterface->RequestUserInformation(SteamId, false);
		ScheduleAvatarLoadRetry();
		return;
	}

	uint32 Width = 0;
	uint32 Height = 0;
	if (!SteamUtilsInterface->GetImageSize(AvatarHandle, &Width, &Height) || Width == 0 || Height == 0)
	{
		ScheduleAvatarLoadRetry();
		return;
	}

	TArray64<uint8> ImageData;
	ImageData.SetNumUninitialized(static_cast<int64>(Width) * Height * 4);
	if (!SteamUtilsInterface->GetImageRGBA(
		AvatarHandle,
		ImageData.GetData(),
		static_cast<int32>(ImageData.Num())))
	{
		ScheduleAvatarLoadRetry();
		return;
	}

	for (int64 PixelIndex = 0; PixelIndex < ImageData.Num(); PixelIndex += 4)
	{
		Swap(ImageData[PixelIndex], ImageData[PixelIndex + 2]);
	}

	PlayerAvatarTexture = UTexture2D::CreateTransient(
		static_cast<int32>(Width),
		static_cast<int32>(Height),
		PF_B8G8R8A8,
		NAME_None,
		ImageData);
	if (IsValid(PlayerAvatarTexture))
	{
		PlayerAvatarImage->SetBrushFromTexture(PlayerAvatarTexture);
	}
}

void UNPJoinPlayer::ScheduleAvatarLoadRetry()
{
	constexpr int32 MaxAvatarLoadAttempts = 100;
	constexpr float AvatarLoadRetryInterval = 0.2f;

	if (++AvatarLoadAttemptCount >= MaxAvatarLoadAttempts)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AvatarLoadTimer,
			this,
			&UNPJoinPlayer::TryLoadSteamAvatar,
			AvatarLoadRetryInterval,
			false);
	}
}

void UNPJoinPlayer::HandleLeaveAnimationFinished()
{
	OnLeaveAnimationFinished.Broadcast(this);
}
