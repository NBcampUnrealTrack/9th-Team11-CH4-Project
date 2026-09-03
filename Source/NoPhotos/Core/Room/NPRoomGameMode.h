#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "NPRoomGameMode.generated.h"

class ANPRoomPlayerController;
class UWorld;

UCLASS()
class NOPHOTOS_API ANPRoomGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ANPRoomGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleSeamlessTravelPlayer(AController*& Controller) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	bool ActivateRoom(APlayerController* HostPlayer);
	void TryStartGame(APlayerController* RequestingPlayer);
	void RequestExitRoom(APlayerController* RequestingController);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room")
	TSoftObjectPtr<UWorld> GameLevel;

private:
	void FinishHostMigrationExit();
	bool TryActivateExistingWaitingRoom();
	void RestoreReturningPlayer(AController* Controller);

	bool bRoomActive = false;

	UPROPERTY()
	TObjectPtr<ANPRoomPlayerController> PendingExitingHost;

	FTimerHandle HostMigrationExitTimer;
};
