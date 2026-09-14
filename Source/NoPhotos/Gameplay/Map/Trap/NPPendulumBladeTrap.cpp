#include "Gameplay/Map/Trap/NPPendulumBladeTrap.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Gameplay/Map/Trap/NPTrapKnockbackComponent.h"

ANPPendulumBladeTrap::ANPPendulumBladeTrap()
{
	OperationMode = ENPStairTrapOperationMode::Continuous;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PendulumPivotComponent = CreateDefaultSubobject<USceneComponent>(
		TEXT("PendulumPivotComponent"));
	PendulumPivotComponent->SetupAttachment(TrapRootComponent);

	BladeCollisionComponent = CreateDefaultSubobject<UBoxComponent>(
		TEXT("BladeCollisionComponent"));
	BladeCollisionComponent->SetupAttachment(PendulumPivotComponent);
	BladeCollisionComponent->SetBoxExtent(FVector(20.0f, 100.0f, 100.0f));
	BladeCollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	BladeCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BladeCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BladeCollisionComponent->SetCollisionResponseToChannel(
		ECC_PhysicsBody,
		ECR_Overlap);
	// 실제 피격은 서버의 명시적인 Box Query가 담당합니다.
	BladeCollisionComponent->SetGenerateOverlapEvents(false);
	BladeCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BladeMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("BladeMeshComponent"));
	BladeMeshComponent->SetupAttachment(BladeCollisionComponent);
	BladeMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	KnockbackDirectionArrow = CreateDefaultSubobject<UArrowComponent>(
		TEXT("KnockbackDirectionArrow"));
	KnockbackDirectionArrow->SetupAttachment(TrapRootComponent);
	KnockbackDirectionArrow->SetHiddenInGame(true);

	KnockbackComponent = CreateDefaultSubobject<UNPTrapKnockbackComponent>(
		TEXT("KnockbackComponent"));
	KnockbackComponent->SetHitOncePerActivation(false);
}

void ANPPendulumBladeTrap::BeginPlay()
{
	Super::BeginPlay();

	RestRelativeRotation = PendulumPivotComponent->GetRelativeRotation();
	ReturnStartRelativeRotation = RestRelativeRotation;
	UpdateBladePose();
}

void ANPPendulumBladeTrap::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBladePose();
}

void ANPPendulumBladeTrap::HandleTrapStateChanged(
	const ENPStairTrapState PreviousState,
	const ENPStairTrapState NewState)
{
	Super::HandleTrapStateChanged(PreviousState, NewState);

	if (NewState == ENPStairTrapState::Returning)
	{
		ReturnStartRelativeRotation =
			PendulumPivotComponent->GetRelativeRotation();
	}

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
	UpdateBladePose();
}

void ANPPendulumBladeTrap::UpdateBladePose()
{
	if (!IsValid(PendulumPivotComponent))
	{
		return;
	}

	FQuat TargetRotation = RestRelativeRotation.Quaternion();
	switch (GetTrapState())
	{
	case ENPStairTrapState::Active:
	{
		FVector SafeSwingAxis = LocalSwingAxis.GetSafeNormal();
		if (SafeSwingAxis.IsNearlyZero())
		{
			SafeSwingAxis = FVector::ForwardVector;
		}

		const float Period = FMath::Max(SwingPeriod, 0.05f);
		const float PhaseRadians =
			GetTrapPhaseElapsedTime() / Period * 2.0f * UE_PI
			+ FMath::DegreesToRadians(PhaseOffsetDegrees);
		const float AngleRadians = FMath::DegreesToRadians(
			FMath::Sin(PhaseRadians) * MaximumSwingAngle);
		TargetRotation = RestRelativeRotation.Quaternion()
			* FQuat(SafeSwingAxis, AngleRadians);
		break;
	}
	case ENPStairTrapState::Returning:
	{
		const float Alpha = FMath::Clamp(
			GetTrapPhaseElapsedTime()
				/ FMath::Max(ReturnToRestDuration, UE_SMALL_NUMBER),
			0.0f,
			1.0f);
		TargetRotation = FQuat::Slerp(
			ReturnStartRelativeRotation.Quaternion(),
			RestRelativeRotation.Quaternion(),
			Alpha);
		break;
	}
	case ENPStairTrapState::Cooldown:
	case ENPStairTrapState::Warning:
	case ENPStairTrapState::Idle:
	case ENPStairTrapState::Disabled:
	default:
		break;
	}

	const FVector SweepStart = IsValid(BladeCollisionComponent)
		? BladeCollisionComponent->GetComponentLocation()
		: FVector::ZeroVector;

	// 회전 컴포넌트 자체의 Collision Sweep/Block은 사용하지 않습니다.
	// 칼날의 피격 판정은 회전 적용 후 별도 서버 Query Sweep으로 처리합니다.
	PendulumPivotComponent->SetRelativeRotation(
		TargetRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (HasAuthority()
		&& GetTrapState() == ENPStairTrapState::Active
		&& IsValid(BladeCollisionComponent))
	{
		QueryBladeSweep(
			SweepStart,
			BladeCollisionComponent->GetComponentLocation());
	}
}

void ANPPendulumBladeTrap::QueryBladeSweep(
	const FVector& StartWorldLocation,
	const FVector& EndWorldLocation)
{
	UWorld* World = GetWorld();
	if (!HasAuthority()
		|| !World
		|| !IsValid(BladeCollisionComponent)
		|| !IsValid(KnockbackComponent)
		|| GetTrapState() != ENPStairTrapState::Active)
	{
		return;
	}

	const FVector BoxHalfExtent =
		BladeCollisionComponent->GetScaledBoxExtent();
	if (BoxHalfExtent.IsNearlyZero())
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(PendulumBladePlayerSweep),
		false,
		this);
	QueryParams.AddIgnoredActor(this);

	TArray<FHitResult> SweepHits;
	World->SweepMultiByObjectType(
		SweepHits,
		StartWorldLocation,
		EndWorldLocation,
		BladeCollisionComponent->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(BoxHalfExtent),
		QueryParams);

	for (const FHitResult& SweepHit : SweepHits)
	{
		TryKnockbackActor(SweepHit.GetActor(), SweepHit);
	}
}

void ANPPendulumBladeTrap::TryKnockbackActor(
	AActor* OtherActor,
	const FHitResult& Hit)
{
	if (!HasAuthority()
		|| GetTrapState() != ENPStairTrapState::Active
		|| !IsValid(KnockbackComponent)
		|| !IsValid(KnockbackDirectionArrow))
	{
		return;
	}

	KnockbackComponent->TryApplyKnockback(
		OtherActor,
		Hit,
		KnockbackDirectionArrow->GetForwardVector());
}

void ANPPendulumBladeTrap::SetDamageCollisionEnabled(const bool bEnabled)
{
	if (!IsValid(BladeCollisionComponent))
	{
		return;
	}

	BladeCollisionComponent->SetCollisionEnabled(
		bEnabled && HasAuthority()
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
}
