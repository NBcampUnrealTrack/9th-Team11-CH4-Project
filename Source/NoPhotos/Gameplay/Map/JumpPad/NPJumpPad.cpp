#include "NPJumpPad.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPJumpPad, Log, All);

ANPJumpPad::ANPJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);

	LaunchVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("LaunchVolume"));
	LaunchVolume->SetupAttachment(SceneRoot);
	LaunchVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	LaunchVolume->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	LaunchVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LaunchVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	LaunchVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	LaunchVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	LaunchVolume->SetGenerateOverlapEvents(true);
}

void ANPJumpPad::BeginPlay()
{
	Super::BeginPlay();
	LaunchVolume->OnComponentBeginOverlap.AddDynamic(this, &ANPJumpPad::HandleLaunchOverlap);
}

void ANPJumpPad::HandleLaunchOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!HasAuthority() || !IsValid(Pawn) || IsOnCooldown(Pawn))
	{
		return;
	}

	if (!LaunchPawn(Pawn))
	{
		return;
	}

	RecordLaunch(Pawn);
	UE_LOG(
		LogNPJumpPad,
		Log,
		TEXT("점프대 발사: Pawn=%s, Pad=%s, Strength=%.1f"),
		*GetNameSafe(Pawn),
		*GetName(),
		LaunchStrength);
}

bool ANPJumpPad::IsOnCooldown(const APawn* Pawn) const
{
	if (!IsValid(Pawn) || !GetWorld())
	{
		return true;
	}

	const TWeakObjectPtr<APawn> PawnKey(const_cast<APawn*>(Pawn));
	const double* LastLaunchTime = LastLaunchTimes.Find(PawnKey);
	return LastLaunchTime && GetWorld()->GetTimeSeconds() < *LastLaunchTime + Cooldown;
}

void ANPJumpPad::RecordLaunch(APawn* Pawn)
{
	if (IsValid(Pawn) && GetWorld())
	{
		const TWeakObjectPtr<APawn> PawnKey(Pawn);
		LastLaunchTimes.FindOrAdd(PawnKey) = GetWorld()->GetTimeSeconds();
	}
}

bool ANPJumpPad::LaunchPawn(APawn* Pawn) const
{
	if (!IsValid(Pawn) || LaunchStrength <= 0.0f)
	{
		return false;
	}

	if (ANPStablePhysicsPawn* StablePhysicsPawn = Cast<ANPStablePhysicsPawn>(Pawn))
	{
		StablePhysicsPawn->SetExternalVerticalVelocity(LaunchStrength);
		return true;
	}

	const FVector LaunchVelocity = FVector::UpVector * LaunchStrength;
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->LaunchCharacter(LaunchVelocity, false, true);
		return true;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
	{
		FVector RootVelocity = RootPrimitive->GetPhysicsLinearVelocity();
		RootVelocity.Z = LaunchStrength;
		RootPrimitive->SetPhysicsLinearVelocity(RootVelocity);
		return true;
	}

	return false;
}
