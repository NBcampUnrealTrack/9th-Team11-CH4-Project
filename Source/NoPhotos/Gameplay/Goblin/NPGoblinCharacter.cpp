#include "NPGoblinCharacter.h"

#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Data/Structs/NPRelicData.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "NPGoblinAIController.h"
#include "NPGoblinPresentationDoor.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPGoblinCharacter, Log, All);

ANPGoblinCharacter::ANPGoblinCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	static ConstructorHelpers::FObjectFinder<UDataTable> RelicDropTableFinder(
		TEXT("/Game/NoPhotos/Table/Relic/DT_Relic.DT_Relic"));
	if (RelicDropTableFinder.Succeeded())
	{
		RelicDropTable = RelicDropTableFinder.Object;
	}

	AIControllerClass = ANPGoblinAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	PresentationDoorClass = ANPGoblinPresentationDoor::StaticClass();
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->MaxWalkSpeed = RoamMoveSpeed;
	}
}

void ANPGoblinCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		CurrentPhotoHP = GetMaxPhotoHP();
		ForceNetUpdate();
	}
	OnRep_CurrentPhotoHP();
	NotifyLifecycleStateChanged();
}

void ANPGoblinCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, LifecycleState);
	DOREPLIFETIME(ThisClass, CurrentPhotoHP);
}

void ANPGoblinCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelDoorPresentation();
	GetWorldTimerManager().ClearTimer(PhotoReactionTimer);
	Super::EndPlay(EndPlayReason);
}

void ANPGoblinCharacter::PrepareForSpawnPresentation()
{
	if (!HasAuthority())
	{
		return;
	}

	LifecycleState = ENPGoblinLifecycleState::Spawning;
	ForceNetUpdate();
}

void ANPGoblinCharacter::FinishSpawnPresentation()
{
	if (HasAuthority() && LifecycleState == ENPGoblinLifecycleState::Spawning
		&& DoorPresentationPhase == EDoorPresentationPhase::None)
	{
		SetLifecycleState(ENPGoblinLifecycleState::Active);
	}
}

void ANPGoblinCharacter::BeginDespawnPresentation()
{
	if (HasAuthority() && LifecycleState != ENPGoblinLifecycleState::Despawning)
	{
		SetLifecycleState(ENPGoblinLifecycleState::Despawning);
	}
}

void ANPGoblinCharacter::FinishDespawnPresentation()
{
	if (HasAuthority() && LifecycleState == ENPGoblinLifecycleState::Despawning
		&& DoorPresentationPhase == EDoorPresentationPhase::None)
	{
		Destroy();
	}
}

void ANPGoblinCharacter::SetLifecycleState(const ENPGoblinLifecycleState NewState)
{
	if (!HasAuthority() || LifecycleState == NewState)
	{
		return;
	}

	LifecycleState = NewState;
	NotifyLifecycleStateChanged();
	ForceNetUpdate();
}

