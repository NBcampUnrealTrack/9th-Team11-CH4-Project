#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Chat/NPChatComponent.h"
#include "NPChatWidget.generated.h"

class UEditableTextBox;
class UBackgroundBlur;
class UBorder;
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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBackgroundBlur> ChatBackgroundBlur;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ChatBackgroundDim;

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

	void SetChatBackgroundVisible(bool bVisible);
};
