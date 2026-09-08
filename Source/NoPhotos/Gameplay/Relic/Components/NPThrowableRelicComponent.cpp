#include "Gameplay/Relic/Components/NPThrowableRelicComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Abilities/NPThrowableRelicUseAbility.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPThrowableRelic, Log, All);

UNPThrowableRelicComponent::UNPThrowableRelicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	SetUseAbilityClass(UNPThrowableRelicUseAbility::StaticClass());
}

bool UNPThrowableRelicComponent::CanThrow(
	const ANPStablePhysicsPawn* ThrowerPawn) const
{
	const ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner());
	const UWorld* World = GetWorld();
	const UGrabbableComponent* Grabbable = Relic
		? Relic->FindComponentByClass<UGrabbableComponent>()
		: nullptr;
	const UNPStablePhysicsGrabComponent* HandGrab = ThrowerPawn
		? ThrowerPawn->GetRightHandGrabComponent()
		: nullptr;
	return IsValid(Relic)
		&& !Relic->IsReturned()
		&& IsValid(World)
		&& IsValid(ThrowerPawn)
		&& IsValid(Grabbable)
		&& Grabbable->GetActiveGrabCount() == 1
		&& IsValid(HandGrab)
		&& HandGrab->GetGrabbedComponent()
		&& HandGrab->GetGrabbedComponent()->GetOwner() == Relic
		&& World->GetTimeSeconds() >= NextThrowAllowedTime;
}

bool UNPThrowableRelicComponent::TryThrow(ANPStablePhysicsPawn* ThrowerPawn)
{
	ANPBaseRelic* Relic = Cast<ANPBaseRelic>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(Relic)
		|| !Relic->HasAuthority()
		|| !IsValid(World)
		|| !CanThrow(ThrowerPawn))
	{
		return false;
	}

	UPrimitiveComponent* RelicMesh = ResolveRelicMesh();
	UGrabbableComponent* Grabbable =
		Relic->FindComponentByClass<UGrabbableComponent>();
	if (!IsValid(RelicMesh) || !IsValid(Grabbable))
	{
		return false;
	}

	const FVector ThrowVelocity = CalculateThrowVelocity(
		ThrowerPawn->GetViewForwardDirection(),
		ThrowerPawn->GetVelocity(),
		ThrowSettings);
	const FVector AngularVelocity = CalculateAngularVelocityDegrees(
		RelicMesh->GetComponentTransform(),
		ThrowSettings);
	if (ThrowVelocity.IsNearlyZero())
	{
		return false;
	}

	RestorePawnCollision();
	Grabbable->ForceReleaseAllGrabs();
	if (Grabbable->GetActiveGrabCount() != 0
		|| !Relic->ReleaseWithVelocityImpulse(FVector::ZeroVector)
		|| !RelicMesh->IsSimulatingPhysics())
	{
		UE_LOG(LogNPThrowableRelic, Warning,
			TEXT("투척 실패: 그랩 해제 또는 물리 활성화를 확인하세요. Relic=%s Grabs=%d Simulating=%s"),
			*GetNameSafe(Relic), Grabbable->GetActiveGrabCount(),
			RelicMesh->IsSimulatingPhysics() ? TEXT("true") : TEXT("false"));
		return false;
	}

	Relic->SetInstigator(ThrowerPawn);
	RelicMesh->WakeAllRigidBodies();
	RelicMesh->SetPhysicsLinearVelocity(ThrowVelocity, false);
	RelicMesh->SetPhysicsAngularVelocityInDegrees(AngularVelocity, false);
	NextThrowAllowedTime = World->GetTimeSeconds()
		+ (FMath::IsFinite(ThrowSettings.Cooldown)
			? FMath::Max(0.0f, ThrowSettings.Cooldown)
			: 0.0f);

	const float GraceTime = FMath::IsFinite(ThrowSettings.PawnCollisionGraceTime)
		? FMath::Max(0.0f, ThrowSettings.PawnCollisionGraceTime)
		: 0.0f;
	if (GraceTime > UE_SMALL_NUMBER)
	{
		MulticastBeginPawnCollisionGrace(GraceTime);
	}

	Relic->ForceNetUpdate();
	UE_LOG(LogNPThrowableRelic, Display,
		TEXT("유물 투척: Relic=%s Thrower=%s Velocity=%s AngularDeg=%s"),
		*GetNameSafe(Relic), *GetNameSafe(ThrowerPawn),
		*ThrowVelocity.ToCompactString(), *AngularVelocity.ToCompactString());
	return true;
}

FVector UNPThrowableRelicComponent::CalculateThrowVelocity(
	const FVector& ForwardDirection,
	const FVector& ThrowerVelocity,
	const FNPRelicThrowSettings& Settings)
{
	FVector Direction(ForwardDirection.X, ForwardDirection.Y, 0.0f);
	Direction.Normalize();
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}
	const FVector InheritedVelocity = Settings.bInheritThrowerVelocity
		&& !ThrowerVelocity.ContainsNaN()
		? ThrowerVelocity
		: FVector::ZeroVector;
	const float ForwardSpeed = FMath::IsFinite(Settings.ForwardSpeed)
		? FMath::Max(0.0f, Settings.ForwardSpeed) : 0.0f;
	const float UpwardSpeed = FMath::IsFinite(Settings.UpwardSpeed)
		? FMath::Max(0.0f, Settings.UpwardSpeed) : 0.0f;
	return Direction * ForwardSpeed
		+ FVector::UpVector * UpwardSpeed
		+ InheritedVelocity;
}

FVector UNPThrowableRelicComponent::CalculateAngularVelocityDegrees(
	const FTransform& RelicTransform,
	const FNPRelicThrowSettings& Settings)
{
	FVector LocalAxis = Settings.LocalSpinAxis.GetSafeNormal();
	if (LocalAxis.IsNearlyZero())
	{
		LocalAxis = FVector::YAxisVector;
	}
	const float SpinSpeed = FMath::IsFinite(Settings.SpinSpeed)
		? FMath::Max(0.0f, Settings.SpinSpeed) : 0.0f;
	return RelicTransform.TransformVectorNoScale(LocalAxis).GetSafeNormal()
		* SpinSpeed;
}

UPrimitiveComponent* UNPThrowableRelicComponent::ResolveRelicMesh() const
{
	return GetOwner()
		? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent())
		: nullptr;
}

void UNPThrowableRelicComponent::MulticastBeginPawnCollisionGrace_Implementation(
	const float Duration)
{
	UWorld* World = GetWorld();
	UPrimitiveComponent* Mesh = ResolveRelicMesh();
	if (!IsValid(World) || !IsValid(Mesh))
	{
		return;
	}
	RestorePawnCollision();
	CollisionGraceMesh = Mesh;
	PreviousPawnCollisionResponse = Mesh->GetCollisionResponseToChannel(ECC_Pawn);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	World->GetTimerManager().SetTimer(
		CollisionGraceTimer,
		this,
		&ThisClass::RestorePawnCollision,
		FMath::Max(Duration, 0.01f),
		false);
}

void UNPThrowableRelicComponent::RestorePawnCollision()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollisionGraceTimer);
	}
	if (UPrimitiveComponent* Mesh = CollisionGraceMesh.Get())
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnCollisionResponse);
	}
	CollisionGraceMesh.Reset();
}

void UNPThrowableRelicComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	RestorePawnCollision();
	Super::EndPlay(EndPlayReason);
}
