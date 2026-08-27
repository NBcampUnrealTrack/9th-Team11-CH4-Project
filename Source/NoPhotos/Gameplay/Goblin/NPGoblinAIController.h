#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NPGoblinAIController.generated.h"

class ANPGoblinCharacter;
class ANPGoblinPatrolRoute;

enum class ENPGoblinMovementState : uint8
{
	Patrol,
	Flee,
	ReturnToRoute,
	RandomRoamFallback
};

/** 서버에서 고블린의 NavMesh 배회와 플레이어 회피 목적지를 선택합니다. */
UCLASS()
class NOPHOTOS_API ANPGoblinAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANPGoblinAIController();

	/** 등장/퇴장 연출 동안 이동 판단을 중지하거나 다시 시작합니다. */
	void SetGameplayEnabled(bool bEnabled);

	/** 서버 전용. 놀람 대기 후 촬영 위치 반대 방향의 이동을 일정 시간 우선합니다. */
	bool StartPhotoFlee(const FVector& CameraLocation, const FVector& FleeDirection);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void EvaluateMovement();
	void EnterFleeState();
	void LeaveFleeState();
	void BeginReturnToRoute();
	void TryFollowPatrolRoute();
	void TryReturnToRoute();
	void TryStartRoaming();
	void TryUpdateFleeDestination(const TArray<FVector>& PlayerLocations);
	void TryUpdatePhotoFleeDestination();
	bool FindPhotoFleeDestination(FVector& OutDestination) const;
	bool GatherPlayerLocations(TArray<FVector>& OutPlayerLocations, float& OutNearestDistanceSquared) const;
	bool FindBestFleeDestination(const TArray<FVector>& PlayerLocations, FVector& OutDestination) const;
	bool RequestMoveToLocation(const FVector& Destination, float AcceptanceRadius);
	ANPGoblinCharacter* GetGoblin() const;
	ANPGoblinPatrolRoute* GetUsablePatrolRoute() const;

	FVector RoamOrigin = FVector::ZeroVector;
	FVector ReturnTargetLocation = FVector::ZeroVector;
	FVector ActivePatrolTargetLocation = FVector::ZeroVector;
	float ActivePatrolTargetDistance = 0.0f;
	ENPGoblinMovementState MovementState = ENPGoblinMovementState::RandomRoamFallback;
	bool bHasActivePatrolTarget = false;
	bool bReturnMoveRequested = false;
	bool bGameplayEnabled = false;
	bool bPhotoFleeActive = false;
	FVector PhotoSourceLocation = FVector::ZeroVector;
	FVector PhotoFleeDirection = FVector::ForwardVector;
	double PhotoReactionEndTime = 0.0;
	double PhotoFleeEndTime = 0.0;
	double NextRoamTime = 0.0;
	double NextFleeRepathTime = 0.0;
	FTimerHandle DecisionTimer;
	FTimerHandle PhotoReactionTimer;
};
