#include "NPGoblinAIController.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "NPGoblinCharacter.h"
#include "NPGoblinPatrolRoute.h"
#include "TimerManager.h"

ANPGoblinAIController::ANPGoblinAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	bAttachToPawn = true;
}

void ANPGoblinAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ANPGoblinCharacter* Goblin = GetGoblin();
	if (!HasAuthority() || !Goblin)
	{
		return;
	}

	RoamOrigin = Goblin->GetActorLocation();
	MovementState = GetUsablePatrolRoute()
		? ENPGoblinMovementState::Patrol
		: ENPGoblinMovementState::RandomRoamFallback;
	bHasActivePatrolTarget = false;
	bReturnMoveRequested = false;
	NextRoamTime = 0.0;
	NextFleeRepathTime = 0.0;
	bGameplayEnabled = Goblin->IsGameplayActive();
	bPhotoFleeActive = false;
	GetWorldTimerManager().SetTimer(
		DecisionTimer,
		this,
		&ThisClass::EvaluateMovement,
		Goblin->GetAIDecisionInterval(),
		true,
		0.01f);

	if (!bGameplayEnabled)
	{
		StopMovement();
	}
}

void ANPGoblinAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(DecisionTimer);
	GetWorldTimerManager().ClearTimer(PhotoReactionTimer);
	bPhotoFleeActive = false;
	StopMovement();
	Super::OnUnPossess();
}

void ANPGoblinAIController::SetGameplayEnabled(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bGameplayEnabled = bEnabled;
	bPhotoFleeActive = false;
	GetWorldTimerManager().ClearTimer(PhotoReactionTimer);
	bHasActivePatrolTarget = false;
	bReturnMoveRequested = false;
	NextRoamTime = 0.0;
	NextFleeRepathTime = 0.0;
	StopMovement();

	ANPGoblinCharacter* Goblin = GetGoblin();
	if (!bGameplayEnabled || !Goblin)
	{
		return;
	}

	RoamOrigin = Goblin->GetActorLocation();
	MovementState = GetUsablePatrolRoute()
		? ENPGoblinMovementState::Patrol
		: ENPGoblinMovementState::RandomRoamFallback;
	if (UCharacterMovementComponent* Movement = Goblin->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Goblin->GetRoamMoveSpeed();
	}
}

bool ANPGoblinAIController::StartPhotoFlee(
	const FVector& CameraLocation, const FVector& FleeDirection)
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	if (!HasAuthority() || !bGameplayEnabled || !Goblin || !World
		|| !Goblin->IsGameplayActive() || Goblin->GetCurrentPhotoHP() <= 0
		|| Goblin->GetPhotoFleeDuration() <= 0.0f)
	{
		return false;
	}

	PhotoSourceLocation = CameraLocation;
	PhotoFleeDirection = FleeDirection.GetSafeNormal2D();
	if (PhotoFleeDirection.IsNearlyZero())
	{
		return false;
	}

	// A new photo replaces the previous source/direction and restarts the escape window.
	EnterFleeState();
	bPhotoFleeActive = true;
	PhotoReactionEndTime = World->GetTimeSeconds() + Goblin->GetPhotoReactionDuration();
	PhotoFleeEndTime = PhotoReactionEndTime + Goblin->GetPhotoFleeDuration();
	if (UCharacterMovementComponent* Movement = Goblin->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->MaxWalkSpeed = Goblin->GetPhotoFleeMoveSpeed();
	}

	GetWorldTimerManager().ClearTimer(PhotoReactionTimer);
	if (Goblin->GetPhotoReactionDuration() > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PhotoReactionTimer, this,
			&ThisClass::EvaluateMovement, Goblin->GetPhotoReactionDuration(), false);
	}
	else
	{
		EvaluateMovement();
	}
	return true;
}

