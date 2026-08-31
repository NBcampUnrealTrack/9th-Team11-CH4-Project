#include "NPSantaEventDefinition.h"

UNPSantaEventDefinition::UNPSantaEventDefinition()
{
	RouteGroup = FGameplayTag::RequestGameplayTag(TEXT("Santa"), false);
}
