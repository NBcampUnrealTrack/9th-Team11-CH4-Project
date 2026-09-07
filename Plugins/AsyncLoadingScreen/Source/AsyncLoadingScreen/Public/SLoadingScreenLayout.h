/************************************************************************************
 *																					*
 * Copyright (C) 2020 Truong Bui.													*
 * Website:	https://github.com/truong-bui/AsyncLoadingScreen						*
 * Licensed under the MIT License. See 'LICENSE' file for full license information. *
 *																					*
 ************************************************************************************/

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Engine/GameViewportClient.h"

//DECLARE_LOG_CATEGORY_EXTERN(LogLoadingScreen, Log, All);
/**
 * SLoadingScreenLayout
 * 
 * This is the base class for all loading screen layouts.
 * It provides common functionality such as DPI scaling and point size conversion.
 * 
 * Note: This class is not intended to be instantiated directly.
 * Instead, use derived classes that implement specific loading screen layouts.
 *
 */
class ASYNCLOADINGSCREEN_API SLoadingScreenLayout : public SCompoundWidget
{
public:	
	static float PointSizeToSlateUnits(float PointSize);
	void SetIsMoviePlayerFadeEnabled(bool NewIsMoviePlayerFadeEnabled);
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
protected:
	float GetDPIScale() const;
	void CalculateViewportSize() const;
private:
	static constexpr float FadeDurationSeconds = 0.75f;
	float FadeAlpha = 0.0f;
	bool IsMoviePlayerFadeEnabled = true;
	bool IsFadeOutStarted = false;
	mutable FIntPoint _cachedViewportSize;
};
