#include "NPSantaEventDefinition.h"

#include "Gameplay/Relic/NPRelicEditorSelectionUtils.h"

UNPSantaEventDefinition::UNPSantaEventDefinition()
{
	RouteGroup = FGameplayTag::RequestGameplayTag(TEXT("Santa"), false);
}

void UNPSantaEventDefinition::ImportSelectedRelicClasses()
{
	NPRelicEditorSelectionUtils::AppendSelectedRelicClasses(
		this,
		RelicClasses);
}
