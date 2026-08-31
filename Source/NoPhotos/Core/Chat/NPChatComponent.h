#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPChatComponent.generated.h"

class UInputComponent;

USTRUCT(BlueprintType)
struct FNPChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Content;

	FText ToDisplayText() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNPOnChatMessagesChanged,
	const TArray<FNPChatMessage>&,
	Messages);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnChatInputOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPOnChatInputClosed);

UCLASS(ClassGroup = (Chat), meta = (BlueprintSpawnableComponent))
class NOPHOTOS_API UNPChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPChatComponent();

	void BindChatInput(UInputComponent* InputComponent);

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void OpenChatInput();

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SubmitChatMessage(const FText& Message);

	UFUNCTION(BlueprintCallable, Category = "Chat")
	void CloseChatInput();

	UFUNCTION(BlueprintPure, Category = "Chat")
	bool IsChatInputOpen() const { return bChatInputOpen; }

	UFUNCTION(BlueprintPure, Category = "Chat")
	TArray<FNPChatMessage> GetChatMessages() const { return ChatMessages; }

	UPROPERTY(BlueprintAssignable, Category = "Chat")
	FNPOnChatMessagesChanged OnChatMessagesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Chat")
	FNPOnChatInputOpened OnChatInputOpened;

	UPROPERTY(BlueprintAssignable, Category = "Chat")
	FNPOnChatInputClosed OnChatInputClosed;

private:
	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& Message);

	UFUNCTION(Client, Reliable)
	void ClientReceiveChatMessage(const FNPChatMessage& Message);

	UPROPERTY(Transient)
	TArray<FNPChatMessage> ChatMessages;

	bool bChatInputOpen = false;

	static constexpr int32 MaxVisibleMessages = 10;
	static constexpr int32 MaxMessageLength = 200;
};
