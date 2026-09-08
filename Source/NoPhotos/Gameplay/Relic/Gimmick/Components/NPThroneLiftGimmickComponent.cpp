#include "Gameplay/Relic/Gimmick/Components/NPThroneLiftGimmickComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Gameplay/Relic/Gimmick/NPWallLever.h"
#include "Gameplay/Relic/Gimmick/NPThroneLiftCameraShake.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"

UNPThroneLiftGimmickComponent::UNPThroneLiftGimmickComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	RequiredLevers.SetNum(4);
	Relics.SetNum(2);
	LiftCameraShakeClass = UNPThroneLiftCameraShake::StaticClass();
}

void UNPThroneLiftGimmickComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	SetOwnerSceneComponentsMovable();
	if (Owner->HasAuthority())
	{
		Owner->SetReplicates(true);
		Owner->SetReplicateMovement(false);
		AttachRelic();
		for (ANPWallLever* Lever : RequiredLevers)
		{
			if (IsValid(Lever))
			{
				Lever->OnLeverActivated.AddUniqueDynamic(
					this, &UNPThroneLiftGimmickComponent::RaiseChair);
			}
		}
		RaiseChair();
	}
}

void UNPThroneLiftGimmickComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ANPWallLever* Lever : RequiredLevers)
	{
		if (IsValid(Lever))
		{
			Lever->OnLeverActivated.RemoveDynamic(
				this, &UNPThroneLiftGimmickComponent::RaiseChair);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UNPThroneLiftGimmickComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsLifting)
	{
		return;
	}

	const float Duration = FMath::Max(LiftDuration, UE_SMALL_NUMBER);
	const float Alpha = FMath::Clamp(
		(GetSynchronizedWorldTime() - LiftStartTime) / Duration,
		0.0f,
		1.0f);
	ApplyLift(Alpha);

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Alpha < 1.0f)
	{
		return;
	}

	bIsLifting = false;
	bIsRaised = true;
	DetachRelic();
	CompleteGimmick();
	Owner->ForceNetUpdate();
}

void UNPThroneLiftGimmickComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPThroneLiftGimmickComponent, LiftStartLocation);
	DOREPLIFETIME(UNPThroneLiftGimmickComponent, LiftEndLocation);
	DOREPLIFETIME(UNPThroneLiftGimmickComponent, LiftStartTime);
	DOREPLIFETIME(UNPThroneLiftGimmickComponent, bIsLifting);
	DOREPLIFETIME(UNPThroneLiftGimmickComponent, bIsRaised);
}

void UNPThroneLiftGimmickComponent::RaiseChair()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsLifting || bIsRaised)
	{
		return;
	}

	if (RequiredLevers.Num() != 4)
	{
		return;
	}

	TSet<const ANPWallLever*> ActivatedLevers;
	for (const ANPWallLever* Lever : RequiredLevers)
	{
		if (!IsValid(Lever) || !Lever->IsActivated())
		{
			return;
		}
		ActivatedLevers.Add(Lever);
	}
	if (ActivatedLevers.Num() != 4)
	{
		return;
	}

	SetOwnerSceneComponentsMovable();
	LiftStartLocation = Owner->GetActorLocation();
	LiftEndLocation = LiftStartLocation;
	LiftEndLocation.Z += RaisedRootZ - LoweredRootZ;
	LiftStartTime = GetSynchronizedWorldTime();
	bIsLifting = true;
	ApplyLift(0.0f);
	MulticastPlayLiftCameraShake();
	Owner->ForceNetUpdate();
}

void UNPThroneLiftGimmickComponent::MulticastPlayLiftCameraShake_Implementation()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !LiftCameraShakeClass)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Epicenter = Owner->GetActorLocation();
	const float InnerRadius = FMath::Max(CameraShakeInnerRadius, 0.0f);
	const float OuterRadius = FMath::Max(CameraShakeOuterRadius, InnerRadius);
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!PlayerController
			|| !PlayerController->IsLocalController()
			|| !PlayerController->PlayerCameraManager)
		{
			continue;
		}

		const float Distance = FVector::Distance(
			PlayerController->PlayerCameraManager->GetCameraLocation(),
			Epicenter);
		if (Distance > OuterRadius)
		{
			continue;
		}

		float DistanceScale = 1.0f;
		if (Distance > InnerRadius && OuterRadius > InnerRadius)
		{
			const float DistanceAlpha = 1.0f
				- (Distance - InnerRadius) / (OuterRadius - InnerRadius);
			DistanceScale = FMath::Pow(
				FMath::Clamp(DistanceAlpha, 0.0f, 1.0f),
				FMath::Max(CameraShakeFalloff, 0.0f));
		}

		PlayerController->PlayerCameraManager->StartCameraShake(
			LiftCameraShakeClass,
			FMath::Max(CameraShakeScale, 0.0f) * DistanceScale);
	}
}

void UNPThroneLiftGimmickComponent::OnRep_LiftState()
{
	SetOwnerSceneComponentsMovable();
	if (bIsRaised)
	{
		ApplyLift(1.0f);
		return;
	}

	if (!bIsLifting)
	{
		return;
	}

	const float Duration = FMath::Max(LiftDuration, UE_SMALL_NUMBER);
	ApplyLift(FMath::Clamp(
		(GetSynchronizedWorldTime() - LiftStartTime) / Duration,
		0.0f,
		1.0f));
}

void UNPThroneLiftGimmickComponent::AttachRelic()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	for (ANPBaseRelic* Relic : Relics)
	{
		if (IsValid(Relic))
		{
			Relic->AttachToActor(
				Owner,
				FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void UNPThroneLiftGimmickComponent::DetachRelic()
{
	AActor* Owner = GetOwner();
	for (ANPBaseRelic* Relic : Relics)
	{
		if (IsValid(Relic) && Relic->GetAttachParentActor() == Owner)
		{
			Relic->DetachFromActor(
				FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

void UNPThroneLiftGimmickComponent::SetOwnerSceneComponentsMovable() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent)
		{
			SceneComponent->SetMobility(EComponentMobility::Movable);
		}
	}
}

void UNPThroneLiftGimmickComponent::ApplyLift(float Alpha) const
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		Owner->SetActorLocation(
			FMath::Lerp(LiftStartLocation, LiftEndLocation, Alpha),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
}

float UNPThroneLiftGimmickComponent::GetSynchronizedWorldTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = World->GetGameState();
	return GameState
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
}
