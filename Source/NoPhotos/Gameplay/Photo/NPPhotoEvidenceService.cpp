#include "Gameplay/Photo/NPPhotoEvidenceService.h"

#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Data/Interface/NPPhotoReactiveTarget.h"
#include "Gameplay/Photo/NPRelicHolderInterface.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Relic/NPBreakableRelic.h"
#include "Core/Main/NPMainGameMode.h"

UNPPhotoEvidenceService::UNPPhotoEvidenceService()
{
	HeadVisibilitySampleNames = {
		TEXT("PhotoHeadUDBL"), TEXT("PhotoHeadUDBR"),
		TEXT("PhotoHeadUFR"), TEXT("PhotoHeadUFL"),
		TEXT("PhotoHeadBR"), TEXT("PhotoHeadBL"),
		TEXT("PhotoHeadFR"), TEXT("PhotoHeadFL"),
		TEXT("PhotoHeadUL"), TEXT("PhotoHeadUR"),
		TEXT("PhotoHeadUB"), TEXT("PhotoHeadUF"),
		TEXT("PhotoHeadU"), TEXT("PhotoHeadF"),
		TEXT("PhotoHeadB"), TEXT("PhotoHeadR"),
		TEXT("PhotoHeadL"), TEXT("PhotoHeadC"),
		TEXT("head_end")
	};
}

void UNPPhotoEvidenceService::Initialize(ANPMainGameMode* InOwningGameMode)
{
	OwningGameMode = InOwningGameMode;
}

void UNPPhotoEvidenceService::SetMinimumVisibleHeadSampleCount(const int32 InSampleCount)
{
	MinimumVisibleHeadSampleCount = FMath::Clamp(
		InSampleCount,
		1,
		FMath::Max(1, HeadVisibilitySampleNames.Num()));
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

	for (TActorIterator<ANPReplicatedStablePhysicsPawn> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ANPReplicatedStablePhysicsPawn* CandidateThief = *Iterator;
		if (!IsValid(CandidateThief)
			|| CandidateThief == PhotographerPawn
			|| !IsValid(CandidateThief->GetPlayerState()))
		{
			continue;
		}

		AActor* HeldRelic = nullptr;
		if (!IsRelicHolderCapturable(
			Request,
			CandidateThief,
			PhotographerPawn,
			HeldRelic))
		{
			continue;
		}

		FNPPhotoRelicEvidenceGroup* EvidenceGroup = Result.RelicEvidenceGroups.FindByPredicate(
			[HeldRelic](const FNPPhotoRelicEvidenceGroup& Group)
			{
				return Group.Relic == HeldRelic;
			});
		if (!EvidenceGroup)
		{
			EvidenceGroup = &Result.RelicEvidenceGroups.AddDefaulted_GetRef();
			EvidenceGroup->Relic = HeldRelic;
		}
		EvidenceGroup->Thieves.AddUnique(CandidateThief->GetPlayerState());
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[Evidence] Valid relic holder. Thief=%s Relic=%s"),
			*GetNameSafe(CandidateThief),
			*GetNameSafe(HeldRelic));
	}
	Result.bSuccess = !Result.RelicEvidenceGroups.IsEmpty();
	if (Result.bSuccess)
	{
		Result.ServerCaptureTime = GetWorld()->GetTimeSeconds();
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
		// Keep the original callback intact, including Blueprint-only interface implementations.
		if (IsValid(BestReactiveTarget))
		{
			INPPhotoReactiveTarget::Execute_OnPhotographedFromCamera(
				BestReactiveTarget,
				Result.Photographer.Get(),
				BestReactiveVisibility,
				Result.CaptureSequence,
				Request.CameraLocation,
				Request.CameraForward);
		}

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
			TEXT("[Evidence] Success. RelicEvidence=%s RelicCount=%d ReactiveTarget=%s"),
			Result.bSuccess ? TEXT("true") : TEXT("false"),
			Result.RelicEvidenceGroups.Num(),
			*GetNameSafe(Result.ReactiveTarget.Get()));
	}
	return Result;
}

bool UNPPhotoEvidenceService::IsRelicHolderCapturable(
	const FNPPhotoCaptureRequest& Request,
	ANPReplicatedStablePhysicsPawn* TargetPawn,
	APawn* PhotographerPawn,
	AActor*& OutHeldRelic,
	const int32 RequiredVisibleHeadSampleCount) const
{
	OutHeldRelic = nullptr;
	if (!IsValid(TargetPawn) || TargetPawn == PhotographerPawn
		|| !TargetPawn->GetClass()->ImplementsInterface(UNPRelicHolderInterface::StaticClass())
		|| FVector::DistSquared(Request.CameraLocation, TargetPawn->GetActorLocation())
			> FMath::Square(MaximumCaptureDistance))
	{
		return false;
	}

	ANPBaseRelic* HeldRelic = Cast<ANPBaseRelic>(
		INPRelicHolderInterface::Execute_GetHeldRelic(TargetPawn));
	if (!IsValid(HeldRelic) || HeldRelic->IsReturned())
	{
		return false;
	}
	if (const ANPBreakableRelic* BreakableRelic = Cast<ANPBreakableRelic>(HeldRelic);
		BreakableRelic && BreakableRelic->IsBroken())
	{
		return false;
	}

	const int32 VisibleHeadSampleRequirement = RequiredVisibleHeadSampleCount > 0
		? RequiredVisibleHeadSampleCount
		: MinimumVisibleHeadSampleCount;
	if (CountVisibleHeadSamples(Request, TargetPawn, PhotographerPawn)
		< VisibleHeadSampleRequirement)
	{
		return false;
	}

	OutHeldRelic = HeldRelic;
	return true;
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

int32 UNPPhotoEvidenceService::CountVisibleHeadSamples(
	const FNPPhotoCaptureRequest& Request,
	ANPReplicatedStablePhysicsPawn* TargetPawn,
	APawn* PhotographerPawn) const
{
	if (!IsValid(TargetPawn))
	{
		return 0;
	}

	UWorld* World = TargetPawn->GetWorld();
	USkeletalMeshComponent* Mesh = TargetPawn->FindComponentByClass<USkeletalMeshComponent>();
	if (!World || !IsValid(Mesh))
	{
		return 0;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhotoHeadVisibility), true);
	QueryParams.AddIgnoredActor(PhotographerPawn);
	int32 VisiblePointCount = 0;
	for (const FName SampleName : HeadVisibilitySampleNames)
	{
		if (!Mesh->DoesSocketExist(SampleName))
		{
			continue;
		}

		const FVector SampleLocation = Mesh->GetSocketLocation(SampleName);
		if (!IsInsideCameraFOV(Request, SampleLocation))
		{
			continue;
		}

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(
			Hit,
			Request.CameraLocation,
			SampleLocation,
			ECC_Visibility,
			QueryParams);
		if (!bBlocked || Hit.GetActor() == TargetPawn)
		{
			++VisiblePointCount;
		}
	}

	return VisiblePointCount;
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
