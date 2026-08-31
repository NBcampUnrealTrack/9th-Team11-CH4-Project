#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Chat/NPChatComponent.h"
#include "NPChatWidget.generated.h"

class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class NOPHOTOS_API UNPChatWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> MessageList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ChatInput;

	UPROPERTY(Transient)
	TObjectPtr<UNPChatComponent> ChatComponent;

	UFUNCTION()
	void RefreshMessages(const TArray<FNPChatMessage>& Messages);

	UFUNCTION()
	void OpenChatInput();

	UFUNCTION()
	void CloseChatInput();

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