void ANPGoblinAIController::EvaluateMovement()
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	if (!HasAuthority() || !bGameplayEnabled || !Goblin || !World)
	{
		return;
	}

	TArray<FVector> PlayerLocations;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	const bool bHasPlayer = GatherPlayerLocations(PlayerLocations, NearestDistanceSquared);
	const float EnterDistanceSquared = FMath::Square(Goblin->GetPlayerDetectionRadius());
	const float LeaveDistanceSquared = FMath::Square(Goblin->GetFleeReleaseDistance());

	if (bPhotoFleeActive)
	{
		const double CurrentTime = World->GetTimeSeconds();
		if (CurrentTime < PhotoFleeEndTime)
		{
			if (CurrentTime >= PhotoReactionEndTime)
			{
				TryUpdatePhotoFleeDestination();
			}
			// Neither proximity checks nor patrol may override the photo reaction/escape.
			return;
		}

		bPhotoFleeActive = false;
		UE_LOG(LogNPPhoto, Display, TEXT("[GoblinPhoto] ESCAPE_END Goblin=%s"), *GetNameSafe(Goblin));
		if (bHasPlayer && NearestDistanceSquared < LeaveDistanceSquared)
		{
			// Still threatened: continue ordinary avoidance, at the ordinary flee speed.
			EnterFleeState();
		}
		else
		{
			LeaveFleeState();
		}
	}

	if (MovementState != ENPGoblinMovementState::Flee
		&& bHasPlayer
		&& NearestDistanceSquared <= EnterDistanceSquared)
	{
		EnterFleeState();
	}
	else if (MovementState == ENPGoblinMovementState::Flee
		&& (!bHasPlayer || NearestDistanceSquared >= LeaveDistanceSquared))
	{
		LeaveFleeState();
	}

	switch (MovementState)
	{
	case ENPGoblinMovementState::Flee:
		TryUpdateFleeDestination(PlayerLocations);
		break;
	case ENPGoblinMovementState::ReturnToRoute:
		TryReturnToRoute();
		break;
	case ENPGoblinMovementState::Patrol:
		TryFollowPatrolRoute();
		break;
	case ENPGoblinMovementState::RandomRoamFallback:
	default:
		TryStartRoaming();
		break;
	}
}

void ANPGoblinAIController::EnterFleeState()
{
	MovementState = ENPGoblinMovementState::Flee;
	bHasActivePatrolTarget = false;
	bReturnMoveRequested = false;
	NextFleeRepathTime = 0.0;
	StopMovement();

	if (ANPGoblinCharacter* Goblin = GetGoblin())
	{
		if (UCharacterMovementComponent* Movement = Goblin->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = Goblin->GetFleeMoveSpeed();
		}
	}
}

void ANPGoblinAIController::LeaveFleeState()
{
	StopMovement();

	ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	if (!Goblin || !World)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Goblin->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Goblin->GetRoamMoveSpeed();
	}

	if (GetUsablePatrolRoute())
	{
		BeginReturnToRoute();
		return;
	}

	MovementState = ENPGoblinMovementState::RandomRoamFallback;

	const float MinimumWait = FMath::Min(Goblin->GetMinimumRoamWaitTime(), Goblin->GetMaximumRoamWaitTime());
	const float MaximumWait = FMath::Max(Goblin->GetMinimumRoamWaitTime(), Goblin->GetMaximumRoamWaitTime());
	NextRoamTime = World->GetTimeSeconds() + FMath::FRandRange(MinimumWait, MaximumWait);
}

void ANPGoblinAIController::BeginReturnToRoute()
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	ANPGoblinPatrolRoute* Route = GetUsablePatrolRoute();
	if (!Goblin || !Route)
	{
		MovementState = ENPGoblinMovementState::RandomRoamFallback;
		return;
	}

	const float ClosestDistance = Route->FindDistanceClosestToWorldLocation(
		Goblin->GetActorLocation());
	const float ReturnDistance = Route->AdvanceDistance(
		ClosestDistance,
		Goblin->GetRouteReturnLookAheadDistance());
	ReturnTargetLocation = Route->GetWorldLocationAtDistance(ReturnDistance);
	MovementState = ENPGoblinMovementState::ReturnToRoute;
	bReturnMoveRequested = false;
}

