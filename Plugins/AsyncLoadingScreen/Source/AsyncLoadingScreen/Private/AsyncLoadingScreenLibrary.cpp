/************************************************************************************
 *																					*
 * Copyright (C) 2020 Truong Bui.													*
 * Website:	https://github.com/truong-bui/AsyncLoadingScreen						*
 * Licensed under the MIT License. See 'LICENSE' file for full license information. *
 *																					*
 ************************************************************************************/


#include "AsyncLoadingScreenLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "MoviePlayer.h"
#include "AsyncLoadingScreen.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

namespace AsyncLoadingScreenHandoff
{
	TSharedPtr<SWidget> OverlayWidget;
	TWeakObjectPtr<UGameViewportClient> OwningViewport;
}

int32 UAsyncLoadingScreenLibrary::DisplayBackgroundIndex = -1;
int32 UAsyncLoadingScreenLibrary::DisplayTipTextIndex = -1;
int32 UAsyncLoadingScreenLibrary::DisplayMovieIndex = -1;
bool  UAsyncLoadingScreenLibrary::bShowLoadingScreen = true;

void UAsyncLoadingScreenLibrary::SetDisplayBackgroundIndex(int32 BackgroundIndex)
{
	UAsyncLoadingScreenLibrary::DisplayBackgroundIndex = BackgroundIndex;
}

void UAsyncLoadingScreenLibrary::SetDisplayTipTextIndex(int32 TipTextIndex)
{
	UAsyncLoadingScreenLibrary::DisplayTipTextIndex = TipTextIndex;
}

void UAsyncLoadingScreenLibrary::SetDisplayMovieIndex(int32 MovieIndex)
{
	UAsyncLoadingScreenLibrary::DisplayMovieIndex = MovieIndex;	
}

void UAsyncLoadingScreenLibrary::SetEnableLoadingScreen(bool bIsEnableLoadingScreen)
{
	bShowLoadingScreen = bIsEnableLoadingScreen;
}

void UAsyncLoadingScreenLibrary::StopLoadingScreen()
{
	GetMoviePlayer()->StopMovie();
}

void UAsyncLoadingScreenLibrary::ShowTransitionHandoffOverlay()
{
	if (IsRunningDedicatedServer()
		|| AsyncLoadingScreenHandoff::OverlayWidget.IsValid()
		|| !GEngine
		|| !GEngine->GameViewport)
	{
		return;
	}

	AsyncLoadingScreenHandoff::OverlayWidget =
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Black)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT(
				"AsyncLoadingScreen",
				"TransitionHandoffLoadingRooms",
				"Loading Rooms..."))
			.ColorAndOpacity(FLinearColor::White)
		];

	AsyncLoadingScreenHandoff::OwningViewport = GEngine->GameViewport;
	GEngine->GameViewport->AddViewportWidgetContent(
		AsyncLoadingScreenHandoff::OverlayWidget.ToSharedRef(),
		MAX_int32 - 1);
}

void UAsyncLoadingScreenLibrary::HideTransitionHandoffOverlay()
{
	UGameViewportClient* Viewport =
		AsyncLoadingScreenHandoff::OwningViewport.Get();
	if (Viewport && AsyncLoadingScreenHandoff::OverlayWidget.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(
			AsyncLoadingScreenHandoff::OverlayWidget.ToSharedRef());
	}

	AsyncLoadingScreenHandoff::OverlayWidget.Reset();
	AsyncLoadingScreenHandoff::OwningViewport.Reset();
}

