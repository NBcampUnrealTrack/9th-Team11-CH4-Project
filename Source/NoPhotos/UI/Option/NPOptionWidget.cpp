#include "UI/Option/NPOptionWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/SizeBox.h"
#include "GameFramework/GameUserSettings.h"

namespace NPOptionWidget
{
	constexpr int32 LowQuality = 0;
	constexpr int32 MediumQuality = 1;
	constexpr int32 HighQuality = 2;
	constexpr int32 EpicQuality = 3;

	const TArray<FIntPoint> Resolutions =
	{
		FIntPoint(1920, 1080),
		FIntPoint(1600, 900),
		FIntPoint(1280, 720)
	};

	const TArray<FString> WindowModeLabels =
	{
		TEXT("전체화면"),
		TEXT("전체 창모드"),
		TEXT("창모드")
	};

	const TArray<EWindowMode::Type> WindowModes =
	{
		EWindowMode::Fullscreen,
		EWindowMode::WindowedFullscreen,
		EWindowMode::Windowed
	};
}

void UNPOptionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeGraphicsQualityButtons();
	InitializeResolutionOptions();
	InitializeWindowModeOptions();
	UpdateResolutionControlState();

	if (IsValid(LowQualityRadioButton))
	{
		LowQualityRadioButton->OnCheckStateChanged.AddDynamic(this, &UNPOptionWidget::OnLowQualityChanged);
	}

	if (IsValid(MediumQualityRadioButton))
	{
		MediumQualityRadioButton->OnCheckStateChanged.AddDynamic(this, &UNPOptionWidget::OnMediumQualityChanged);
	}

	if (IsValid(HighQualityRadioButton))
	{
		HighQualityRadioButton->OnCheckStateChanged.AddDynamic(this, &UNPOptionWidget::OnHighQualityChanged);
	}

	if (IsValid(EpicQualityRadioButton))
	{
		EpicQualityRadioButton->OnCheckStateChanged.AddDynamic(this, &UNPOptionWidget::OnEpicQualityChanged);
	}

	if (IsValid(ResolutionComboBox))
	{
		ResolutionComboBox->OnSelectionChanged.AddDynamic(this, &UNPOptionWidget::OnResolutionSelected);
	}

	if (IsValid(WindowModeComboBox))
	{
		WindowModeComboBox->OnSelectionChanged.AddDynamic(this, &UNPOptionWidget::OnWindowModeSelected);
	}

	if (IsValid(ApplyButton))
	{
		ApplyButton->OnClicked.AddDynamic(this, &UNPOptionWidget::OnApplyClicked);
	}
}

