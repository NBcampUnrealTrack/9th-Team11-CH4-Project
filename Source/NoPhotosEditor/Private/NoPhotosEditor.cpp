#include "NoPhotosEditor.h"

#include "Goblin/NPGoblinCharacterDetails.h"
#include "PropertyEditorModule.h"
#include "Santa/NPSantaEventDefinitionDetails.h"

#define LOCTEXT_NAMESPACE "FNoPhotosEditorModule"

void FNoPhotosEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("NPGoblinCharacter"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FNPGoblinCharacterDetails::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("NPSantaEventDefinition"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FNPSantaEventDefinitionDetails::MakeInstance));
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FNoPhotosEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("NPGoblinCharacter"));
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("NPSantaEventDefinition"));
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FNoPhotosEditorModule, NoPhotosEditor)