void ANPGoblinCharacter::NotifyLifecycleStateChanged()
{
	if (LastNotifiedLifecycleState == LifecycleState)
	{
		return;
	}

	const bool bCancelHiddenSpawn = HasAuthority()
		&& LifecycleState == ENPGoblinLifecycleState::Despawning
		&& LastNotifiedLifecycleState == ENPGoblinLifecycleState::Spawning
		&& IsHidden();
	LastNotifiedLifecycleState = LifecycleState;
	GetWorldTimerManager().ClearTimer(PresentationTimeoutTimer);
	if (HasAuthority())
	{
		CancelDoorPresentation();
	}

	ANPGoblinAIController* GoblinController = Cast<ANPGoblinAIController>(GetController());
	const bool bShouldEnableGameplay = LifecycleState == ENPGoblinLifecycleState::Active;
	if (!bShouldEnableGameplay)
	{
		StopPhotoReaction();
	}
	if (HasAuthority() && GoblinController)
	{
		GoblinController->SetGameplayEnabled(bShouldEnableGameplay);
	}

	switch (LifecycleState)
	{
	case ENPGoblinLifecycleState::Spawning:
		if (bUseDoorPresentation)
		{
			if (HasAuthority() && !StartDoorPresentation())
			{
				FinishSpawnPresentation();
			}
			break;
		}
		if (HasAuthority())
		{
			if (SpawnPresentationTimeout <= 0.0f)
			{
				FinishSpawnPresentation();
				return;
			}
			GetWorldTimerManager().SetTimer(
				PresentationTimeoutTimer,
				this,
				&ThisClass::FinishSpawnPresentation,
				SpawnPresentationTimeout,
				false);
		}
		BP_OnSpawnPresentationStarted();
		break;

	case ENPGoblinLifecycleState::Active:
		BP_OnGameplayActivated();
		break;

	case ENPGoblinLifecycleState::Despawning:
		if (bCancelHiddenSpawn)
		{
			// The event ended before the goblin emerged: close the entrance without revealing it.
			FinishDespawnPresentation();
			break;
		}
		if (bUseDoorPresentation)
		{
			if (HasAuthority() && !StartDoorPresentation())
			{
				FinishDespawnPresentation();
			}
			break;
		}
		if (HasAuthority())
		{
			if (DespawnPresentationTimeout <= 0.0f)
			{
				FinishDespawnPresentation();
				return;
			}
			GetWorldTimerManager().SetTimer(
				PresentationTimeoutTimer,
				this,
				&ThisClass::FinishDespawnPresentation,
				DespawnPresentationTimeout,
				false);
		}
		BP_OnDespawnPresentationStarted();
		break;

	default:
		break;
	}
}

bool ANPGoblinCharacter::FindDoorTravelLocation(FVector& OutFloorLocation, FTransform& OutDoorTransform) const
{
	UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANPGoblinPresentationDoor* DoorDefaults = PresentationDoorClass.GetDefaultObject();
	if (!Navigation || !DoorDefaults)
	{
		return false;
	}
	const FVector Origin = GetActorLocation();
	const FVector Feet = Origin - FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FNavLocation StartFloor;
	if (!Navigation->ProjectPointToNavigation(Feet, StartFloor, FVector(50.0f, 50.0f, 150.0f)))
	{
		return false;
	}
	const float Distance = FMath::Max(100.0f, DoorTravelDistance);
	const float Angles[] = { 0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 180.0f };
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GoblinDoorClearance), false, this);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		GetCapsuleComponent()->GetScaledCapsuleRadius(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	for (const float Angle : Angles)
	{
		const FVector Direction = GetActorForwardVector().RotateAngleAxis(Angle, FVector::UpVector).GetSafeNormal2D();
		FNavLocation Projected;
		if (!Navigation->ProjectPointToNavigation(Feet + Direction * Distance,
			Projected, FVector(80.0f, 80.0f, 150.0f))
			|| FVector::DistSquared2D(Feet, Projected.Location) < FMath::Square(100.0f))
		{
			continue;
		}
		const FVector CapsuleCenter = Projected.Location
			+ FVector::UpVector * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f);
		if (GetWorld()->OverlapBlockingTestByProfile(CapsuleCenter, FQuat::Identity,
			GetCapsuleComponent()->GetCollisionProfileName(), CapsuleShape, QueryParams))
		{
			continue;
		}
		const bool bEnteringWorld = LifecycleState == ENPGoblinLifecycleState::Spawning;
		const FVector Outward = (bEnteringWorld ? Projected.Location - StartFloor.Location
			: StartFloor.Location - Projected.Location).GetSafeNormal2D();
		// For exit, the movement goal is behind the black interior, not in front of the door.
		const FVector DoorLocation = bEnteringWorld ? StartFloor.Location
			: Projected.Location + Outward * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 20.0f);
		const FVector DoorExtent = DoorDefaults->GetClearanceHalfExtent();
		if (GetWorld()->OverlapBlockingTestByProfile(
			DoorLocation + FVector::UpVector * (DoorExtent.Z + 2.0f), Outward.Rotation().Quaternion(),
			GetCapsuleComponent()->GetCollisionProfileName(), FCollisionShape::MakeBox(DoorExtent), QueryParams))
		{
			continue;
		}
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			GetWorld(), Origin, Projected.Location, const_cast<ANPGoblinCharacter*>(this));
		if (Path && Path->IsValid() && !Path->IsPartial()
			&& Path->GetPathLength() < Distance * 2.0f)
		{
			OutFloorLocation = Projected.Location;
			OutDoorTransform = FTransform(Outward.Rotation(), DoorLocation);
			return true;
		}
	}
	return false;
}

