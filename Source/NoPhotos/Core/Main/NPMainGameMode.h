#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "NPMainGameMode.generated.h"

class UWorld;
class UNPPhotoEvidenceService;
class UNPPhotoRepository;
class UNPRelicDeliveryService;

UCLASS()
class NOPHOTOS_API ANPMainGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ANPMainGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleSeamlessTravelPlayer(AController*& Controller) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	void RequestRestartRoom(APlayerController* RequestingPlayer);
	FNPPhotoEvidenceResult HandlePhotoCaptureRequest(const FNPPhotoCaptureRequest& Request);
	UNPPhotoRepository* GetPhotoRepository() const { return PhotoRepository; }
	UNPRelicDeliveryService* GetRelicDeliveryService() const { return RelicDeliveryService; }
	void HandlePhotoStored(APlayerController* Photographer, uint16 CaptureSequence, const FGuid& PhotoId);

	virtual void InitGame(
		const FString& MapName,
		const FString& Options,
		FString& ErrorMessage) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Game", meta = (ClampMin = "1"))
	int32 GameDurationSeconds = 180;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Game")
	TSoftObjectPtr<UWorld> RoomLevel;

	UPROPERTY(EditDefaultsOnly, Category = "Photo")
	TSubclassOf<UNPPhotoEvidenceService> PhotoEvidenceServiceClass;

private:
	void PlayPhotoWorldFeedback(const FNPPhotoEvidenceResult& Result);
	void StartMainGame();
	void UpdateMainGameTimer();
	void RefreshPlayerRankings();
	void HandleWaitingRoomRestored(bool bWasSuccessful);

	FTimerHandle MainGameTimer;
	bool bReturningToRoom = false;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoEvidenceService> PhotoEvidenceService;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoRepository> PhotoRepository;

	UPROPERTY(Transient)
	TObjectPtr<UNPRelicDeliveryService> RelicDeliveryService;
};
