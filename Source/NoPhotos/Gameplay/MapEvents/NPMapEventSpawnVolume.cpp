#include "NPMapEventSpawnVolume.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

ANPMapEventSpawnVolume::ANPMapEventSpawnVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SpawnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
	SetRootComponent(SpawnBounds);
	SpawnBounds->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
	SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnBounds->SetGenerateOverlapEvents(false);
	SpawnBounds->SetHiddenInGame(true);
}

bool ANPMapEventSpawnVolume::SupportsSpawnGroup(const FGameplayTag SpawnGroup) const
{
	return SpawnGroup.IsValid() && SupportedSpawnGroups.HasTagExact(SpawnGroup);
}

bool ANPMapEventSpawnVolume::FindRandomGroundTransform(
	FVector RequiredHalfExtent,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	LastSpawnFailureReason.Reset();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World
		? UNavigationSystemV1::GetCurrent(World)
		: nullptr;
	if (!HasAuthority() || !World || !NavigationSystem || !SpawnBounds)
	{
		LastSpawnFailureReason = TEXT("Server/World/NavigationSystem/SpawnBounds unavailable");
		return false;
	}
	if (!NavigationSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
	{
		LastSpawnFailureReason = TEXT("No NavData: persistent level NavMesh and runtime generation must be configured");
		return false;
	}

	RequiredHalfExtent.X = FMath::Max(1.0f, RequiredHalfExtent.X);
	RequiredHalfExtent.Y = FMath::Max(1.0f, RequiredHalfExtent.Y);
	RequiredHalfExtent.Z = FMath::Max(1.0f, RequiredHalfExtent.Z);

	const FVector LocalExtent = SpawnBounds->GetUnscaledBoxExtent();
	const FTransform BoundsTransform = SpawnBounds->GetComponentTransform();
	// A tall streamed volume can sample thousands of cm above its floor. Search its full height,
	// rather than rejecting a valid floor merely because the default Z projection is only 300 cm.
	FVector ProjectionExtent = NavigationProjectionExtent.GetAbs();
	ProjectionExtent.Z = FMath::Max(ProjectionExtent.Z, SpawnBounds->Bounds.BoxExtent.Z * 2.0);
	int32 ProjectionFailures = 0;
	int32 BoundsFailures = 0;
	int32 GroundFailures = 0;
	int32 ClearanceFailures = 0;
	int32 ReachabilityFailures = 0;
	const int32 Attempts = FMath::Max(1, MaximumSamplingAttempts);
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const FVector LocalSample(
			FMath::FRandRange(-LocalExtent.X, LocalExtent.X),
			FMath::FRandRange(-LocalExtent.Y, LocalExtent.Y),
			FMath::FRandRange(-LocalExtent.Z, LocalExtent.Z));
		const FVector WorldSample = BoundsTransform.TransformPosition(LocalSample);

		FNavLocation NavigationLocation;
		if (!NavigationSystem->ProjectPointToNavigation(
				WorldSample,
				NavigationLocation,
				ProjectionExtent))
		{
			++ProjectionFailures;
			continue;
		}
		if (!IsInsideSpawnBounds(NavigationLocation.Location))
		{
			++BoundsFailures;
			continue;
		}

		const float Yaw = bRandomizeYaw
			? FMath::FRandRange(-180.0f, 180.0f)
			: SpawnBounds->GetComponentRotation().Yaw;
		const FRotator SpawnRotation(0.0f, Yaw, 0.0f);
		FVector GroundLocation;
		if (!TryResolveGroundLocation(
				NavigationLocation.Location,
				RequiredHalfExtent,
				SpawnRotation,
				GroundLocation))
		{
			++GroundFailures;
			continue;
		}
		if (!HasRequiredClearance(GroundLocation, RequiredHalfExtent, SpawnRotation))
		{
			++ClearanceFailures;
			continue;
		}
		if (!IsReachableFromAnyPlayer(GroundLocation))
		{
			++ReachabilityFailures;
			continue;
		}

		OutTransform = FTransform(SpawnRotation, GroundLocation);
		return true;
	}

	LastSpawnFailureReason = FString::Printf(
		TEXT("Samples=%d NavProjection=%d OutsideBounds=%d Ground=%d Clearance=%d PlayerPath=%d"),
		Attempts, ProjectionFailures, BoundsFailures, GroundFailures, ClearanceFailures, ReachabilityFailures);
	return false;
}

