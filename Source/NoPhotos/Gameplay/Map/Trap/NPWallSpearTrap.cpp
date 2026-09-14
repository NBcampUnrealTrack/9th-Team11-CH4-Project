#include "Gameplay/Map/Trap/NPWallSpearTrap.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Gameplay/Map/Trap/NPTrapKnockbackComponent.h"

ANPWallSpearTrap::ANPWallSpearTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SpearCollisionComponent = CreateDefaultSubobject<UBoxComponent>(
		TEXT("SpearCollisionComponent"));
	SpearCollisionComponent->SetupAttachment(TrapRootComponent);
	SpearCollisionComponent->SetBoxExtent(FVector(100.0f, 20.0f, 20.0f));
	SpearCollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SpearCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpearCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SpearCollisionComponent->SetCollisionResponseToChannel(
		ECC_PhysicsBody,
		ECR_Block);
	SpearCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpearCollisionComponent->SetNotifyRigidBodyCollision(true);
	SpearCollisionComponent->OnComponentHit.AddDynamic(
		this,
		&ThisClass::HandleSpearHit);

	SpearMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("SpearMeshComponent"));
	SpearMeshComponent->SetupAttachment(SpearCollisionComponent);
	SpearMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	KnockbackComponent = CreateDefaultSubobject<UNPTrapKnockbackComponent>(
		TEXT("KnockbackComponent"));
}

void ANPWallSpearTrap::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	CacheInitialSpearConfiguration();
}

void ANPWallSpearTrap::BeginPlay()
{
	// 다른 Actor의 BeginPlay에서 먼저 Idle 상태를 전달하더라도
	// 원본 값은 PostInitializeComponents 단계에서 이미 보존되어 있습니다.
	CacheInitialSpearConfiguration();
	Super::BeginPlay();
	UpdateSpearPose();
}

void ANPWallSpearTrap::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSpearPose();
}

void ANPWallSpearTrap::HandleTrapStateChanged(
	const ENPStairTrapState PreviousState,
	const ENPStairTrapState NewState)
{
	Super::HandleTrapStateChanged(PreviousState, NewState);

	if (HasAuthority())
	{
		if (NewState == ENPStairTrapState::Active)
		{
			KnockbackComponent->BeginActivationCycle(
				GetTrapCycleSequence());
		}
		else
		{
			KnockbackComponent->EndActivationCycle();
		}
	}

	SetDamageCollisionEnabled(NewState == ENPStairTrapState::Active);
	SetActorTickEnabled(
		NewState == ENPStairTrapState::Active
		|| NewState == ENPStairTrapState::Returning);
	UpdateSpearPose();
}

void ANPWallSpearTrap::HandleSpearHit(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	FVector,
	const FHitResult& Hit)
{
	if (HasAuthority()
		&& GetTrapState() == ENPStairTrapState::Active
		&& IsValid(KnockbackComponent))
	{
		KnockbackComponent->TryApplyKnockback(
			OtherActor,
			Hit,
			GetActorForwardVector());
	}
}

void ANPWallSpearTrap::UpdateSpearPose()
{
	if (!IsValid(SpearCollisionComponent))
	{
		return;
	}

	float LengthAlpha = 0.0f;
	bool bSweepForPlayers = false;
	switch (GetTrapState())
	{
	case ENPStairTrapState::Active:
	{
		const float Alpha = FMath::Clamp(
			GetTrapPhaseElapsedTime()
				/ FMath::Max(ExtensionDuration, UE_SMALL_NUMBER),
			0.0f,
			1.0f);
		const float EasedAlpha = FMath::InterpEaseOut(
			0.0f,
			1.0f,
			Alpha,
			FMath::Max(1.0f, ExtensionEaseExponent));
		LengthAlpha = EasedAlpha;
		bSweepForPlayers = true;
		break;
	}
	case ENPStairTrapState::Returning:
	{
		const float Alpha = FMath::Clamp(
			GetTrapPhaseElapsedTime()
				/ FMath::Max(RetractionDuration, UE_SMALL_NUMBER),
			0.0f,
			1.0f);
		const float EasedAlpha = FMath::InterpEaseOut(
			0.0f,
			1.0f,
			Alpha,
			FMath::Max(1.0f, RetractionEaseExponent));
		LengthAlpha = 1.0f - EasedAlpha;
		break;
	}
	case ENPStairTrapState::Cooldown:
	case ENPStairTrapState::Warning:
	case ENPStairTrapState::Idle:
	case ENPStairTrapState::Disabled:
	default:
		break;
	}

	ApplySpearLengthAlpha(LengthAlpha);
	SetDamageCollisionEnabled(
		GetTrapState() == ENPStairTrapState::Active
		&& LengthAlpha > KINDA_SMALL_NUMBER);

	// Box 중심을 현재 길이의 절반만큼 이동해 뒤쪽 면은 밑동에 고정하고,
	// 앞쪽 면만 Local Forward 방향으로 뻗게 합니다.
	const float CurrentHalfLength =
		FMath::Max(0.0f, ExtensionDistance) * 0.5f * LengthAlpha;
	MoveSpearTo(
		SpearBaseRelativeLocation
			+ FVector::ForwardVector * CurrentHalfLength,
		bSweepForPlayers);
}

