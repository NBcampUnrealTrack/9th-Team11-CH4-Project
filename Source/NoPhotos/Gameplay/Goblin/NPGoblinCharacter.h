#pragma once

#include "CoreMinimal.h"
#include "Data/Interface/NPPhotoReactiveTarget.h"
#include "GameFramework/Character.h"
#include "NPGoblinCharacter.generated.h"

class ANPGoblinPatrolRoute;
class ANPBaseRelic;
class APlayerState;
class UAnimMontage;
class ANPGoblinPresentationDoor;
class UDataTable;

UENUM(BlueprintType)
enum class ENPGoblinLifecycleState : uint8
{
	Spawning UMETA(DisplayName = "Spawning"),
	Active UMETA(DisplayName = "Active"),
	Despawning UMETA(DisplayName = "Despawning"),
	MAX UMETA(Hidden)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FNPOnGoblinPhotographed,
	APlayerState*, Photographer,
	float, Visibility,
	int32, CaptureSequence);

/**
 * NavMesh를 이용해 배회하고 가까운 플레이어에게서 도망가는 고블린입니다.
 * 실제 이동 의사결정은 ANPGoblinAIController가 서버에서 수행합니다.
 */
UCLASS(Blueprintable)
class NOPHOTOS_API ANPGoblinCharacter : public ACharacter, public INPPhotoReactiveTarget
{
	GENERATED_BODY()

public:
	ANPGoblinCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Deferred Spawn이 끝나기 전에 호출하여 등장 연출 동안 AI가 움직이지 않게 합니다. */
	void PrepareForSpawnPresentation();

	/** 등장 Montage의 Anim Notify에서 호출하면 즉시 순찰을 시작합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Goblin|Presentation")
	void FinishSpawnPresentation();

	/** 퇴장 연출을 시작합니다. 완료 전까지 AI 이동과 사진 판정이 중지됩니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Goblin|Presentation")
	void BeginDespawnPresentation();

	/** 퇴장 Montage의 Anim Notify에서 호출하면 고블린을 제거합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Goblin|Presentation")
	void FinishDespawnPresentation();

	UFUNCTION(BlueprintPure, Category = "Goblin|Presentation")
	ENPGoblinLifecycleState GetLifecycleState() const { return LifecycleState; }

	UFUNCTION(BlueprintPure, Category = "Goblin|Presentation")
	bool IsGameplayActive() const { return LifecycleState == ENPGoblinLifecycleState::Active; }

	/** 이벤트가 생성 직전에 주입하는 서버 전용 순찰 경로입니다. */
	void SetPatrolRoute(ANPGoblinPatrolRoute* InPatrolRoute)
	{
		PatrolRoute = InPatrolRoute;
		bRequiresPatrolRoute = InPatrolRoute != nullptr;
	}
	ANPGoblinPatrolRoute* GetPatrolRoute() const { return PatrolRoute; }
	bool RequiresPatrolRoute() const { return bRequiresPatrolRoute; }

