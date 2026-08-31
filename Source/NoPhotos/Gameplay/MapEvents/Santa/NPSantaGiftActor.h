#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPSantaGiftTypes.h"
#include "NPSantaGiftActor.generated.h"

class ANPBaseRelic;
class UBoxComponent;
class UProjectileMovementComponent;
class USceneComponent;
class UStaticMeshComponent;

/** 서버 낙하/착지/유물 생성 + 각 화면의 개봉 연출. 산타 이벤트와 별도로 수명을 가집니다. */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPSantaGiftActor : public AActor
{
	GENERATED_BODY()

public:
	ANPSantaGiftActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_ReplicatedMovement() override;

	/** deferred spawn 중 서버에서 호출. 후보 목록을 복사하므로 이후 이벤트/DA 수명에 의존하지 않습니다. */
	bool InitializeGift(const TArray<TSubclassOf<ANPBaseRelic>>& InRelicClasses);

	UFUNCTION(BlueprintPure, Category="Santa Gift")
	bool HasLanded() const { return LandingState.bLanded; }

	UFUNCTION(BlueprintPure, Category="Santa Gift")
	float GetOpeningProgress() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 연출 전용 훅. 실제 유물은 서버 C++에서만 생성합니다. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Santa Gift|Presentation")
	void OnGiftLanded();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Santa Gift|Presentation")
	void OnGiftOpened();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Gift")
	TObjectPtr<UBoxComponent> CollisionBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Gift")
	TObjectPtr<UProjectileMovementComponent> FallingMovement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Gift|Visual")
	TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Gift|Visual")
	TObjectPtr<UStaticMeshComponent> ClosedBoxMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Gift|Visual")
	TObjectPtr<UStaticMeshComponent> OpenBoxMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Gift|Visual")
	TObjectPtr<USceneComponent> LidPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Santa Gift|Visual")
	TObjectPtr<UStaticMeshComponent> LidMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Opening", meta=(ClampMin="0.1", ClampMax="10.0", Units="s"))
	float OpeningDuration = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Opening", meta=(ClampMin="0.0", Units="cm"))
	float LidLiftHeight = 60.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Opening")
	FRotator LidOpenRotation = FRotator(-110.0, 0.0, 0.0);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Opening", meta=(ClampMin="0.5", Units="s"))
	float OpenedLifeSpan = 3.0f;

	/** 바닥을 못 찾는 경로의 선물이 영구히 남지 않게 합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Landing", meta=(ClampMin="1.0", Units="s"))
	float MaxFallDuration = 45.0f;
	/** 위쪽을 향한 바닥/지붕만 착지 인정. 벽면/초기 관통은 개봉하지 않습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Landing", meta=(ClampMin="0.1", ClampMax="1.0"))
	float MinimumGroundNormalZ = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Relic", meta=(ClampMin="0.0", Units="cm"))
	float RelicSpawnHeight = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Santa Gift|Relic", meta=(ClampMin="0.0", Units="cm/s"))
	float RelicPopUpSpeed = 150.0f;

private:
	UFUNCTION()
	void HandleFallStopped(const FHitResult& Hit);
	UFUNCTION()
	void OnRep_LandingState();
	void ApplyLandingState();
	void UpdateOpeningVisuals();
	void SpawnRelic();
	void HandleFallTimeout();
	float GetServerTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_LandingState)
	FNPSantaGiftLandingState LandingState;
	UPROPERTY(Transient)
	TArray<TSubclassOf<ANPBaseRelic>> RelicClasses;

	FTransform ClosedBoxInitialTransform;
	FTransform LidInitialTransform;
	FTimerHandle OpeningTimer;
	FTimerHandle FallTimeoutTimer;
	bool bInitialized = false;
	bool bRelicSpawnAttempted = false;
	bool bLandedPresentationStarted = false;
	bool bOpenedPresentationStarted = false;
};
