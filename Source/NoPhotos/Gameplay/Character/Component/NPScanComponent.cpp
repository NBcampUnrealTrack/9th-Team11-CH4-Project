#include "Gameplay/Character/Component/NPScanComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Relic/NPBaseRelic.h"

UNPScanComponent::UNPScanComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	InitSphereRadius(1500.0f);
	SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	ShapeColor = FColor::Cyan;
}

void UNPScanComponent::StartScan(float MaxDistance)
{
	FinishScan();
	if (!GetWorld() || !FMath::IsFinite(MaxDistance) || MaxDistance <= 0.0f)
	{
		return;
	}

	MaxScanDistance = MaxDistance;
	ScanConeCos = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ScanHalfAngle, 0.0f, 89.0f)));
	CurrentScanDistance = 0.0f;

	SetAbsolute(true, true, true);
	SetWorldScale3D(FVector::OneVector);
	if (!UpdateScanView())
	{
		return;
	}

	const float CandidateRadius = MaxScanDistance / ScanConeCos
		+ FMath::Max(0.0f, CandidateMovementMargin);
	SetSphereRadius(CandidateRadius, false);
	bScanActive = true;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ScanCandidates), false, GetOwner());
	GetWorld()->OverlapMultiByObjectType(Overlaps, ScanOrigin, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(GetScaledSphereRadius()), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ANPBaseRelic* Relic = Cast<ANPBaseRelic>(Overlap.GetActor());
		if (!IsValid(Relic) || Relic->IsReturned())
		{
			continue;
		}

		PendingScanActors.Add(Relic);
	}
}

void UNPScanComponent::UpdateScanDistance(float Distance)
{
	if (!bScanActive || !FMath::IsFinite(Distance))
	{
		return;
	}
	if (!UpdateScanView())
	{
		return;
	}

	CurrentScanDistance = FMath::Clamp(Distance, CurrentScanDistance, MaxScanDistance);
	TArray<TWeakObjectPtr<AActor>> ReachedActors;
	for (auto Iterator = PendingScanActors.CreateIterator(); Iterator; ++Iterator)
	{
		AActor* Actor = (*Iterator).Get();
		if (!IsValid(Actor))
		{
			Iterator.RemoveCurrent();
		}
		else
		{
			const FVector ToActor = Actor->GetActorLocation() - ScanOrigin;
			const double ForwardDistance = FVector::DotProduct(ToActor, ScanForward);
			const double DirectionCos = ToActor.IsNearlyZero()
				? 1.0
				: FVector::DotProduct(ScanForward, ToActor.GetSafeNormal());
			const bool bReached = ForwardDistance >= 0.0
				&& ForwardDistance <= CurrentScanDistance
				&& DirectionCos >= ScanConeCos;
			if (bReached)
			{
				ReachedActors.Add(Actor);
				Iterator.RemoveCurrent();
			}
		}
	}

	for (const TWeakObjectPtr<AActor>& ReachedActor : ReachedActors)
	{
		if (!bScanActive)
		{
			break;
		}
		AActor* Actor = ReachedActor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		OnActorScanned.Broadcast(Actor);
	}
}

void UNPScanComponent::FinishScan()
{
	bScanActive = false;
	PendingScanActors.Reset();
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool UNPScanComponent::UpdateScanView()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;
	if (!IsValid(PlayerController))
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector CameraForward = CameraRotation.Vector().GetSafeNormal();
	if (CameraLocation.ContainsNaN() || CameraForward.ContainsNaN() || CameraForward.IsNearlyZero())
	{
		return false;
	}

	ScanOrigin = CameraLocation;
	ScanForward = CameraForward;
	SetWorldLocation(ScanOrigin);
	return true;
}

bool UNPScanComponent::IsActorInScanRange(const AActor* Actor) const
{
	if (!bScanActive || !IsValid(Actor) || Actor == GetOwner())
	{
		return false;
	}

	const FVector ToActor = Actor->GetActorLocation() - ScanOrigin;
	const double ForwardDistance = FVector::DotProduct(ToActor, ScanForward);
	if (ForwardDistance < 0.0 || ForwardDistance > CurrentScanDistance)
	{
		return false;
	}

	return ToActor.IsNearlyZero()
		|| FVector::DotProduct(ScanForward, ToActor.GetSafeNormal()) >= ScanConeCos;
}

void UNPScanComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bScanActive || !bDrawDebugRange || !GetWorld())
	{
		return;
	}
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	FVector AxisY;
	FVector AxisZ;
	ScanForward.FindBestAxisVectors(AxisY, AxisZ);
	const float AngleTangent = FMath::Sqrt(FMath::Max(0.0f, 1.0f - ScanConeCos * ScanConeCos)) / ScanConeCos;
	const int32 Segments = FMath::Clamp(DebugSegments, 4, 64);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float AngleA = 2.0f * PI * Index / Segments;
		const float AngleB = 2.0f * PI * (Index + 1) / Segments;
		const FVector EdgeA = ScanForward + (AxisY * FMath::Cos(AngleA) + AxisZ * FMath::Sin(AngleA)) * AngleTangent;
		const FVector EdgeB = ScanForward + (AxisY * FMath::Cos(AngleB) + AxisZ * FMath::Sin(AngleB)) * AngleTangent;
		DrawDebugLine(GetWorld(), ScanOrigin + EdgeA * MaxScanDistance,
			ScanOrigin + EdgeB * MaxScanDistance, FColor(0, 96, 128), false, 0.0f, 0, 0.5f);
		DrawDebugLine(GetWorld(), ScanOrigin, ScanOrigin + EdgeA * MaxScanDistance,
			FColor(0, 96, 128), false, 0.0f, 0, 0.5f);
		DrawDebugLine(GetWorld(), ScanOrigin + EdgeA * CurrentScanDistance,
			ScanOrigin + EdgeB * CurrentScanDistance, FColor::Cyan, false, 0.0f, 0, 2.0f);
	}
}
