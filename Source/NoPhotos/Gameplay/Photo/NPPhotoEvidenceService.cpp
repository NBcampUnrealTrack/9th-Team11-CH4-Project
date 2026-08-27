#include "Gameplay/Photo/NPPhotoEvidenceService.h"

#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Data/Interface/NPPhotoReactiveTarget.h"
#include "Gameplay/Photo/NPRelicHolderInterface.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/NPBreakableRelic.h"
#include "Core/Main/NPMainGameMode.h"

void UNPPhotoEvidenceService::Initialize(ANPMainGameMode* InOwningGameMode)
{
	OwningGameMode = InOwningGameMode;
}

UWorld* UNPPhotoEvidenceService::GetWorld() const
{
	return OwningGameMode.IsValid() ? OwningGameMode->GetWorld() : nullptr;
}

FNPPhotoEvidenceResult UNPPhotoEvidenceService::EvaluatePhoto(
	const FNPPhotoCaptureRequest& Request)
{
	FNPPhotoEvidenceResult Result;
	Result.CaptureSequence = Request.CaptureSequence;
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[Evidence] Evaluation started. Photographer=%s Sequence=%u"),
		*GetNameSafe(Request.Photographer),
		Request.CaptureSequence);
	Result.Photographer = IsValid(Request.Photographer)
		? Request.Photographer->PlayerState
		: nullptr;
	if (!ValidateRequest(Request, Result))
	{
		return Result;
	}

	APawn* PhotographerPawn = Request.Photographer->GetPawn();
	const float MaximumDistanceSquared = FMath::Square(MaximumCaptureDistance);

	// 유물 증거 판정과 독립적으로 사진에 들어온 플레이어를 먼저 찾습니다.
	float BestPlayerVisibility = -1.0f;
	for (TActorIterator<ANPReplicatedStablePhysicsPawn> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ANPReplicatedStablePhysicsPawn* CandidatePlayer = *Iterator;
		if (!IsValid(CandidatePlayer)
			|| CandidatePlayer == PhotographerPawn
			|| !IsValid(CandidatePlayer->GetPlayerState())
			|| FVector::DistSquared(
				Request.CameraLocation,
				CandidatePlayer->GetActorLocation()) > MaximumDistanceSquared)
		{
			continue;
		}

		const float PlayerVisibility = CalculateActorVisibility(
			Request,
			CandidatePlayer,
			PhotographerPawn);
		if (PlayerVisibility < MinimumPlayerCaptureVisibility
			|| PlayerVisibility <= BestPlayerVisibility)
		{
			continue;
		}

		BestPlayerVisibility = PlayerVisibility;
		Result.bPlayerCaptured = true;
		Result.CapturedPlayer = CandidatePlayer->GetPlayerState();
		Result.CapturedPlayerVisibility = PlayerVisibility;
		Result.ServerCaptureTime = GetWorld()->GetTimeSeconds();
	}

	if (Result.bPlayerCaptured)
	{
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[Evidence] Player captured independently. Player=%s Visibility=%.2f"),
			*GetNameSafe(Result.CapturedPlayer.Get()),
			Result.CapturedPlayerVisibility);
	}

	float BestEvidenceQuality = -1.0f;
	for (TActorIterator<APawn> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		APawn* CandidateThief = *Iterator;
		if (!IsValid(CandidateThief)
			|| CandidateThief == PhotographerPawn
			|| !CandidateThief->GetClass()->ImplementsInterface(UNPRelicHolderInterface::StaticClass()))
		{
			continue;
		}

		AActor* HeldRelic = INPRelicHolderInterface::Execute_GetHeldRelic(CandidateThief);
		UE_LOG(
			LogNPPhoto,
			Verbose,
			TEXT("[Evidence] Holder candidate. Thief=%s Relic=%s"),
			*GetNameSafe(CandidateThief),
			*GetNameSafe(HeldRelic));
		if (!IsValid(HeldRelic) || !HeldRelic->IsA<ANPBaseRelic>())
		{
			continue;
		}

		if (const ANPBreakableRelic* BreakableRelic = Cast<ANPBreakableRelic>(HeldRelic);
			BreakableRelic && BreakableRelic->IsBroken())
		{
			UE_LOG(
				LogNPPhoto,
				Verbose,
				TEXT("[Evidence] Broken relic rejected. Thief=%s Relic=%s"),
				*GetNameSafe(CandidateThief),
				*GetNameSafe(HeldRelic));
			continue;
		}

		if (FVector::DistSquared(Request.CameraLocation, CandidateThief->GetActorLocation())
			> MaximumDistanceSquared
			|| FVector::DistSquared(Request.CameraLocation, HeldRelic->GetActorLocation())
			> MaximumDistanceSquared)
		{
			continue;
		}

		const float ThiefVisibility = CalculateActorVisibility(
			Request, CandidateThief, PhotographerPawn);
		const float RelicVisibility = CalculateActorVisibility(
			Request, HeldRelic, PhotographerPawn);
		if (ThiefVisibility < MinimumThiefVisibility
			|| RelicVisibility < MinimumRelicVisibility)
		{
			UE_LOG(
				LogNPPhoto,
				Verbose,
				TEXT("[Evidence] Visibility rejected. Thief=%s Relic=%s ThiefVisibility=%.2f RelicVisibility=%.2f"),
				*GetNameSafe(CandidateThief),
				*GetNameSafe(HeldRelic),
				ThiefVisibility,
				RelicVisibility);
			continue;
		}

		const float EvidenceQuality = (ThiefVisibility + RelicVisibility) * 0.5f;
		if (EvidenceQuality <= BestEvidenceQuality)
		{
			continue;
		}

		BestEvidenceQuality = EvidenceQuality;
		Result.bSuccess = true;
		Result.FailureReason = ENPPhotoEvidenceFailureReason::None;
		Result.Thief = CandidateThief->GetPlayerState();
		Result.Relic = HeldRelic;
		Result.ThiefVisibility = ThiefVisibility;
		Result.RelicVisibility = RelicVisibility;
		Result.ServerCaptureTime = GetWorld()->GetTimeSeconds();
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[Evidence] Valid candidate. Thief=%s Relic=%s ThiefVisibility=%.2f RelicVisibility=%.2f"),
			*GetNameSafe(CandidateThief),
			*GetNameSafe(HeldRelic),
			ThiefVisibility,
			RelicVisibility);
	}

	AActor* BestReactiveTarget = nullptr;
	float BestReactiveVisibility = -1.0f;
	for (TActorIterator<AActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		AActor* CandidateTarget = *Iterator;
		if (!IsValid(CandidateTarget)
			|| CandidateTarget == PhotographerPawn
			|| !CandidateTarget->GetClass()->ImplementsInterface(UNPPhotoReactiveTarget::StaticClass())
			|| FVector::DistSquared(Request.CameraLocation, CandidateTarget->GetActorLocation())
				> MaximumDistanceSquared
			|| !INPPhotoReactiveTarget::Execute_CanBePhotographed(
				CandidateTarget,
				Result.Photographer.Get()))
		{
			continue;
		}

		const float Visibility = CalculateActorVisibility(
			Request,
			CandidateTarget,
			PhotographerPawn);
		if (Visibility < MinimumReactiveTargetVisibility
			|| Visibility <= BestReactiveVisibility)
		{
			continue;
		}

		BestReactiveTarget = CandidateTarget;
		BestReactiveVisibility = Visibility;
	}

	if (IsValid(BestReactiveTarget))
	{
		Result.bReactiveTargetSuccess = true;
		Result.ReactiveTarget = BestReactiveTarget;
		Result.ReactiveTargetVisibility = BestReactiveVisibility;
		Result.ServerCaptureTime = GetWorld()->GetTimeSeconds();
		INPPhotoReactiveTarget::Execute_OnPhotographed(
			BestReactiveTarget,
			Result.Photographer.Get(),
			BestReactiveVisibility,
			Result.CaptureSequence);

		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[Evidence] Reactive target success. Target=%s Visibility=%.2f Photographer=%s"),
			*GetNameSafe(BestReactiveTarget),
			BestReactiveVisibility,
			*GetNameSafe(Result.Photographer.Get()));
	}

	if (!Result.bSuccess && !Result.bReactiveTargetSuccess)
	{
		Result.FailureReason = ENPPhotoEvidenceFailureReason::NoValidEvidence;
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[Evidence] Failed: no valid thief/relic pair or reactive target."));
	}
	else
	{
		Result.FailureReason = ENPPhotoEvidenceFailureReason::None;
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[Evidence] Success. RelicEvidence=%s Thief=%s Relic=%s ReactiveTarget=%s"),
			Result.bSuccess ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Result.Thief.Get()),
			*GetNameSafe(Result.Relic.Get()),
			*GetNameSafe(Result.ReactiveTarget.Get()));
	}
	return Result;
}

