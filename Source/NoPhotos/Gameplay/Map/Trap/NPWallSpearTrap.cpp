#include "Gameplay/Map/Trap/NPWallSpearTrap.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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

void ANPWallSpearTrap::BeginPlay()
{
	Super::BeginPlay();

	RetractedRelativeLocation =
		SpearCollisionComponent->GetRelativeLocation();
	ExtendedRelativeLocation = RetractedRelativeLocation
		+ FVector::ForwardVector * FMath::Max(0.0f, ExtensionDistance);
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

	FVector TargetLocation = RetractedRelativeLocation;
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
		TargetLocation = FMath::Lerp(
			RetractedRelativeLocation,
			ExtendedRelativeLocation,
			Alpha);
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
		TargetLocation = FMath::Lerp(
			ExtendedRelativeLocation,
			RetractedRelativeLocation,
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

	MoveSpearTo(TargetLocation, bSweepForPlayers);
}

void ANPWallSpearTrap::MoveSpearTo(
	const FVector& NewRelativeLocation,
	const bool bSweepForPlayers)
{
	FHitResult SweepHit;
	SpearCollisionComponent->SetRelativeLocation(
		NewRelativeLocation,
		bSweepForPlayers && HasAuthority(),
		&SweepHit,
		ETeleportType::None);

	if (HasAuthority()
		&& SweepHit.bBlockingHit
		&& IsValid(KnockbackComponent))
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
