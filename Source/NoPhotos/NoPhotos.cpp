// Copyright Epic Games, Inc. All Rights Reserved.

#include "NoPhotos.h"
#include "Modules/ModuleManager.h"

#if !WITH_EDITOR
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CoreDelegates.h"
#endif

class FNoPhotosModule : public FDefaultGameModuleImpl
{
public:
#if !WITH_EDITOR
	virtual void StartupModule() override
	{
		FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FNoPhotosModule::ApplyFrameRateLimit);
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	}

private:
	void ApplyFrameRateLimit()
	{
		if (IsRunningDedicatedServer() || !GEngine)
		{
			return;
		}

		if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
		{
			Settings->SetFrameRateLimit(60.0f);
			Settings->ApplyNonResolutionSettings();
		}
	}
#endif
};

IMPLEMENT_PRIMARY_GAME_MODULE( FNoPhotosModule, NoPhotos, "NoPhotos" );

DEFINE_LOG_CATEGORY(LogNoPhotos)