	float GetAIDecisionInterval() const { return FMath::Max(0.05f, AIDecisionInterval); }
	float GetPatrolAcceptanceRadius() const { return FMath::Max(1.0f, PatrolAcceptanceRadius); }
	float GetPatrolTargetSpacing() const { return FMath::Max(GetPatrolAcceptanceRadius() * 2.0f, PatrolTargetSpacing); }
	float GetPatrolTargetSwitchDistance() const
	{
		return FMath::Clamp(
			PatrolTargetSwitchDistance,
			GetPatrolAcceptanceRadius() + 1.0f,
			GetPatrolTargetSpacing() - 1.0f);
	}
	float GetRouteReturnLookAheadDistance() const { return FMath::Max(0.0f, RouteReturnLookAheadDistance); }
	float GetFleeRouteDirectionWeight() const { return FMath::Max(0.0f, FleeRouteDirectionWeight); }
	float GetFleeRouteDistancePenaltyWeight() const { return FMath::Max(0.0f, FleeRouteDistancePenaltyWeight); }
	float GetRoamRadius() const { return FMath::Max(0.0f, RoamRadius); }
	float GetRoamAcceptanceRadius() const { return FMath::Max(1.0f, RoamAcceptanceRadius); }
	float GetMinimumRoamWaitTime() const { return FMath::Max(0.0f, MinimumRoamWaitTime); }
	float GetMaximumRoamWaitTime() const { return FMath::Max(0.0f, MaximumRoamWaitTime); }
	float GetPlayerDetectionRadius() const { return FMath::Max(0.0f, PlayerDetectionRadius); }
	float GetFleeReleaseDistance() const { return FMath::Max(GetPlayerDetectionRadius(), FleeReleaseDistance); }
	float GetFleeTravelDistance() const { return FMath::Max(1.0f, FleeTravelDistance); }
	float GetFleeAcceptanceRadius() const { return FMath::Max(1.0f, FleeAcceptanceRadius); }
	float GetFleeRepathInterval() const { return FMath::Max(0.05f, FleeRepathInterval); }
	int32 GetFleeSamplingAttempts() const { return FMath::Max(1, FleeSamplingAttempts); }
	float GetRoamMoveSpeed() const { return FMath::Max(0.0f, RoamMoveSpeed); }
	float GetFleeMoveSpeed() const { return FMath::Max(0.0f, FleeMoveSpeed); }
	float GetPhotoReactionDuration() const { return FMath::Max(0.0f, PhotoReactionDuration); }
	float GetPhotoFleeDuration() const { return FMath::Max(0.0f, PhotoFleeDuration); }
	float GetPhotoFleeMoveSpeed() const { return FMath::Max(1.0f, PhotoFleeMoveSpeed); }

	virtual bool CanBePhotographed_Implementation(APlayerState* Photographer) const override;
	virtual void OnPhotographed_Implementation(
		APlayerState* Photographer,
		float Visibility,
		int32 CaptureSequence) override;
	virtual void OnPhotographedFromCamera_Implementation(
		APlayerState* Photographer, float Visibility, int32 CaptureSequence,
		FVector CameraLocation, FVector CameraForward) override;

