#include "NPRoomGenerationHelper.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "SubSystem/Room/NPRoomGenerateSubsystem.h"

ANPRoomGenerationHelper::ANPRoomGenerationHelper()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TopRightSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("TopRightSlot"));
	TopRightSlot->SetupAttachment(SceneRoot);

	TopLeftSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("TopLeftSlot"));
	TopLeftSlot->SetupAttachment(SceneRoot);

	BottomRightSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("BottomRightSlot"));
	BottomRightSlot->SetupAttachment(SceneRoot);
	BottomRightSlot->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	BottomLeftSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("BottomLeftSlot"));
	BottomLeftSlot->SetupAttachment(SceneRoot);
	BottomLeftSlot->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
}

void ANPRoomGenerationHelper::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPRoomGenerationHelper, LayoutSeed);
}

void ANPRoomGenerationHelper::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		LayoutSeed = FMath::Max(FMath::Rand(), 1);
		RequestRoomGeneration();
	}
	else if (LayoutSeed != 0)
	{
		RequestRoomGeneration();
	}
}

void ANPRoomGenerationHelper::OnRep_LayoutSeed()
{
	RequestRoomGeneration();
}

void ANPRoomGenerationHelper::RequestRoomGeneration()
{
	if (LayoutSeed == 0 || Rooms.IsEmpty())
	{
		return;
	}

	const TArray<const USceneComponent*> Slots = {
		TopRightSlot.Get(),
		TopLeftSlot.Get(),
		BottomRightSlot.Get(),
		BottomLeftSlot.Get()
	};

	TArray<FTransform> SlotTransforms;
	SlotTransforms.Reserve(Slots.Num());
	for (const USceneComponent* Slot : Slots)
	{
		SlotTransforms.Emplace(
			Slot->GetComponentQuat(),
			Slot->GetComponentLocation());
	}

	if (UNPRoomGenerateSubsystem* RoomGenerateSubsystem =
		GetWorld()->GetSubsystem<UNPRoomGenerateSubsystem>())
	{
		RoomGenerateSubsystem->GenerateRooms(Rooms, SlotTransforms, LayoutSeed);
	}
}
