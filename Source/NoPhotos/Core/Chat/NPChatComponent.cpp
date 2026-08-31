#include "Core/Chat/NPChatComponent.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"

FText FNPChatMessage::ToDisplayText() const
{
	return FText::FromString(FString::Printf(TEXT("%s : %s"), *Nickname, *Content));
}

UNPChatComponent::UNPChatComponent()
{
	SetIsReplicatedByDefault(true);
}

void UNPChatComponent::BindChatInput(UInputComponent* InputComponent)
{
	if (!IsValid(InputComponent))
	{
		return;
	}

	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::OpenChatInput);
}

void UNPChatComponent::OpenChatInput()
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || bChatInputOpen)
	{
		return;
	}

	bChatInputOpen = true;
	OnChatInputOpened.Broadcast();
}

void UNPChatComponent::SubmitChatMessage(const FText& Message)
{
	if (!bChatInputOpen)
	{
		return;
	}

	FString SanitizedMessage = Message.ToString();
	SanitizedMessage.TrimStartAndEndInline();
	SanitizedMessage.ReplaceInline(TEXT("\r"), TEXT(" "));
	SanitizedMessage.ReplaceInline(TEXT("\n"), TEXT(" "));

	if (SanitizedMessage.IsEmpty())
	{
		CloseChatInput();
		return;
	}

	SanitizedMessage.LeftInline(MaxMessageLength);
	ServerSendChatMessage(SanitizedMessage);
}

void UNPChatComponent::CloseChatInput()
{
	if (!bChatInputOpen)
	{
		return;
	}

	bChatInputOpen = false;
	OnChatInputClosed.Broadcast();
}

void UNPChatComponent::ServerSendChatMessage_Implementation(const FString& Message)
{
	FString SanitizedMessage = Message;
	SanitizedMessage.TrimStartAndEndInline();
	SanitizedMessage.ReplaceInline(TEXT("\r"), TEXT(" "));
	SanitizedMessage.ReplaceInline(TEXT("\n"), TEXT(" "));
	SanitizedMessage.LeftInline(MaxMessageLength);
	if (SanitizedMessage.IsEmpty())
	{
		return;
	}

	const APlayerController* SenderController = Cast<APlayerController>(GetOwner());
	const APlayerState* SenderPlayerState = SenderController ? SenderController->PlayerState : nullptr;
	if (!IsValid(SenderPlayerState))
	{
		return;
	}

	FNPChatMessage ChatMessage;
	ChatMessage.Nickname = SenderPlayerState->GetPlayerName();
	ChatMessage.Content = MoveTemp(SanitizedMessage);

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		UNPChatComponent* ChatComponent = PlayerController
			? PlayerController->FindComponentByClass<UNPChatComponent>()
			: nullptr;
		if (IsValid(ChatComponent))
		{
			ChatComponent->ClientReceiveChatMessage(ChatMessage);
		}
	}
}

void UNPChatComponent::ClientReceiveChatMessage_Implementation(const FNPChatMessage& Message)
{
	ChatMessages.Add(Message);
	if (ChatMessages.Num() > MaxVisibleMessages)
	{
		ChatMessages.RemoveAt(0, ChatMessages.Num() - MaxVisibleMessages);
	}

	OnChatMessagesChanged.Broadcast(ChatMessages);
}