void ANPWallSpearTrap::CacheInitialSpearConfiguration()
{
	if (bInitialSpearConfigurationCached
		|| !IsValid(SpearCollisionComponent))
	{
		return;
	}

	SpearBaseRelativeLocation =
		SpearCollisionComponent->GetRelativeLocation();
	OriginalSpearCollisionExtent =
		SpearCollisionComponent->GetUnscaledBoxExtent();
	OriginalSpearMeshRelativeLocation = IsValid(SpearMeshComponent)
		? SpearMeshComponent->GetRelativeLocation()
		: FVector::ZeroVector;
	OriginalSpearMeshScale = IsValid(SpearMeshComponent)
		? SpearMeshComponent->GetRelativeScale3D()
		: FVector::OneVector;
	bInitialSpearConfigurationCached = true;
}

void ANPWallSpearTrap::ApplySpearLengthAlpha(const float LengthAlpha)
{
	const float SafeAlpha = FMath::Clamp(LengthAlpha, 0.0f, 1.0f);

	if (IsValid(SpearMeshComponent))
	{
		FVector MeshScale = OriginalSpearMeshScale;
		// 현재 창 에셋은 Mesh Local Z축이 길이 방향입니다.
		MeshScale.Z *= SafeAlpha;
		SpearMeshComponent->SetRelativeScale3D(MeshScale);

		// 부모 Box 중심 이동을 상쇄해 Mesh의 밑동 위치는 고정합니다.
		const float CurrentHalfLength =
			FMath::Max(0.0f, ExtensionDistance) * 0.5f * SafeAlpha;
		SpearMeshComponent->SetRelativeLocation(
			OriginalSpearMeshRelativeLocation
				- FVector::ForwardVector * CurrentHalfLength);
		SpearMeshComponent->SetVisibility(
			SafeAlpha > KINDA_SMALL_NUMBER,
			true);
	}

	if (IsValid(SpearCollisionComponent))
	{
		FVector CollisionExtent = OriginalSpearCollisionExtent;
		// Box의 Local X 반길이를 현재 창 길이의 절반으로 맞춥니다.
		CollisionExtent.X =
			FMath::Max(0.0f, ExtensionDistance) * 0.5f * SafeAlpha;
		SpearCollisionComponent->SetBoxExtent(
			CollisionExtent,
			false);
	}
}

void ANPWallSpearTrap::MoveSpearTo(
	const FVector& NewRelativeLocation,
	const bool bSweepForPlayers)
{
	const FVector StartWorldLocation =
		SpearCollisionComponent->GetComponentLocation();

	// 컴포넌트 이동 Sweep은 Blocking Collision의 물리 밀침을 만들 수 있으므로
	// 사용하지 않습니다. 플레이어 판정은 아래의 별도 Query Sweep이 담당합니다.
	SpearCollisionComponent->SetRelativeLocation(
		NewRelativeLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (bSweepForPlayers && HasAuthority())
	{
		QuerySpearSweep(
			StartWorldLocation,
			SpearCollisionComponent->GetComponentLocation());
	}
}

void ANPWallSpearTrap::QuerySpearSweep(
	const FVector& StartWorldLocation,
	const FVector& EndWorldLocation)
{
	UWorld* World = GetWorld();
	if (!HasAuthority()
		|| !World
		|| !IsValid(SpearCollisionComponent)
		|| !IsValid(KnockbackComponent)
		|| GetTrapState() != ENPStairTrapState::Active)
	{
		return;
	}

	const FVector BoxHalfExtent =
		SpearCollisionComponent->GetScaledBoxExtent();
	if (BoxHalfExtent.IsNearlyZero())
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(WallSpearPlayerSweep),
		false,
		this);
	QueryParams.AddIgnoredActor(this);

	TArray<FHitResult> SweepHits;
	World->SweepMultiByObjectType(
		SweepHits,
		StartWorldLocation,
		EndWorldLocation,
		SpearCollisionComponent->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(BoxHalfExtent),
		QueryParams);

	for (const FHitResult& SweepHit : SweepHits)
	{
		KnockbackComponent->TryApplyKnockback(
			SweepHit.GetActor(),
			SweepHit,
			GetActorForwardVector());
	}
}

void ANPWallSpearTrap::SetDamageCollisionEnabled(const bool bEnabled)
{
	if (!IsValid(SpearCollisionComponent))
	{
		return;
	}

	SpearCollisionComponent->SetCollisionEnabled(
		bEnabled && HasAuthority()
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
}
