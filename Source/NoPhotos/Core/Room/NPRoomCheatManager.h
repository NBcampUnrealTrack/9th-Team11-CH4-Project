#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "NPRoomCheatManager.generated.h"

class UNPRoomPlayerComponent;

UCLASS()
class NOPHOTOS_API UNPRoomCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void Create();

	UFUNCTION(Exec)
	void List();

	UFUNCTION(Exec)
	void Join(int32 RoomNumber);

	UFUNCTION(Exec)
	void User();

	UFUNCTION(Exec)
	void Start();

	UFUNCTION(Exec)
	void Rehost();

	UFUNCTION(Exec)
	void Out(const FString& Command);

	UFUNCTION(Exec)
	void Add(const FString& Command);

	UFUNCTION(Exec)
	void Remove(const FString& Command);

private:
	UNPRoomPlayerComponent* GetRoomComponent() const;
};
