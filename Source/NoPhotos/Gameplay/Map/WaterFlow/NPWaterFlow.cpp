#include "Gameplay/Map/WaterFlow/NPWaterFlow.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"

ANPWaterFlow::ANPWaterFlow()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetupAttachment(SceneRoot);

	FlowVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("FlowVolume"));
	FlowVolume->SetupAttachment(SceneRoot);
	FlowVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
	FlowVolume->SetBoxExtent(FVector(100.0f, 100.0f, 15.0f));
	FlowVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FlowVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	FlowVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	FlowVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	FlowVolume->SetGenerateOverlapEvents(true);

	FlowDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("FlowDirection"));
	FlowDirection->SetupAttachment(SceneRoot);
	FlowDirection->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
	FlowDirection->ArrowSize = 0.75f;
}

void ANPWaterFlow::BeginPlay()
{
	Super::BeginPlay();

	FlowVolume->OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleFlowVolumeBeginOverlap);
	FlowVolume->OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleFlowVolumeEndOverlap);

	TArray<AActor*> OverlappingActors;
	FlowVolume->GetOverlappingActors(OverlappingActors, ANPStablePhysicsPawn::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		AddAffectedPawn(Cast<ANPStablePhysicsPawn>(OverlappingActor));
	}
}

void ANPWaterFlow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<ANPStablePhysicsPawn>& PawnPtr : AffectedPawns)
	{
		if (ANPStablePhysicsPawn* Pawn = PawnPtr.Get())
		{
			if (UNPStablePhysicsMovementComponent* Movement = Pawn->GetStablePhysicsMovementComponent())
			{
				Movement->ClearExternalFlowVelocity(this);
			}
		}
	}
	AffectedPawns.Reset();

	Super::EndPlay(EndPlayReason);
}

void ANPWaterFlow::HandleFlowVolumeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AddAffectedPawn(Cast<ANPStablePhysicsPawn>(OtherActor));
}

void ANPWaterFlow::HandleFlowVolumeEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	RemoveAffectedPawn(Cast<ANPStablePhysicsPawn>(OtherActor));
}

void ANPWaterFlow::AddAffectedPawn(ANPStablePhysicsPawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return;
	}

	if (UNPStablePhysicsMovementComponent* Movement = Pawn->GetStablePhysicsMovementComponent())
	{
		Movement->SetExternalFlowVelocity(this, GetFlowVelocity());
		AffectedPawns.Add(Pawn);
	}
}

void ANPWaterFlow::RemoveAffectedPawn(ANPStablePhysicsPawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return;
	}

	if (UNPStablePhysicsMovementComponent* Movement = Pawn->GetStablePhysicsMovementComponent())
	{
		Movement->ClearExternalFlowVelocity(this);
	}
	AffectedPawns.Remove(Pawn);
}

FVector ANPWaterFlow::GetFlowVelocity() const
{
	FVector FlowDirectionVector = IsValid(FlowDirection)
		? FlowDirection->GetForwardVector()
		: GetActorForwardVector();
	FlowDirectionVector.Z = 0.0f;
	return FlowDirectionVector.GetSafeNormal() * FlowSpeed;
}
