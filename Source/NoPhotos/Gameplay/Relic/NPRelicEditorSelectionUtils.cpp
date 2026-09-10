#include "Gameplay/Relic/NPRelicEditorSelectionUtils.h"

#include "Gameplay/Relic/NPBaseRelic.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "Engine/Blueprint.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#endif

int32 NPRelicEditorSelectionUtils::AppendSelectedRelicClasses(
	UObject* EditedObject,
	TArray<TSubclassOf<ANPBaseRelic>>& InOutRelicClasses)
{
#if WITH_EDITOR
	if (!IsValid(EditedObject))
	{
		return 0;
	}

	TArray<FAssetData> SelectedAssets;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"))
		.Get()
		.GetSelectedAssets(SelectedAssets);

	EditedObject->Modify();
	int32 AddedCount = 0;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		UObject* Asset = AssetData.GetAsset();
		UClass* CandidateClass = nullptr;
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			CandidateClass = Blueprint->GeneratedClass;
		}
		else
		{
			CandidateClass = Cast<UClass>(Asset);
		}

		if (!IsValid(CandidateClass)
			|| !CandidateClass->IsChildOf(ANPBaseRelic::StaticClass())
			|| CandidateClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		if (!InOutRelicClasses.Contains(CandidateClass))
		{
			InOutRelicClasses.Add(CandidateClass);
			++AddedCount;
		}
	}

	if (AddedCount > 0)
	{
		EditedObject->MarkPackageDirty();
		EditedObject->PostEditChange();
	}
	return AddedCount;
#else
	return 0;
#endif
}