bool ANPGoblinCharacter::StartDoorPresentation()
{
	AAIController* AI = Cast<AAIController>(GetController());
	FVector TravelFloorLocation;
	FTransform DoorTransform;
	if (!PresentationDoorClass || !AI || !FindDoorTravelLocation(TravelFloorLocation, DoorTransform))
	{
		UE_LOG(LogNPGoblinCharacter, Warning,
			TEXT("[GoblinDoor] 문 출입 위치를 찾지 못해 연출을 생략합니다. Goblin=%s (문 클래스/AI/NavMesh/이동 공간 확인)"),
			*GetNameSafe(this));
		return false;
	}

	const bool bEnteringWorld = LifecycleState == ENPGoblinLifecycleState::Spawning;
	FActorSpawnParameters Parameters;
	// No owner/attachment to the goblin: this actor must finish closing after the goblin is gone.
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PresentationDoor = GetWorld()->SpawnActor<ANPGoblinPresentationDoor>(
		PresentationDoorClass, DoorTransform.GetLocation(), DoorTransform.Rotator(), Parameters);
	if (!IsValid(PresentationDoor))
	{
		return false;
	}
	DoorMoveTarget = TravelFloorLocation;
	DoorPresentationPhase = EDoorPresentationPhase::Opening;
	DoorPhaseEndTime = GetWorld()->GetTimeSeconds() + PresentationDoor->GetOpenDuration() + 0.1f;
	DoorDeadline = GetWorld()->GetTimeSeconds() + FMath::Max(1.0f, DoorPresentationTimeout);
	AI->StopMovement();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(1.0f, DoorWalkSpeed);
	if (bEnteringWorld)
	{
		SetActorRotation(DoorTransform.Rotator());
		SetActorHiddenInGame(true);
	}
	GetWorldTimerManager().SetTimer(DoorPresentationTimer, this,
		&ThisClass::UpdateDoorPresentation, 0.05f, true);
	ForceNetUpdate();
	UE_LOG(LogNPGoblinCharacter, Display, TEXT("[GoblinDoor] %s Goblin=%s Door=%s"),
		bEnteringWorld ? TEXT("SPAWN") : TEXT("DESPAWN"), *GetNameSafe(this), *GetNameSafe(PresentationDoor.Get()));
	return true;
}

