#include "Gameplay/Interaction/Ladder/NPLadderActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"

ANPLadderActor::ANPLadderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ClimbVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimbVolume"));
	ClimbVolume->SetupAttachment(SceneRoot);
	ClimbVolume->SetRelativeLocation(FVector(60.0f, 0.0f, 200.0f));
	ClimbVolume->SetBoxExtent(FVector(80.0f, 70.0f, 200.0f));
	ClimbVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClimbVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClimbVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	ClimbVolume->SetGenerateOverlapEvents(true);
}

void ANPLadderActor::BeginPlay()
{
	Super::BeginPlay();

	ClimbVolume->OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleClimbVolumeBeginOverlap);
	ClimbVolume->OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleClimbVolumeEndOverlap);
}

void ANPLadderActor::HandleClimbVolumeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	ANPStablePhysicsPawn* Pawn = Cast<ANPStablePhysicsPawn>(OtherActor);
	if (IsValid(Pawn) && OtherComponent == Pawn->GetRootComponent())
	{
		Pawn->SetLadderVolumeActive(true);
	}
}

void ANPLadderActor::HandleClimbVolumeEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	ANPStablePhysicsPawn* Pawn = Cast<ANPStablePhysicsPawn>(OtherActor);
	if (IsValid(Pawn) && OtherComponent == Pawn->GetRootComponent())
	{
		Pawn->SetLadderVolumeActive(false);
	}
}