bool UNPPhotoEvidenceService::ValidateRequest(
	const FNPPhotoCaptureRequest& Request,
	FNPPhotoEvidenceResult& OutResult)
{
	UWorld* World = GetWorld();
	APlayerController* Photographer = Request.Photographer;
	if (!World || !IsValid(Photographer) || !IsValid(Photographer->GetPawn())
		|| !IsValid(Photographer->PlayerState))
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Evidence] Request rejected: invalid photographer."));
		OutResult.FailureReason = ENPPhotoEvidenceFailureReason::InvalidPhotographer;
		return false;
	}

	const UNPStablePhysicsGrabComponent* GrabComponent =
		Photographer->GetPawn()->FindComponentByClass<UNPStablePhysicsGrabComponent>();
	if (GrabComponent && GrabComponent->IsHoldingObject())
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Evidence] Request rejected: photographer is grabbing."));
		OutResult.FailureReason = ENPPhotoEvidenceFailureReason::PhotographerIsGrabbing;
		return false;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (const double* LastCaptureTime = LastCaptureTimes.Find(Photographer))
	{
		if (CurrentTime - *LastCaptureTime < ServerCaptureCooldown)
		{
			UE_LOG(LogNPPhoto, Warning, TEXT("[Evidence] Request rejected: server cooldown."));
			OutResult.FailureReason = ENPPhotoEvidenceFailureReason::CaptureOnCooldown;
			return false;
		}
	}

	APawn* PhotographerPawn = Photographer->GetPawn();
	const ANPReplicatedStablePhysicsPawn* StablePhysicsPawn =
		Cast<ANPReplicatedStablePhysicsPawn>(PhotographerPawn);
	const FRotator ServerViewRotation = StablePhysicsPawn
		? StablePhysicsPawn->GetServerViewRotation()
		: PhotographerPawn->GetViewRotation();
	const FVector RequestForward = Request.CameraForward.GetSafeNormal();
	const float MinimumDirectionDot = FMath::Cos(FMath::DegreesToRadians(MaximumCameraDirectionError));
	const float CameraDistanceFromPawn = FVector::Distance(
		Request.CameraLocation,
		PhotographerPawn->GetActorLocation());
	const float DirectionDot = FVector::DotProduct(
		RequestForward,
		ServerViewRotation.Vector());
	if (RequestForward.IsNearlyZero()
		|| CameraDistanceFromPawn > MaximumCameraDistanceFromPawn
		|| DirectionDot < MinimumDirectionDot)
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[Evidence] Request rejected: invalid camera. DistanceFromPawn=%.1f MaximumDistance=%.1f DirectionDot=%.3f RequiredDot=%.3f"),
			CameraDistanceFromPawn,
			MaximumCameraDistanceFromPawn,
			DirectionDot,
			MinimumDirectionDot);
		OutResult.FailureReason = ENPPhotoEvidenceFailureReason::InvalidCamera;
		return false;
	}

	LastCaptureTimes.Add(Photographer, CurrentTime);
	return true;
}

