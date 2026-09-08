#include "Gameplay/Interaction/Components/NPImpactRotateComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UNPImpactRotateComponent::UNPImpactRotateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UNPImpactRotateComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	InitialActorRotation = Owner->GetActorRotation();
	if (bEnableOwnerMovementReplication && Owner->HasAuthority())
	{
		Owner->SetReplicates(true);
		Owner->SetReplicateMovement(true);
	}

	BindImpactTarget();
}

void UNPImpactRotateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindImpactTarget();
	Super::EndPlay(EndPlayReason);
}

void UNPImpactRotateComponent::SetImpactTargetComponent(
	UPrimitiveComponent* InTargetComponent)
{
	if (ImpactTargetComponent == InTargetComponent)
	{
		return;
	}

	UnbindImpactTarget();
	ImpactTargetComponent = InTargetComponent;
	if (HasBegunPlay())
	{
		BindImpactTarget();
	}
}

void UNPImpactRotateComponent::BindImpactTarget()
{
	AActor* Owner = GetOwner();
	if (!IsValid(ImpactTargetComponent))
	{
		ImpactTargetComponent = Owner
			? Cast<UPrimitiveComponent>(Owner->GetRootComponent())
			: nullptr;
	}

	if (!IsValid(ImpactTargetComponent))
	{
		return;
	}

	ImpactTargetComponent->SetNotifyRigidBodyCollision(true);
	ImpactTargetComponent->OnComponentHit.AddUniqueDynamic(
		this,
		&ThisClass::HandleTargetHit);
}

void UNPImpactRotateComponent::UnbindImpactTarget()
{
	if (IsValid(ImpactTargetComponent))
	{
		ImpactTargetComponent->OnComponentHit.RemoveDynamic(
			this,
			&ThisClass::HandleTargetHit);
	}
}

void UNPImpactRotateComponent::HandleTargetHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const FVector NormalImpulse,
	const FHitResult& Hit)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World
		|| OtherActor == Owner
		|| (OtherComponent && OtherComponent->GetOwner() == Owner))
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (CurrentTime < NextImpactAllowedTime)
	{
		return;
	}

	const float ImpactStrength = NormalImpulse.Size();
	const float ValidMinimumImpulse = FMath::Max(0.0f, MinimumImpactImpulse);
	const float ValidMaximumImpulse = FMath::Max(ValidMinimumImpulse, MaximumImpactImpulse);
	if (ImpactStrength < ValidMinimumImpulse)
	{
		return;
	}

	FVector ImpactDirection(NormalImpulse.X, NormalImpulse.Y, 0.0f);
	if (ImpactDirection.IsNearlyZero())
	{
		ImpactDirection = FVector(
			-Hit.ImpactNormal.X,
			-Hit.ImpactNormal.Y,
			0.0f);
	}
	if (ImpactDirection.IsNearlyZero() && IsValid(OtherActor))
	{
		ImpactDirection = Owner->GetActorLocation() - OtherActor->GetActorLocation();
		ImpactDirection.Z = 0.0f;
	}
	if (!ImpactDirection.Normalize())
	{
		return;
	}
	if (bInvertRotationDirection)
	{
		ImpactDirection *= -1.0f;
	}

	const float ImpactAlpha = ValidMaximumImpulse > ValidMinimumImpulse
		? FMath::Clamp(
			(ImpactStrength - ValidMinimumImpulse)
				/ (ValidMaximumImpulse - ValidMinimumImpulse),
			0.0f,
			1.0f)
		: 1.0f;
	const float MinimumDegrees = FMath::Max(0.0f, MinimumRotationPerImpact);
	const float MaximumDegrees = FMath::Max(MinimumDegrees, MaximumRotationPerImpact);
	const float RotationDegrees = FMath::Lerp(MinimumDegrees, MaximumDegrees, ImpactAlpha);

	// 액터의 로컬 우측 방향을 기준으로 오른쪽 힘은 +Yaw, 왼쪽 힘은 -Yaw로 적용합니다.
	const FVector InitialRight = InitialActorRotation.RotateVector(FVector::RightVector);
	const float RightAmount = FVector::DotProduct(ImpactDirection, InitialRight);
	const float ValidMaximumYawOffset = FMath::Max(0.0f, MaximumYawOffset);
	TargetYawOffset = FMath::Clamp(
		TargetYawOffset + RightAmount * RotationDegrees,
		-ValidMaximumYawOffset,
		ValidMaximumYawOffset);

	NextImpactAllowedTime = CurrentTime + FMath::Max(0.0f, ImpactCooldown);
	ReturnStartTime = CurrentTime + FMath::Max(0.0f, ReturnDelay);
	SetComponentTickEnabled(true);
	Owner->ForceNetUpdate();
}

void UNPImpactRotateComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const bool bReturning = bReturnToInitialRotation
		&& World->GetTimeSeconds() >= ReturnStartTime;
	if (bReturning)
	{
		TargetYawOffset = 0.0f;
	}

	const float InterpSpeed = bReturning
		? FMath::Max(0.0f, ReturnInterpSpeed)
		: FMath::Max(0.0f, RotationInterpSpeed);
	CurrentYawOffset = InterpSpeed > 0.0f
		? FMath::FInterpTo(CurrentYawOffset, TargetYawOffset, DeltaTime, InterpSpeed)
		: TargetYawOffset;

	FRotator NewRotation = InitialActorRotation;
	NewRotation.Yaw = InitialActorRotation.Yaw + CurrentYawOffset;
	Owner->SetActorRotation(NewRotation);

	if (FMath::IsNearlyEqual(CurrentYawOffset, TargetYawOffset, 0.05f))
	{
		CurrentYawOffset = TargetYawOffset;
		if (!bReturnToInitialRotation || bReturning)
		{
			Owner->ForceNetUpdate();
			SetComponentTickEnabled(false);
		}
	}
}
