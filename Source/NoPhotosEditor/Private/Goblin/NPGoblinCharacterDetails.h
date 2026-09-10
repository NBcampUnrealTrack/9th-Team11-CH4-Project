#pragma once

#include "IDetailCustomization.h"

class ANPGoblinCharacter;

class FNPGoblinCharacterDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply ImportSelectedRelicClasses();

	TArray<TWeakObjectPtr<ANPGoblinCharacter>> CustomizedGoblins;
};