	/** 서버에서 플레이어가 이 고블린을 유효하게 촬영할 때마다 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "Goblin|Photo")
	FNPOnGoblinPhotographed OnGoblinPhotographed;

	UFUNCTION(BlueprintPure, Category = "Goblin|Photo|Reward")
	ANPBaseRelic* GetSpawnedPhotoRelic() const { return SpawnedPhotoRelic; }

	UFUNCTION(BlueprintPure, Category = "Goblin|Photo|Health")
	int32 GetCurrentPhotoHP() const { return CurrentPhotoHP; }

	UFUNCTION(BlueprintPure, Category = "Goblin|Photo|Health")
	int32 GetMaxPhotoHP() const { return FMath::Max(1, MaxPhotoHP); }

protected:
	/** 켜면 기존 Spawn/Despawn Presentation Started BP 이벤트 대신 코드의 문 출입 연출을 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Presentation|Door")
	bool bUseDoorPresentation = true;

	/** 기본 C++ 문을 그대로 사용하거나 해당 클래스를 상속한 BP에서 문짝/재질을 변경합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Presentation|Door")
	TSubclassOf<ANPGoblinPresentationDoor> PresentationDoorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Presentation|Door", meta = (ClampMin = "100", Units = "cm"))
	float DoorTravelDistance = 250.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Presentation|Door", meta = (ClampMin = "1", Units = "cm/s"))
	float DoorWalkSpeed = 200.0f;
	/** NavMesh 이동 실패나 외부 중단 시에도 연출 상태에 갇히지 않는 제한 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Presentation|Door", meta = (ClampMin = "1", Units = "s"))
	float DoorPresentationTimeout = 8.0f;

	/** 생존한 고블린의 촬영 반응. 서버와 관련 클라이언트에서 호출되며 방향은 월드 XY 방향입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Goblin|Photo|Escape", meta = (DisplayName = "On Photo Escape Started"))
	void BP_OnPhotoEscapeStarted(FVector FleeDirection, float ReactionDuration);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Escape")
	bool bFleeWhenPhotographed = true;

	/** 놀란 뒤 도주를 시작하기까지의 정지 시간. 0이면 즉시 도주합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Escape", meta = (ClampMin = "0.0", Units = "s"))
	float PhotoReactionDuration = 0.25f;

	/** 놀람 시간이 끝난 뒤 촬영 반대 방향 도주를 우선하는 시간. 이후 일반 회피/순찰로 돌아갑니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Escape", meta = (ClampMin = "0.0", Units = "s"))
	float PhotoFleeDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Escape", meta = (ClampMin = "1.0", Units = "cm/s"))
	float PhotoFleeMoveSpeed = 800.0f;

	/** 선택 사항: 제자리 놀람 몽타주. 놀람 시간 종료/퇴장 시 중단됩니다. AnimBP에 해당 Slot이 필요합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Escape")
	TObjectPtr<UAnimMontage> PhotoReactionMontage;

	/** 사진 대미지와 촬영 보상 생성 후 서버에서 BP 고블린에게 전달되는 콜백입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Goblin|Photo", meta = (DisplayName = "On Photographed"))
	void BP_OnPhotographed(APlayerState* Photographer, float Visibility, int32 CaptureSequence);

	/** 서버의 현재 사진 HP가 변경되고 복제될 때 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Goblin|Photo|Health", meta = (DisplayName = "On Photo HP Changed"))
	void BP_OnPhotoHPChanged(int32 NewPhotoHP, int32 MaximumPhotoHP);

	/** 사진 HP가 0이 된 서버에서 마지막 촬영 보상 생성 후 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Goblin|Photo|Health", meta = (DisplayName = "On Photo HP Depleted"))
	void BP_OnPhotoHPDepleted(APlayerState* Photographer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Health", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxPhotoHP = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Health", meta = (ClampMin = "1", UIMin = "1"))
	int32 PhotoDamagePerCapture = 1;

	/** 지정하면 유효한 RelicClass를 가진 행 중 하나를 무작위로 골라 보상으로 생성합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Reward")
	TObjectPtr<UDataTable> RelicDropTable;

	/** RelicDropTable을 사용하지 못할 때 서버에서 생성할 예비 유물 BP입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Reward")
	TSubclassOf<ANPBaseRelic> PhotographedRelicClass;

	/** 고블린 중심을 기준으로 유물을 생성할 상대 위치입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Reward", meta = (Units = "cm"))
	FVector PhotographedRelicSpawnOffset = FVector(0.0f, 0.0f, 50.0f);

	/** 생성된 유물이 위로 튀어 오르는 속도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Reward", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PhotographedRelicUpwardLaunchSpeed = 500.0f;

	/** 여러 유물이 겹치지 않도록 임의의 수평 방향으로 퍼지는 속도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Photo|Reward", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PhotographedRelicHorizontalLaunchSpeed = 150.0f;

	/** BP에서 포탈 생성과 등장 Montage 재생을 시작합니다. 모든 인스턴스에서 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Goblin|Presentation", meta = (DisplayName = "On Spawn Presentation Started"))
	void BP_OnSpawnPresentationStarted();

	/** 등장 연출이 끝나고 AI와 사진 판정이 활성화됐을 때 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Goblin|Presentation", meta = (DisplayName = "On Gameplay Activated"))
	void BP_OnGameplayActivated();

	/** BP에서 퇴장 포탈 생성과 퇴장 Montage 재생을 시작합니다. 모든 인스턴스에서 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Goblin|Presentation", meta = (DisplayName = "On Despawn Presentation Started"))
	void BP_OnDespawnPresentationStarted();

	/** Anim Notify가 누락돼도 등장 상태에 갇히지 않게 하는 최대 대기 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Presentation", meta = (ClampMin = "0.0", Units = "s"))
	float SpawnPresentationTimeout = 5.0f;

	/** Anim Notify가 누락돼도 제거되도록 하는 퇴장 최대 대기 시간입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|Presentation", meta = (ClampMin = "0.0", Units = "s"))
	float DespawnPresentationTimeout = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI", meta = (ClampMin = "0.05", Units = "s"))
	float AIDecisionInterval = 0.25f;

	/** 순찰 중 한 번의 MoveTo가 Spline을 따라 전진할 거리입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Patrol", meta = (ClampMin = "100.0", Units = "cm"))
	float PatrolTargetSpacing = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Patrol", meta = (ClampMin = "1.0", Units = "cm"))
	float PatrolAcceptanceRadius = 100.0f;

	/** 현재 목표에 완전히 멈추기 전에 다음 Spline 목표로 전환할 거리입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Patrol", meta = (ClampMin = "1.0", Units = "cm"))
	float PatrolTargetSwitchDistance = 250.0f;

	/** 도주 종료 후 가장 가까운 Spline 위치보다 앞쪽으로 복귀할 거리입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Patrol", meta = (ClampMin = "0.0", Units = "cm"))
	float RouteReturnLookAheadDistance = 400.0f;

	/** 최초 생성 위치를 중심으로 랜덤 배회 목적지를 찾을 반경입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Roam", meta = (ClampMin = "0.0", Units = "cm"))
	float RoamRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Roam", meta = (ClampMin = "1.0", Units = "cm"))
	float RoamAcceptanceRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Roam", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumRoamWaitTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Roam", meta = (ClampMin = "0.0", Units = "s"))
	float MaximumRoamWaitTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Roam", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RoamMoveSpeed = 300.0f;

	/** 플레이어가 이 거리 안으로 들어오면 도망 상태로 전환합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "0.0", Units = "cm"))
	float PlayerDetectionRadius = 1000.0f;

	/** 이 거리까지 벗어난 뒤 배회 상태로 돌아갑니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "0.0", Units = "cm"))
	float FleeReleaseDistance = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "1.0", Units = "cm"))
	float FleeTravelDistance = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "1.0", Units = "cm"))
	float FleeAcceptanceRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "0.05", Units = "s"))
	float FleeRepathInterval = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "1", UIMin = "1"))
	int32 FleeSamplingAttempts = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "0.0", Units = "cm/s"))
	float FleeMoveSpeed = 600.0f;

	/** 도주 후보가 Spline 진행 방향과 일치할 때 주는 가산점 비율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "0.0"))
	float FleeRouteDirectionWeight = 0.5f;

	/** 도주 후보가 Spline에서 멀어질수록 적용할 감점 비율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goblin|AI|Flee", meta = (ClampMin = "0.0"))
	float FleeRouteDistancePenaltyWeight = 0.25f;

private:
	enum class EDoorPresentationPhase : uint8 { None, Opening, Walking, Closing };
	bool StartDoorPresentation();
	bool FindDoorTravelLocation(FVector& OutFloorLocation, FTransform& OutDoorTransform) const;
	void UpdateDoorPresentation();
	void CompleteDoorPresentation();
	void CancelDoorPresentation();
	EDoorPresentationPhase DoorPresentationPhase = EDoorPresentationPhase::None;
	FVector DoorMoveTarget = FVector::ZeroVector;
	double DoorPhaseEndTime = 0.0;
	double DoorDeadline = 0.0;
	FTimerHandle DoorPresentationTimer;
	UPROPERTY(Transient)
	TObjectPtr<ANPGoblinPresentationDoor> PresentationDoor;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPhotoReaction(FVector FleeDirection, float ReactionDuration);
	void StopPhotoReaction();
	FTimerHandle PhotoReactionTimer;

	void SetLifecycleState(ENPGoblinLifecycleState NewState);
	void NotifyLifecycleStateChanged();
	void TrySpawnPhotographedRelic();

	UFUNCTION()
	void OnRep_LifecycleState();

	UFUNCTION()
	void OnRep_CurrentPhotoHP();

	UPROPERTY(ReplicatedUsing = OnRep_LifecycleState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Goblin|Presentation", meta = (AllowPrivateAccess = "true"))
	ENPGoblinLifecycleState LifecycleState = ENPGoblinLifecycleState::Active;

	ENPGoblinLifecycleState LastNotifiedLifecycleState = ENPGoblinLifecycleState::MAX;
	FTimerHandle PresentationTimeoutTimer;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhotoHP, VisibleInstanceOnly, BlueprintReadOnly, Category = "Goblin|Photo|Health", meta = (AllowPrivateAccess = "true"))
	int32 CurrentPhotoHP = 3;

	UPROPERTY(Transient)
	TObjectPtr<ANPGoblinPatrolRoute> PatrolRoute;
	bool bRequiresPatrolRoute = false;

	/** 가장 최근 촬영에서 생성된 유물입니다. */
	UPROPERTY(Transient)
	TObjectPtr<ANPBaseRelic> SpawnedPhotoRelic;

};
