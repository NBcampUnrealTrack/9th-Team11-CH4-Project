#include "NPGoblinPresentationDoor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ANPGoblinPresentationDoor::ANPGoblinPresentationDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	SetRootComponent(DoorRoot);
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(DoorRoot);
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	Interior = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Interior"));
	Interior->SetupAttachment(DoorRoot);
	LeftFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFrame"));
	LeftFrame->SetupAttachment(DoorRoot);
	RightFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFrame"));
	RightFrame->SetupAttachment(DoorRoot);
	TopFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopFrame"));
	TopFrame->SetupAttachment(DoorRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorAsset(
		TEXT("/Game/LevelPrototyping/Interactable/Door/Meshes/SM_Door.SM_Door"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BlackMaterial(
		TEXT("/Engine/EngineDebugMaterials/BlackUnlitMaterial.BlackUnlitMaterial"));
	DoorMesh->SetStaticMesh(DoorAsset.Object);
	for (UStaticMeshComponent* Part : { Interior.Get(), LeftFrame.Get(), RightFrame.Get(), TopFrame.Get() })
	{
		Part->SetStaticMesh(CubeAsset.Object);
	}
	if (BlackMaterial.Succeeded())
	{
		Interior->SetMaterial(0, BlackMaterial.Object);
	}
	for (UStaticMeshComponent* Part : { DoorMesh.Get(), Interior.Get(), LeftFrame.Get(), RightFrame.Get(), TopFrame.Get() })
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetMobility(EComponentMobility::Movable);
	}
}

void ANPGoblinPresentationDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const float Width = FMath::Max(50.0f, OpeningWidth);
	const float Height = FMath::Max(50.0f, OpeningHeight);
	const float Thickness = FMath::Max(1.0f, FrameThickness);
	Hinge->SetRelativeLocation(FVector(0.0f, -Width * 0.5f, 0.0f));
	LeftFrame->SetRelativeLocation(FVector(0.0f, -(Width + Thickness) * 0.5f, Height * 0.5f));
	RightFrame->SetRelativeLocation(FVector(0.0f, (Width + Thickness) * 0.5f, Height * 0.5f));
	LeftFrame->SetRelativeScale3D(FVector(Thickness, Thickness, Height) / 100.0f);
	RightFrame->SetRelativeScale3D(LeftFrame->GetRelativeScale3D());
	TopFrame->SetRelativeLocation(FVector(0.0f, 0.0f, Height + Thickness * 0.5f));
	TopFrame->SetRelativeScale3D(FVector(Thickness, Width + Thickness * 2.0f, Thickness) / 100.0f);
	Interior->SetRelativeLocation(FVector(-Thickness, 0.0f, Height * 0.5f));
	Interior->SetRelativeScale3D(FVector(2.0f, Width, Height) / 100.0f);

	if (bAutoFitDoorMesh && DoorMesh->GetStaticMesh())
	{
		const FBox Bounds = DoorMesh->GetStaticMesh()->GetBoundingBox();
		const FVector Size = Bounds.GetSize().ComponentMax(FVector(0.01f));
		const bool bRotate = Size.Y < Size.X;
		const FRotator Rotation(0.0f, bRotate ? 90.0f : 0.0f, 0.0f);
		const FVector Scale = bRotate
			? FVector(Width / Size.X, 5.0f / Size.Y, Height / Size.Z)
			: FVector(5.0f / Size.X, Width / Size.Y, Height / Size.Z);
		const FVector Center = Rotation.RotateVector(Bounds.GetCenter() * Scale);
		DoorMesh->SetRelativeTransform(FTransform(Rotation,
			FVector(0.0f, Width * 0.5f, Height * 0.5f) - Center, Scale));
	}
}

void ANPGoblinPresentationDoor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		AnimationStartTime = GetPresentationTime();
		SetLifeSpan(30.0f); // Cleanup even if the owning event is unexpectedly interrupted.
		ForceNetUpdate();
	}
}

double ANPGoblinPresentationDoor::GetPresentationTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

float ANPGoblinPresentationDoor::GetOpenAlpha() const
{
	const float Duration = bClosing ? GetCloseDuration() : GetOpenDuration();
	const float Progress = FMath::Clamp(static_cast<float>((GetPresentationTime() - AnimationStartTime) / Duration), 0.0f, 1.0f);
	const float SmoothProgress = Progress * Progress * (3.0f - 2.0f * Progress);
	return FMath::Lerp(StartOpenAlpha, bClosing ? 0.0f : 1.0f, SmoothProgress);
}

void ANPGoblinPresentationDoor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Hinge->SetRelativeRotation(FRotator(0.0f, OpenAngle * GetOpenAlpha(), 0.0f));
}

void ANPGoblinPresentationDoor::CloseAndDestroy()
{
	if (!HasAuthority() || bClosing)
	{
		return;
	}
	StartOpenAlpha = GetOpenAlpha();
	bClosing = true;
	AnimationStartTime = GetPresentationTime();
	SetLifeSpan(GetCloseDuration() + 0.25f);
	ForceNetUpdate();
}

void ANPGoblinPresentationDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, AnimationStartTime);
	DOREPLIFETIME(ThisClass, StartOpenAlpha);
	DOREPLIFETIME(ThisClass, bClosing);
}
