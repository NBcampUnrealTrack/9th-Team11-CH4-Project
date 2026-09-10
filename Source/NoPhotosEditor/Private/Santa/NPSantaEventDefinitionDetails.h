#pragma once

#include "IDetailCustomization.h"

class UNPSantaEventDefinition;

class FNPSantaEventDefinitionDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply ImportSelectedRelicClasses();

	TArray<TWeakObjectPtr<UNPSantaEventDefinition>> CustomizedDefinitions;
};
