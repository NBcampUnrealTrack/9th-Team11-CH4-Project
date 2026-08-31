#include "UI/Chat/NPChatWidget.h"

#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Core/Chat/NPChatComponent.h"
#include "GameFramework/PlayerController.h"
#include "SubSystem/NPUIManagerSubsystem.h"

void UNPChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerController* PlayerController = GetOwningPlayer();
	ChatComponent = PlayerController
		? PlayerController->FindComponentByClass<UNPChatComponent>()
		: nullptr;
	if (!IsValid(ChatComponent) || !IsValid(ChatInput))
	{
		return;
	}

	ChatComponent->OnChatMessagesChanged.AddUniqueDynamic(this, &ThisClass::RefreshMessages);
	ChatComponent->OnChatInputOpened.AddUniqueDynamic(this, &ThisClass::OpenChatInput);
	ChatComponent->OnChatInputClosed.AddUniqueDynamic(this, &ThisClass::CloseChatInput);
	ChatInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleTextCommitted);

	RefreshMessages(ChatComponent->GetChatMessages());
	if (ChatComponent->IsChatInputOpen())
	{
		OpenChatInput();
	}
	else
	{
		ChatInput->SetVisibility(ESlateVisibility::Collapsed);
		SetChatBackgroundVisible(false);
	}
}

void UNPChatWidget::NativeDestruct()
{
	if (IsValid(ChatComponent) && ChatComponent->IsChatInputOpen())
	{
		ChatComponent->CloseChatInput();
	}

	if (IsValid(ChatComponent))
	{
		ChatComponent->OnChatMessagesChanged.RemoveAll(this);
		ChatComponent->OnChatInputOpened.RemoveAll(this);
		ChatComponent->OnChatInputClosed.RemoveAll(this);
	}

	if (IsValid(ChatInput))
	{
		ChatInput->OnTextCommitted.RemoveAll(this);
	}

	ChatComponent = nullptr;
	Super::NativeDestruct();
}

void UNPChatWidget::RefreshMessages(const TArray<FNPChatMessage>& Messages)
{
	if (!IsValid(MessageList))
	{
		return;
	}

	MessageList->ClearChildren();
	for (const FNPChatMessage& Message : Messages)
	{
		UTextBlock* MessageText = NewObject<UTextBlock>(this);
		MessageText->SetText(Message.ToDisplayText());
		FSlateFontInfo FontInfo = MessageText->GetFont();
		FontInfo.Size = 18;
		MessageText->SetFont(FontInfo);
		MessageList->AddChildToVerticalBox(MessageText);
	}
}

void UNPChatWidget::OpenChatInput()
{
	if (!IsValid(ChatInput))
	{
		return;
	}

	ChatInput->SetVisibility(ESlateVisibility::Visible);
	SetChatBackgroundVisible(true);
	ChatInput->SetKeyboardFocus();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ChatInput->TakeWidget());
		PlayerController->SetInputMode(InputMode);
	}
}

void UNPChatWidget::CloseChatInput()
{
	if (IsValid(ChatInput))
	{
		ChatInput->SetText(FText::GetEmpty());
		ChatInput->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetChatBackgroundVisible(false);

	UGameInstance* GameInstance = GetGameInstance();
	UNPUIManagerSubsystem* UIManager = GameInstance
		? GameInstance->GetSubsystem<UNPUIManagerSubsystem>()
		: nullptr;
	if (IsValid(UIManager))
	{
		UIManager->RefreshTopWidgetInputMode();
	}
}

void UNPChatWidget::HandleTextCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter || !IsValid(ChatComponent))
	{
		return;
	}

	ChatComponent->SubmitChatMessage(Text);
	if (!ChatComponent->IsChatInputOpen() || !IsValid(ChatInput))
	{
		return;
	}

	ChatInput->SetText(FText::GetEmpty());
	ChatInput->SetKeyboardFocus();
}

void UNPChatWidget::SetChatBackgroundVisible(const bool bVisible)
{
	const ESlateVisibility BackgroundVisibility  = bVisible
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;

	if (IsValid(ChatBackgroundBlur))
	{
		ChatBackgroundBlur->SetVisibility(BackgroundVisibility );
	}
	if (IsValid(ChatBackgroundDim))
	{
		ChatBackgroundDim->SetVisibility(BackgroundVisibility );
	}
}