void ANPGoblinAIController::TryFollowPatrolRoute()
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	ANPGoblinPatrolRoute* Route = GetUsablePatrolRoute();
	UWorld* World = GetWorld();
	if (!Goblin || !World)
	{
		return;
	}

	if (!Route)
	{
		MovementState = ENPGoblinMovementState::RandomRoamFallback;
		bHasActivePatrolTarget = false;
		NextRoamTime = 0.0;
		return;
	}

	if (World->GetTimeSeconds() < NextRoamTime)
	{
		return;
	}

	if (bHasActivePatrolTarget)
	{
		const float SwitchDistanceSquared = FMath::Square(
			Goblin->GetPatrolTargetSwitchDistance());
		const bool bShouldAdvanceTarget = FVector::DistSquared2D(
			Goblin->GetActorLocation(),
			ActivePatrolTargetLocation) <= SwitchDistanceSquared;
		if (!bShouldAdvanceTarget
			&& GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			return;
		}

		if (bShouldAdvanceTarget)
		{
			const float NextTargetDistance = Route->AdvanceDistance(
				ActivePatrolTargetDistance,
				Goblin->GetPatrolTargetSpacing());
			const FVector NextTargetLocation = Route->GetWorldLocationAtDistance(
				NextTargetDistance);
			if (RequestMoveToLocation(
					NextTargetLocation,
					Goblin->GetPatrolAcceptanceRadius()))
			{
				ActivePatrolTargetDistance = NextTargetDistance;
				ActivePatrolTargetLocation = NextTargetLocation;
				return;
			}
		}

		bHasActivePatrolTarget = false;
	}

	const float ClosestDistance = Route->FindDistanceClosestToWorldLocation(
		Goblin->GetActorLocation());
	const float TargetDistance = Route->AdvanceDistance(
		ClosestDistance,
		Goblin->GetPatrolTargetSpacing());
	const FVector TargetLocation = Route->GetWorldLocationAtDistance(TargetDistance);
	if (RequestMoveToLocation(TargetLocation, Goblin->GetPatrolAcceptanceRadius()))
	{
		ActivePatrolTargetDistance = TargetDistance;
		ActivePatrolTargetLocation = TargetLocation;
		bHasActivePatrolTarget = true;
		return;
	}

	NextRoamTime = World->GetTimeSeconds() + 0.5;
}

void ANPGoblinAIController::TryReturnToRoute()
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	if (!Goblin)
	{
		return;
	}

	if (!GetUsablePatrolRoute())
	{
		MovementState = ENPGoblinMovementState::RandomRoamFallback;
		bHasActivePatrolTarget = false;
		bReturnMoveRequested = false;
		NextRoamTime = 0.0;
		return;
	}

	if (!bReturnMoveRequested)
	{
		bReturnMoveRequested = RequestMoveToLocation(
			ReturnTargetLocation,
			Goblin->GetPatrolAcceptanceRadius());
		return;
	}

	if (GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		MovementState = ENPGoblinMovementState::Patrol;
		bHasActivePatrolTarget = false;
		bReturnMoveRequested = false;
		NextRoamTime = 0.0;
	}
}

void ANPGoblinAIController::TryStartRoaming()
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!Goblin || !World || !NavigationSystem
		|| World->GetTimeSeconds() < NextRoamTime
		|| GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	FNavLocation Destination;
	if (NavigationSystem->GetRandomReachablePointInRadius(
			RoamOrigin,
			Goblin->GetRoamRadius(),
			Destination))
	{
		RequestMoveToLocation(Destination.Location, Goblin->GetRoamAcceptanceRadius());
	}

	const float MinimumWait = FMath::Min(Goblin->GetMinimumRoamWaitTime(), Goblin->GetMaximumRoamWaitTime());
	const float MaximumWait = FMath::Max(Goblin->GetMinimumRoamWaitTime(), Goblin->GetMaximumRoamWaitTime());
	NextRoamTime = World->GetTimeSeconds() + FMath::FRandRange(MinimumWait, MaximumWait);
}

void ANPGoblinAIController::TryUpdateFleeDestination(const TArray<FVector>& PlayerLocations)
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	if (!Goblin || !World || PlayerLocations.IsEmpty())
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (CurrentTime < NextFleeRepathTime && GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	FVector Destination;
	if (FindBestFleeDestination(PlayerLocations, Destination))
	{
		RequestMoveToLocation(Destination, Goblin->GetFleeAcceptanceRadius());
	}
	NextFleeRepathTime = CurrentTime + Goblin->GetFleeRepathInterval();
}

