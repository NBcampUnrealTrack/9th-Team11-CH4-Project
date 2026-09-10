#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Gameplay/Photo/NPPhotoEvidenceTypes.h"
#include "NPMainGameMode.generated.h"

class UWorld;
class ANPMainPlayerController;
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
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	void RequestRestartRoom(APlayerController* RequestingPlayer);
	FNPPhotoEvidenceResult HandlePhotoCaptureRequest(const FNPPhotoCaptureRequest& Request);
	UNPPhotoRepository* GetPhotoRepository() const { return PhotoRepository; }
	UNPRelicDeliveryService* GetRelicDeliveryService() const { return RelicDeliveryService; }
	void HandlePhotoStored(APlayerController* Photographer, const FGuid& PhotoId);
	void RegisterPlayerWorldReady(ANPMainPlayerController* PlayerController);

	virtual void InitGame(
		const FString& MapName,
		const FString& Options,
		FString& ErrorMessage) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Game", meta = (ClampMin = "1"))
	int32 GameDurationSeconds = 180;

	/** 잘못된 방 설정이나 응답 없는 클라이언트 때문에 로딩 화면이 무한 유지되는 것을 방지합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Game|Loading", meta = (ClampMin = "5.0", Units = "s"))
	float WorldPreparationTimeoutSeconds = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Game")
	TSoftObjectPtr<UWorld> RoomLevel;

	UPROPERTY(EditDefaultsOnly, Category = "Photo")
	TSubclassOf<UNPPhotoEvidenceService> PhotoEvidenceServiceClass;

private:
	bool ShouldBypassRoomPreparationForEditorTest() const;
	void BeginWorldPreparation();
	void TryStartPreparedMainGame();
	void FailWorldPreparation();
	void HandleWorldPreparationTimeout();

	UFUNCTION()
	void HandleServerRoomGenerationCompleted();

	UFUNCTION()
	void HandleServerRoomGenerationFailed();

	void PlayPhotoWorldFeedback(const FNPPhotoEvidenceResult& Result);
	void StartMainGame();
	void UpdateMainGameTimer();
	void RefreshPlayerRankings();
	void HandleWaitingRoomRestored(bool bWasSuccessful);

	FTimerHandle MainGameTimer;
	FTimerHandle WorldPreparationTimeoutTimer;
	bool bReturningToRoom = false;
	bool bServerWorldReady = false;
	bool bMainGameStarted = false;
	TSet<TWeakObjectPtr<ANPMainPlayerController>> ReadyPlayers;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoEvidenceService> PhotoEvidenceService;

	UPROPERTY(Transient)
	TObjectPtr<UNPPhotoRepository> PhotoRepository;

	UPROPERTY(Transient)
	TObjectPtr<UNPRelicDeliveryService> RelicDeliveryService;
};