bool UNPPhotoEvidenceService::IsInsideCameraFOV(
	const FNPPhotoCaptureRequest& Request,
	const FVector& TargetLocation) const
{
	const FVector Forward = Request.CameraForward.GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
	const FVector ToTarget = TargetLocation - Request.CameraLocation;
	const float ForwardDistance = FVector::DotProduct(ToTarget, Forward);
	if (ForwardDistance <= 0.0f || Right.IsNearlyZero() || Up.IsNearlyZero())
	{
		return false;
	}

	const float HalfHorizontalFOV = HorizontalFOV * FOVAcceptanceScale * 0.5f;
	const float HorizontalTangent = FMath::Tan(FMath::DegreesToRadians(HalfHorizontalFOV));
	const float VerticalTangent = HorizontalTangent / FMath::Max(CaptureAspectRatio, 0.1f);
	const float HorizontalOffset = FMath::Abs(FVector::DotProduct(ToTarget, Right));
	const float VerticalOffset = FMath::Abs(FVector::DotProduct(ToTarget, Up));
	return HorizontalOffset <= ForwardDistance * HorizontalTangent
		&& VerticalOffset <= ForwardDistance * VerticalTangent;
}

float UNPPhotoEvidenceService::CalculateActorVisibility(
	const FNPPhotoCaptureRequest& Request,
	AActor* TargetActor,
	APawn* PhotographerPawn) const
{
	if (!IsValid(TargetActor) || !GetWorld())
	{
		return 0.0f;
	}

	TArray<FVector> SamplePoints;
	BuildActorSamplePoints(TargetActor, SamplePoints);
	if (SamplePoints.IsEmpty())
	{
		return 0.0f;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhotoEvidenceVisibility), true);
	QueryParams.AddIgnoredActor(PhotographerPawn);
	int32 VisiblePointCount = 0;
	for (const FVector& SamplePoint : SamplePoints)
	{
		if (!IsInsideCameraFOV(Request, SamplePoint))
		{
			continue;
		}

		FHitResult Hit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			Hit,
			Request.CameraLocation,
			SamplePoint,
			ECC_Visibility,
			QueryParams);
		if (!bBlocked || Hit.GetActor() == TargetActor)
		{
			++VisiblePointCount;
		}
	}

	return static_cast<float>(VisiblePointCount) / SamplePoints.Num();
}

void UNPPhotoEvidenceService::BuildActorSamplePoints(
	AActor* TargetActor,
	TArray<FVector>& OutPoints) const
{
	FVector Origin;
	FVector Extent;
	TargetActor->GetActorBounds(true, Origin, Extent);
	OutPoints.Reserve(5);
	OutPoints.Add(Origin);
	OutPoints.Add(Origin + FVector(0.0f, 0.0f, Extent.Z * 0.75f));
	OutPoints.Add(Origin - FVector(0.0f, 0.0f, Extent.Z * 0.75f));
	OutPoints.Add(Origin + FVector(Extent.X * 0.5f, Extent.Y * 0.5f, 0.0f));
	OutPoints.Add(Origin - FVector(Extent.X * 0.5f, Extent.Y * 0.5f, 0.0f));
}