void ANPGoblinCharacter::UpdateDoorPresentation()
{
	if (!HasAuthority() || DoorPresentationPhase == EDoorPresentationPhase::None)
	{
		return;
	}
	AAIController* AI = Cast<AAIController>(GetController());
	const double Now = GetWorld()->GetTimeSeconds();
	if (!AI || Now >= DoorDeadline
		|| (!IsValid(PresentationDoor) && DoorPresentationPhase != EDoorPresentationPhase::Closing))
	{
		UE_LOG(LogNPGoblinCharacter, Warning, TEXT("[GoblinDoor] 이동/문 연출 중단 또는 시간 초과. Goblin=%s"), *GetNameSafe(this));
		CompleteDoorPresentation();
		return;
	}

	if (DoorPresentationPhase == EDoorPresentationPhase::Opening && Now >= DoorPhaseEndTime)
	{
		SetActorHiddenInGame(false);
		FAIMoveRequest MoveRequest;
		MoveRequest.SetGoalLocation(DoorMoveTarget);
		MoveRequest.SetAcceptanceRadius(10.0f);
		MoveRequest.SetReachTestIncludesAgentRadius(false);
		MoveRequest.SetUsePathfinding(true);
		MoveRequest.SetProjectGoalLocation(true);
		MoveRequest.SetAllowPartialPath(false);
		if (AI->MoveTo(MoveRequest) == EPathFollowingRequestResult::Failed)
		{
			CompleteDoorPresentation();
			return;
		}
		DoorPresentationPhase = EDoorPresentationPhase::Walking;
		ForceNetUpdate();
	}
	if (DoorPresentationPhase == EDoorPresentationPhase::Walking)
	{
		const FVector Feet = GetActorLocation() - FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (FVector::DistSquared2D(Feet, DoorMoveTarget) <= FMath::Square(25.0f)
			&& FMath::Abs(Feet.Z - DoorMoveTarget.Z) < 50.0f)
		{
			AI->StopMovement();
			GetCharacterMovement()->StopMovementImmediately();
			if (LifecycleState == ENPGoblinLifecycleState::Despawning)
			{
				SetActorHiddenInGame(true);
			}
			PresentationDoor->CloseAndDestroy();
			DoorPhaseEndTime = Now + PresentationDoor->GetCloseDuration() + 0.1f;
			DoorPresentationPhase = EDoorPresentationPhase::Closing;
			ForceNetUpdate();
		}
		else if (AI->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			// Aborted/blocked movement must not leave a stationary goblin in a presentation state.
			CompleteDoorPresentation();
		}
	}
	else if (DoorPresentationPhase == EDoorPresentationPhase::Closing && Now >= DoorPhaseEndTime)
	{
		CompleteDoorPresentation();
	}
}

void ANPGoblinCharacter::CancelDoorPresentation()
{
	GetWorldTimerManager().ClearTimer(DoorPresentationTimer);
	if (HasAuthority() && IsValid(PresentationDoor))
	{
		PresentationDoor->CloseAndDestroy();
	}
	PresentationDoor = nullptr;
	DoorPresentationPhase = EDoorPresentationPhase::None;
}

void ANPGoblinCharacter::CompleteDoorPresentation()
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	CancelDoorPresentation();
	if (LifecycleState == ENPGoblinLifecycleState::Spawning)
	{
		SetActorHiddenInGame(false);
		FinishSpawnPresentation();
	}
	else if (LifecycleState == ENPGoblinLifecycleState::Despawning)
	{
		FinishDespawnPresentation();
	}
}

void ANPGoblinCharacter::OnRep_LifecycleState()
{
	NotifyLifecycleStateChanged();
}

void ANPGoblinCharacter::OnRep_CurrentPhotoHP()
{
	BP_OnPhotoHPChanged(CurrentPhotoHP, GetMaxPhotoHP());
}

bool ANPGoblinCharacter::CanBePhotographed_Implementation(APlayerState* Photographer) const
{
	return HasAuthority()
		&& IsGameplayActive()
		&& IsValid(Photographer)
		&& CurrentPhotoHP > 0;
}

