#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/NPUserWidget.h"
#include "NPOptionWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class USizeBox;

UCLASS()
class NOPHOTOS_API UNPOptionWidget : public UNPUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void OnLowQualityChanged(bool bIsChecked);

	UFUNCTION()
	void OnMediumQualityChanged(bool bIsChecked);

	UFUNCTION()
	void OnHighQualityChanged(bool bIsChecked);

	UFUNCTION()
	void OnEpicQualityChanged(bool bIsChecked);

	UFUNCTION()
	void OnResolutionSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnWindowModeSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnApplyClicked();

	void InitializeGraphicsQualityButtons();
	void InitializeResolutionOptions();
	void InitializeWindowModeOptions();
	void HandleGraphicsQualityChanged(int32 QualityLevel, bool bIsChecked);
	void UpdateGraphicsQualityButtons();
	void UpdateResolutionControlState();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> LowQualityRadioButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> MediumQualityRadioButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> HighQualityRadioButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> EpicQualityRadioButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ResolutionComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> WindowModeComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_6;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ApplyButton;

	int32 SelectedGraphicsQuality = INDEX_NONE;
	int32 SelectedResolutionIndex = INDEX_NONE;
	int32 SelectedWindowModeIndex = INDEX_NONE;
};