void ANPGoblinAIController::TryUpdatePhotoFleeDestination()
{
	ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	if (!Goblin || !World || World->GetTimeSeconds() < NextFleeRepathTime)
	{
		return;
	}

	FVector Destination;
	if (FindPhotoFleeDestination(Destination))
	{
		RequestMoveToLocation(Destination, Goblin->GetFleeAcceptanceRadius());
	}
	else if (GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		UE_LOG(LogNPPhoto, Verbose,
			TEXT("[GoblinPhoto] No reachable escape destination. Goblin=%s"), *GetNameSafe(Goblin));
	}
	NextFleeRepathTime = World->GetTimeSeconds() + Goblin->GetFleeRepathInterval();
}

bool ANPGoblinAIController::FindPhotoFleeDestination(FVector& OutDestination) const
{
	const ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!Goblin || !NavigationSystem)
	{
		return false;
	}

	const FVector Origin = Goblin->GetActorLocation();
	const float TravelDistance = Goblin->GetFleeTravelDistance();
	const float MinimumTravelDistance = Goblin->GetFleeAcceptanceRadius()
		+ Goblin->GetSimpleCollisionRadius() + 10.0f;
	const double SourceDistanceSquared = FVector::DistSquared2D(Origin, PhotoSourceLocation);
	const FVector ProjectionExtent(100.0f, 100.0f, 500.0f);
	const float Angles[] = { 0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 90.0f, -90.0f };
	const float DistanceScales[] = { 1.0f, 0.6f, 0.3f };
	double BestScore = TNumericLimits<double>::Lowest();
	bool bFoundDestination = false;

	for (const float Angle : Angles)
	{
		const FVector Direction = PhotoFleeDirection.RotateAngleAxis(Angle, FVector::UpVector);
		for (const float DistanceScale : DistanceScales)
		{
			FNavLocation Candidate;
			if (!NavigationSystem->ProjectPointToNavigation(
				Origin + Direction * TravelDistance * DistanceScale, Candidate, ProjectionExtent))
			{
				continue;
			}

			const FVector Delta = Candidate.Location - Origin;
			const double Distance = Delta.Size2D();
			const double Alignment = FVector::DotProduct(Delta.GetSafeNormal2D(), PhotoFleeDirection);
			if (Distance <= MinimumTravelDistance || Alignment < -0.01
				|| FVector::DistSquared2D(Candidate.Location, PhotoSourceLocation) < SourceDistanceSquared)
			{
				continue;
			}

			// Prefer the photographed-away direction. Sideways detours are allowed, not targets behind us.
			const double Score = Alignment * TravelDistance * 2.0 + Distance;
			if (Score <= BestScore)
			{
				continue;
			}
			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
				World, Origin, Candidate.Location, GetPawn());
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				continue;
			}

			BestScore = Score;
			OutDestination = Candidate.Location;
			bFoundDestination = true;
		}
	}
	return bFoundDestination;
}

bool ANPGoblinAIController::GatherPlayerLocations(
	TArray<FVector>& OutPlayerLocations,
	float& OutNearestDistanceSquared) const
{
	OutPlayerLocations.Reset();
	OutNearestDistanceSquared = TNumericLimits<float>::Max();

	const APawn* GoblinPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!GoblinPawn || !World)
	{
		return false;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!IsValid(PlayerPawn))
		{
			continue;
		}

		const FVector PlayerLocation = PlayerPawn->GetActorLocation();
		OutPlayerLocations.Add(PlayerLocation);
		OutNearestDistanceSquared = FMath::Min(
			OutNearestDistanceSquared,
			FVector::DistSquared2D(GoblinPawn->GetActorLocation(), PlayerLocation));
	}
	return !OutPlayerLocations.IsEmpty();
}