void ANPGoblinCharacter::OnPhotographed_Implementation(
	APlayerState* Photographer,
	const float Visibility,
	const int32 CaptureSequence)
{
	if (!HasAuthority()
		|| !IsGameplayActive()
		|| !IsValid(Photographer)
		|| CurrentPhotoHP <= 0)
	{
		return;
	}

	const int32 PreviousPhotoHP = CurrentPhotoHP;
	const int32 AppliedDamage = FMath::Min(
		CurrentPhotoHP,
		FMath::Max(1, PhotoDamagePerCapture));
	CurrentPhotoHP -= AppliedDamage;
	// BP 콜백 이후 HP가 바뀌어도 이 촬영이 처치한 것인지 판정은 유지합니다.
	const bool bDefeatedByThisPhoto = CurrentPhotoHP == 0;
	OnRep_CurrentPhotoHP();
	ForceNetUpdate();
	UE_LOG(
		LogNPPhoto,
		Display,
		TEXT("[GoblinPhoto] HIT Goblin=%s Photographer=%s Visibility=%.2f Sequence=%d Damage=%d HP=%d->%d/%d"),
		*GetNameSafe(this),
		*GetNameSafe(Photographer),
		Visibility,
		CaptureSequence,
		AppliedDamage,
		PreviousPhotoHP,
		CurrentPhotoHP,
		GetMaxPhotoHP());

	const int32 PhotoDropCount = FMath::Clamp(PhotoRelicDropCount, 0, 100);
	const int32 DefeatDropCount = bDefeatedByThisPhoto ? FMath::Clamp(DefeatRelicDropCount, 0, 100) : 0;
	for (int32 Index = 0; Index < PhotoDropCount + DefeatDropCount; ++Index)
	{
		TrySpawnPhotographedRelic();
	}
	OnGoblinPhotographed.Broadcast(Photographer, Visibility, CaptureSequence);
	BP_OnPhotographed(Photographer, Visibility, CaptureSequence);

	if (bDefeatedByThisPhoto)
	{
		UE_LOG(
			LogNPPhoto,
			Display,
			TEXT("[GoblinPhoto] HP_DEPLETED Goblin=%s Photographer=%s Sequence=%d"),
			*GetNameSafe(this),
			*GetNameSafe(Photographer),
			CaptureSequence);
		BP_OnPhotoHPDepleted(Photographer);
		BeginDespawnPresentation();
	}
}