bool ANPMapEventSpawnVolume::IsInsideSpawnBounds(const FVector& WorldLocation) const
{
	if (!SpawnBounds)
	{
		return false;
	}

	const FVector LocalLocation = SpawnBounds->GetComponentTransform()
		.InverseTransformPosition(WorldLocation);
	const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= Extent.X
		&& FMath::Abs(LocalLocation.Y) <= Extent.Y
		&& FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

bool ANPMapEventSpawnVolume::TraceGround(
	const FVector& TestLocation,
	FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MapEventSpawnGround), false, this);
	const FVector Start = TestLocation + FVector::UpVector * GroundTraceUpDistance;
	const FVector End = TestLocation - FVector::UpVector * GroundTraceDownDistance;
	if (!World->LineTraceSingleByChannel(
			OutHit,
			Start,
			End,
			ECC_WorldStatic,
			QueryParams))
	{
		return false;
	}

	const float MinimumFloorNormalZ = FMath::Cos(
		FMath::DegreesToRadians(FMath::Clamp(MaximumGroundSlopeDegrees, 0.0f, 89.0f)));
	return OutHit.ImpactNormal.Z >= MinimumFloorNormalZ;
}

bool ANPMapEventSpawnVolume::TryResolveGroundLocation(
	const FVector& NavigationLocation,
	const FVector& RequiredHalfExtent,
	const FRotator& Rotation,
	FVector& OutGroundLocation) const
{
	const FVector Forward = Rotation.RotateVector(FVector::ForwardVector);
	const FVector Right = Rotation.RotateVector(FVector::RightVector);
	const FVector ForwardOffset = Forward * RequiredHalfExtent.X;
	const FVector RightOffset = Right * RequiredHalfExtent.Y;
	const FVector TestLocations[] =
	{
		NavigationLocation,
		NavigationLocation + ForwardOffset + RightOffset,
		NavigationLocation + ForwardOffset - RightOffset,
		NavigationLocation - ForwardOffset + RightOffset,
		NavigationLocation - ForwardOffset - RightOffset
	};

	float MinimumGroundZ = TNumericLimits<float>::Max();
	float MaximumGroundZ = TNumericLimits<float>::Lowest();
	for (const FVector& TestLocation : TestLocations)
	{
		FHitResult GroundHit;
		if (!TraceGround(TestLocation, GroundHit)
			|| !IsInsideSpawnBounds(GroundHit.ImpactPoint))
		{
			return false;
		}

		MinimumGroundZ = FMath::Min(MinimumGroundZ, GroundHit.ImpactPoint.Z);
		MaximumGroundZ = FMath::Max(MaximumGroundZ, GroundHit.ImpactPoint.Z);
	}

	if (MaximumGroundZ - MinimumGroundZ > FMath::Max(0.0f, MaximumGroundHeightDifference))
	{
		return false;
	}

	OutGroundLocation = FVector(NavigationLocation.X, NavigationLocation.Y, MaximumGroundZ);
	return true;
}

bool ANPMapEventSpawnVolume::HasRequiredClearance(
	const FVector& GroundLocation,
	const FVector& RequiredHalfExtent,
	const FRotator& Rotation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector ClearanceCenter = GroundLocation
		+ FVector::UpVector * (RequiredHalfExtent.Z + 2.0f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MapEventSpawnClearance), false, this);
	return !World->OverlapBlockingTestByChannel(
		ClearanceCenter,
		Rotation.Quaternion(),
		ECC_WorldStatic,
		FCollisionShape::MakeBox(RequiredHalfExtent),
		QueryParams);
}

bool ANPMapEventSpawnVolume::IsReachableFromAnyPlayer(
	const FVector& GroundLocation) const
{
	if (!bRequireReachableFromPlayer)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	bool bFoundPlayerPawn = false;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!IsValid(PlayerPawn))
		{
			continue;
		}

		bFoundPlayerPawn = true;
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			World,
			PlayerPawn->GetActorLocation(),
			GroundLocation,
			const_cast<APawn*>(PlayerPawn));
		if (IsValid(Path) && Path->IsValid() && !Path->IsPartial())
		{
			return true;
		}
	}

	// 플레이어 Pawn이 아직 생성되지 않은 초기화 시점에는 NavMesh 투영 결과를 허용합니다.
	return !bFoundPlayerPawn;
}