bool ANPGoblinAIController::FindBestFleeDestination(
	const TArray<FVector>& PlayerLocations,
	FVector& OutDestination) const
{
	OutDestination = FVector::ZeroVector;
	const ANPGoblinCharacter* Goblin = GetGoblin();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!Goblin || !NavigationSystem || PlayerLocations.IsEmpty())
	{
		return false;
	}

	const FVector GoblinLocation = Goblin->GetActorLocation();
	FVector NearestPlayerLocation = PlayerLocations[0];
	float NearestDistanceSquared = FVector::DistSquared2D(GoblinLocation, NearestPlayerLocation);
	for (int32 Index = 1; Index < PlayerLocations.Num(); ++Index)
	{
		const float DistanceSquared = FVector::DistSquared2D(GoblinLocation, PlayerLocations[Index]);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestPlayerLocation = PlayerLocations[Index];
		}
	}

	const FVector AwayDirection = (GoblinLocation - NearestPlayerLocation).GetSafeNormal2D();
	const ANPGoblinPatrolRoute* Route = GetUsablePatrolRoute();
	const float ClosestRouteDistance = Route
		? Route->FindDistanceClosestToWorldLocation(GoblinLocation)
		: 0.0f;
	const FVector RouteDirection = Route
		? Route->GetWorldDirectionAtDistance(ClosestRouteDistance).GetSafeNormal2D()
		: FVector::ZeroVector;
	float BestScore = TNumericLimits<float>::Lowest();
	bool bFoundDestination = false;
	for (int32 Attempt = 0; Attempt < Goblin->GetFleeSamplingAttempts(); ++Attempt)
	{
		FNavLocation Candidate;
		if (!NavigationSystem->GetRandomReachablePointInRadius(
				GoblinLocation,
				Goblin->GetFleeTravelDistance(),
				Candidate))
		{
			continue;
		}

		const FVector CandidateDirection = (Candidate.Location - GoblinLocation).GetSafeNormal2D();
		if (CandidateDirection.IsNearlyZero())
		{
			continue;
		}

		float MinimumDistanceSquared = TNumericLimits<float>::Max();
		for (const FVector& PlayerLocation : PlayerLocations)
		{
			MinimumDistanceSquared = FMath::Min(
				MinimumDistanceSquared,
				FVector::DistSquared2D(Candidate.Location, PlayerLocation));
		}

		const float AwayAlignment = AwayDirection.IsNearlyZero()
			? 0.0f
			: FVector::DotProduct(CandidateDirection, AwayDirection);
		const float RouteAlignment = RouteDirection.IsNearlyZero()
			? 0.0f
			: FVector::DotProduct(CandidateDirection, RouteDirection);
		float RouteDistancePenalty = 0.0f;
		if (Route)
		{
			const float CandidateRouteDistance = Route->FindDistanceClosestToWorldLocation(
				Candidate.Location);
			const FVector CandidateRouteLocation = Route->GetWorldLocationAtDistance(
				CandidateRouteDistance);
			RouteDistancePenalty = FVector::DistSquared2D(
				Candidate.Location,
				CandidateRouteLocation) * Goblin->GetFleeRouteDistancePenaltyWeight();
		}
		const float Score = MinimumDistanceSquared
			+ AwayAlignment * FMath::Square(Goblin->GetFleeTravelDistance())
			+ RouteAlignment * FMath::Square(Goblin->GetFleeTravelDistance())
				* Goblin->GetFleeRouteDirectionWeight()
			- RouteDistancePenalty;
		if (!bFoundDestination || Score > BestScore)
		{
			BestScore = Score;
			OutDestination = Candidate.Location;
			bFoundDestination = true;
		}
	}
	return bFoundDestination;
}

bool ANPGoblinAIController::RequestMoveToLocation(
	const FVector& Destination,
	const float AcceptanceRadius)
{
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(Destination);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetAllowPartialPath(false);
	MoveRequest.SetReachTestIncludesAgentRadius(true);
	return MoveTo(MoveRequest) != EPathFollowingRequestResult::Failed;
}

ANPGoblinCharacter* ANPGoblinAIController::GetGoblin() const
{
	return Cast<ANPGoblinCharacter>(GetPawn());
}

ANPGoblinPatrolRoute* ANPGoblinAIController::GetUsablePatrolRoute() const
{
	const ANPGoblinCharacter* Goblin = GetGoblin();
	ANPGoblinPatrolRoute* Route = Goblin ? Goblin->GetPatrolRoute() : nullptr;
	return IsValid(Route) && Route->IsUsableRoute() ? Route : nullptr;
}