void UNPOptionWidget::NativeDestruct()
{
	if (IsValid(LowQualityRadioButton))
	{
		LowQualityRadioButton->OnCheckStateChanged.RemoveAll(this);
	}

	if (IsValid(MediumQualityRadioButton))
	{
		MediumQualityRadioButton->OnCheckStateChanged.RemoveAll(this);
	}

	if (IsValid(HighQualityRadioButton))
	{
		HighQualityRadioButton->OnCheckStateChanged.RemoveAll(this);
	}

	if (IsValid(EpicQualityRadioButton))
	{
		EpicQualityRadioButton->OnCheckStateChanged.RemoveAll(this);
	}

	if (IsValid(ResolutionComboBox))
	{
		ResolutionComboBox->OnSelectionChanged.RemoveAll(this);
	}

	if (IsValid(WindowModeComboBox))
	{
		WindowModeComboBox->OnSelectionChanged.RemoveAll(this);
	}

	if (IsValid(ApplyButton))
	{
		ApplyButton->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UNPOptionWidget::InitializeGraphicsQualityButtons()
{
	UGameUserSettings* GameUserSettings = UGameUserSettings::GetGameUserSettings();
	if (!IsValid(GameUserSettings))
	{
		return;
	}

	SelectedGraphicsQuality = GameUserSettings->GetOverallScalabilityLevel();
	if (SelectedGraphicsQuality < NPOptionWidget::LowQuality
		|| SelectedGraphicsQuality > NPOptionWidget::EpicQuality)
	{
		SelectedGraphicsQuality = NPOptionWidget::EpicQuality;
	}

	UpdateGraphicsQualityButtons();
}

void UNPOptionWidget::InitializeResolutionOptions()
{
	UGameUserSettings* GameUserSettings = UGameUserSettings::GetGameUserSettings();
	if (!IsValid(GameUserSettings))
	{
		return;
	}

	SelectedResolutionIndex = NPOptionWidget::Resolutions.IndexOfByKey(GameUserSettings->GetScreenResolution());
	if (!NPOptionWidget::Resolutions.IsValidIndex(SelectedResolutionIndex))
	{
		SelectedResolutionIndex = 0;
	}
}

void UNPOptionWidget::InitializeWindowModeOptions()
{
	if (!IsValid(WindowModeComboBox))
	{
		return;
	}

	WindowModeComboBox->ClearOptions();
	for (const FString& Label : NPOptionWidget::WindowModeLabels)
	{
		WindowModeComboBox->AddOption(Label);
	}

	UGameUserSettings* GameUserSettings = UGameUserSettings::GetGameUserSettings();
	if (!IsValid(GameUserSettings))
	{
		return;
	}

	SelectedWindowModeIndex = NPOptionWidget::WindowModes.IndexOfByKey(GameUserSettings->GetFullscreenMode());
	WindowModeComboBox->SetSelectedIndex(SelectedWindowModeIndex);
}

void UNPOptionWidget::OnLowQualityChanged(const bool bIsChecked)
{
	HandleGraphicsQualityChanged(NPOptionWidget::LowQuality, bIsChecked);
}

void UNPOptionWidget::OnMediumQualityChanged(const bool bIsChecked)
{
	HandleGraphicsQualityChanged(NPOptionWidget::MediumQuality, bIsChecked);
}

void UNPOptionWidget::OnHighQualityChanged(const bool bIsChecked)
{
	HandleGraphicsQualityChanged(NPOptionWidget::HighQuality, bIsChecked);
}

void UNPOptionWidget::OnEpicQualityChanged(const bool bIsChecked)
{
	HandleGraphicsQualityChanged(NPOptionWidget::EpicQuality, bIsChecked);
}

void UNPOptionWidget::OnResolutionSelected(const FString, const ESelectInfo::Type)
{
	if (IsValid(ResolutionComboBox))
	{
		SelectedResolutionIndex = ResolutionComboBox->GetSelectedIndex();
	}
}

void UNPOptionWidget::OnWindowModeSelected(const FString, const ESelectInfo::Type)
{
	if (IsValid(WindowModeComboBox))
	{
		SelectedWindowModeIndex = WindowModeComboBox->GetSelectedIndex();
		UpdateResolutionControlState();
	}
}

void UNPOptionWidget::HandleGraphicsQualityChanged(const int32 QualityLevel, const bool bIsChecked)
{
	if (bIsChecked)
	{
		SelectedGraphicsQuality = QualityLevel;
	}

	UpdateGraphicsQualityButtons();
}

void UNPOptionWidget::UpdateGraphicsQualityButtons()
{
	if (IsValid(LowQualityRadioButton))
	{
		LowQualityRadioButton->SetIsChecked(SelectedGraphicsQuality == NPOptionWidget::LowQuality);
	}

	if (IsValid(MediumQualityRadioButton))
	{
		MediumQualityRadioButton->SetIsChecked(SelectedGraphicsQuality == NPOptionWidget::MediumQuality);
	}

	if (IsValid(HighQualityRadioButton))
	{
		HighQualityRadioButton->SetIsChecked(SelectedGraphicsQuality == NPOptionWidget::HighQuality);
	}

	if (IsValid(EpicQualityRadioButton))
	{
		EpicQualityRadioButton->SetIsChecked(SelectedGraphicsQuality == NPOptionWidget::EpicQuality);
	}
}

void UNPOptionWidget::UpdateResolutionControlState()
{
	const bool bCanSelectResolution = NPOptionWidget::WindowModes.IsValidIndex(SelectedWindowModeIndex)
		&& NPOptionWidget::WindowModes[SelectedWindowModeIndex] == EWindowMode::Windowed;
	const ESlateVisibility ResolutionVisibility = bCanSelectResolution
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed;

	if (IsValid(SizeBox_6))
	{
		SizeBox_6->SetVisibility(ResolutionVisibility);
	}

	if (IsValid(SizeBox_1))
	{
		SizeBox_1->SetVisibility(ResolutionVisibility);
	}

	if (!IsValid(ResolutionComboBox))
	{
		return;
	}

	ResolutionComboBox->ClearOptions();
	ResolutionComboBox->SetIsEnabled(bCanSelectResolution);

	if (!bCanSelectResolution)
	{
		UGameUserSettings* GameUserSettings = UGameUserSettings::GetGameUserSettings();
		if (IsValid(GameUserSettings))
		{
			const FIntPoint DesktopResolution = GameUserSettings->GetDesktopResolution();
			ResolutionComboBox->AddOption(
				FString::Printf(TEXT("%d * %d"), DesktopResolution.X, DesktopResolution.Y));
			ResolutionComboBox->SetSelectedIndex(0);
		}
		return;
	}

	for (const FIntPoint& Resolution : NPOptionWidget::Resolutions)
	{
		ResolutionComboBox->AddOption(FString::Printf(TEXT("%d * %d"), Resolution.X, Resolution.Y));
	}

	if (!NPOptionWidget::Resolutions.IsValidIndex(SelectedResolutionIndex))
	{
		SelectedResolutionIndex = 0;
	}
	ResolutionComboBox->SetSelectedIndex(SelectedResolutionIndex);
}

void UNPOptionWidget::OnApplyClicked()
{
	UGameUserSettings* GameUserSettings = UGameUserSettings::GetGameUserSettings();
	if (!IsValid(GameUserSettings))
	{
		return;
	}

	if (SelectedGraphicsQuality >= NPOptionWidget::LowQuality
		&& SelectedGraphicsQuality <= NPOptionWidget::EpicQuality)
	{
		GameUserSettings->SetOverallScalabilityLevel(SelectedGraphicsQuality);
	}

	if (NPOptionWidget::WindowModes.IsValidIndex(SelectedWindowModeIndex))
	{
		GameUserSettings->SetFullscreenMode(NPOptionWidget::WindowModes[SelectedWindowModeIndex]);
	}

	const bool bUsesDesktopResolution = NPOptionWidget::WindowModes.IsValidIndex(SelectedWindowModeIndex)
		&& NPOptionWidget::WindowModes[SelectedWindowModeIndex] != EWindowMode::Windowed;
	if (bUsesDesktopResolution)
	{
		GameUserSettings->SetScreenResolution(GameUserSettings->GetDesktopResolution());
	}
	else if (NPOptionWidget::Resolutions.IsValidIndex(SelectedResolutionIndex))
	{
		GameUserSettings->SetScreenResolution(NPOptionWidget::Resolutions[SelectedResolutionIndex]);
	}

	GameUserSettings->ApplySettings(false);
	GameUserSettings->SaveSettings();
}