void ANPGoblinCharacter::OnPhotographedFromCamera_Implementation(
	APlayerState* Photographer,
	const float Visibility,
	const int32 CaptureSequence,
	const FVector CameraLocation,
	const FVector CameraForward)
{
	// The legacy callback has already applied damage and spawned the reward.
	// A lethal photo (or a BP-triggered despawn) must not start another reaction.
	if (!CanBePhotographed_Implementation(Photographer) || !bFleeWhenPhotographed)
	{
		return;
	}

	FVector FleeDirection = (GetActorLocation() - CameraLocation).GetSafeNormal2D();
	if (FleeDirection.IsNearlyZero())
	{
		FleeDirection = CameraForward.GetSafeNormal2D();
	}
	if (FleeDirection.IsNearlyZero())
	{
		FleeDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	ANPGoblinAIController* GoblinController = Cast<ANPGoblinAIController>(GetController());
	if (GoblinController && GoblinController->StartPhotoFlee(CameraLocation, FleeDirection))
	{
		UE_LOG(LogNPPhoto, Display,
			TEXT("[GoblinPhoto] ESCAPE Goblin=%s Direction=%s Reaction=%.2fs Flee=%.2fs"),
			*GetNameSafe(this), *FleeDirection.ToCompactString(),
			GetPhotoReactionDuration(), GetPhotoFleeDuration());
		Multicast_PlayPhotoReaction(FleeDirection, GetPhotoReactionDuration());
	}
}

void ANPGoblinCharacter::Multicast_PlayPhotoReaction_Implementation(
	const FVector FleeDirection, const float ReactionDuration)
{
	StopPhotoReaction();
	if (LifecycleState == ENPGoblinLifecycleState::Despawning || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (PhotoReactionMontage && ReactionDuration > 0.0f)
	{
		PlayAnimMontage(PhotoReactionMontage);
		GetWorldTimerManager().SetTimer(PhotoReactionTimer, this,
			&ThisClass::StopPhotoReaction, ReactionDuration, false);
	}
	BP_OnPhotoEscapeStarted(FleeDirection, ReactionDuration);
}

void ANPGoblinCharacter::StopPhotoReaction()
{
	GetWorldTimerManager().ClearTimer(PhotoReactionTimer);
	if (PhotoReactionMontage)
	{
		StopAnimMontage(PhotoReactionMontage);
	}
}

void ANPGoblinCharacter::TrySpawnPhotographedRelic()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	TSubclassOf<ANPBaseRelic> SelectedRelicClass = PhotographedRelicClass;
	FDataTableRowHandle SelectedRelicData;

	if (RelicDropTable)
	{
		TArray<TPair<FName, TSubclassOf<ANPBaseRelic>>> ValidRelics;
		for (const FName RowName : RelicDropTable->GetRowNames())
		{
			const FNPRelicTableRow* Row = RelicDropTable->FindRow<FNPRelicTableRow>(
				RowName,
				TEXT("GoblinRelicDrop"),
				false);
			UClass* RelicClass = Row ? Row->RelicClass.LoadSynchronous() : nullptr;
			if (RelicClass && RelicClass->IsChildOf(ANPBaseRelic::StaticClass())
				&& !RelicClass->HasAnyClassFlags(CLASS_Abstract))
			{
				ValidRelics.Emplace(RowName, RelicClass);
			}
		}

		if (!ValidRelics.IsEmpty())
		{
			const TPair<FName, TSubclassOf<ANPBaseRelic>>& SelectedRelic =
				ValidRelics[FMath::RandRange(0, ValidRelics.Num() - 1)];
			SelectedRelicClass = SelectedRelic.Value;
			SelectedRelicData.DataTable = RelicDropTable;
			SelectedRelicData.RowName = SelectedRelic.Key;
		}
		else
		{
			UE_LOG(
				LogNPGoblinCharacter,
				Warning,
				TEXT("촬영 보상 테이블에 유효한 유물 행이 없습니다. Goblin=%s Table=%s"),
				*GetNameSafe(this),
				*GetNameSafe(RelicDropTable));
		}
	}

	if (!SelectedRelicClass)
	{
		UE_LOG(
			LogNPGoblinCharacter,
			Warning,
			TEXT("촬영 보상 유물 클래스가 설정되지 않았습니다. Goblin=%s"),
			*GetNameSafe(this));
		return;
	}

	const FVector SpawnLocation = GetActorLocation()
		+ GetActorTransform().TransformVectorNoScale(PhotographedRelicSpawnOffset);
	const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);
	ANPBaseRelic* Relic = World->SpawnActorDeferred<ANPBaseRelic>(
		SelectedRelicClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(Relic))
	{
		UE_LOG(
			LogNPGoblinCharacter,
			Error,
			TEXT("촬영 보상 유물 생성에 실패했습니다. Goblin=%s RelicClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SelectedRelicClass.Get()));
		return;
	}

	if (!SelectedRelicData.RowName.IsNone())
	{
		Relic->SetRelicTableData(SelectedRelicData);
	}
	Relic->FinishSpawning(SpawnTransform);

	const FVector2D HorizontalDirection = FMath::RandPointInCircle(1.0f).GetSafeNormal();
	const FVector LaunchVelocity(
		HorizontalDirection.X * FMath::Max(0.0f, PhotographedRelicHorizontalLaunchSpeed),
		HorizontalDirection.Y * FMath::Max(0.0f, PhotographedRelicHorizontalLaunchSpeed),
		FMath::Max(0.0f, PhotographedRelicUpwardLaunchSpeed));
	const bool bLaunched = Relic->ReleaseWithVelocityImpulse(LaunchVelocity);
	SpawnedPhotoRelic = Relic;
	UE_LOG(
		LogNPGoblinCharacter,
		Display,
		TEXT("고블린 촬영 보상 유물 생성 완료. Goblin=%s Relic=%s Row=%s Location=%s PhysicsLaunch=%s Velocity=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Relic),
		*SelectedRelicData.RowName.ToString(),
		*Relic->GetActorLocation().ToCompactString(),
		bLaunched ? TEXT("true") : TEXT("false"),
		*LaunchVelocity.ToCompactString());
}
