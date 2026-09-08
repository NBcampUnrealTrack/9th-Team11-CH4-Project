#include "Gameplay/Interaction/Components/NPPinataComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UNPPinataComponent::UNPPinataComponent()
{
	SetIsReplicatedByDefault(true);
}

void UNPPinataComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPPinataComponent, DamageStage);
	DOREPLIFETIME(UNPPinataComponent, bIsBroken);
	DOREPLIFETIME(UNPPinataComponent, ReplicatedImpactLocation);
}

void UNPPinataComponent::BeginPlay()
{
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		// 파괴와 컴포넌트 상태가 클라이언트에도 전달되도록 피냐타 소유 액터를 복제합니다.
		Owner->SetReplicates(true);
	}

	UPrimitiveComponent* ImpactTarget = Owner
		? Cast<UPrimitiveComponent>(Owner->GetRootComponent())
		: nullptr;
	if (Owner && !ImpactComponentTag.IsNone())
	{
		const TArray<UActorComponent*> TaggedComponents = Owner->GetComponentsByTag(
			UPrimitiveComponent::StaticClass(),
			ImpactComponentTag);
		if (!TaggedComponents.IsEmpty())
		{
			ImpactTarget = Cast<UPrimitiveComponent>(TaggedComponents[0]);
		}
	}

	if (IsValid(ImpactTarget))
	{
		// 에디터에서 별도로 Simulation Generates Hit Events를 켜지 않아도 충격을 받습니다.
		ImpactTarget->SetNotifyRigidBodyCollision(true);
		SetImpactTargetComponent(ImpactTarget);
	}

	Super::BeginPlay();
	OnDamaged.AddUObject(this, &ThisClass::HandleDurabilityDamaged);
	OnDepleted.AddUObject(this, &ThisClass::HandleDurabilityDepleted);
}

void UNPPinataComponent::HandleDurabilityDamaged(
	const int32,
	const int32 InCurrentHealth,
	const int32 InMaxHealth)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsBroken || InMaxHealth <= 0)
	{
		return;
	}

	const float HealthRatio = FMath::Clamp(
		static_cast<float>(InCurrentHealth) / static_cast<float>(InMaxHealth),
		0.0f,
		1.0f);
	const int32 NewDamageStage = CalculateDamageStage(HealthRatio);
	if (DamageStage == NewDamageStage)
	{
		return;
	}

	DamageStage = NewDamageStage;
	OnRep_DamageStage();
	Owner->ForceNetUpdate();
}

int32 UNPPinataComponent::CalculateDamageStage(const float HealthRatio) const
{
	int32 NewDamageStage = 0;
	for (const float Threshold : DamageStageHealthRatios)
	{
		if (FMath::IsFinite(Threshold)
			&& HealthRatio <= FMath::Clamp(Threshold, 0.0f, 1.0f))
		{
			++NewDamageStage;
		}
	}
	return NewDamageStage;
}

void UNPPinataComponent::HandleDurabilityDepleted(
	const FVector& ImpactLocation)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World || bIsBroken)
	{
		return;
	}

	bIsBroken = true;
	ReplicatedImpactLocation = ImpactLocation;
	SpawnConfiguredRelics();
	OnRep_IsBroken();
	Owner->ForceNetUpdate();

	if (!bDestroyOwnerAfterBreak)
	{
		return;
	}

	if (DestroyDelay <= 0.0f)
	{
		DestroyOwner();
		return;
	}

	World->GetTimerManager().SetTimer(
		DestroyTimer,
		this,
		&ThisClass::DestroyOwner,
		DestroyDelay,
		false);
}

void UNPPinataComponent::SpawnConfiguredRelics()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return;
	}

	const FVector Center = Owner->GetActorTransform().TransformPosition(SpawnOffset);
	const float ValidMinimumSpeed = FMath::Max(0.0f, MinimumLaunchSpeed);
	const float ValidMaximumSpeed = FMath::Max(ValidMinimumSpeed, MaximumLaunchSpeed);
	const float ValidSpawnRadius = FMath::Max(0.0f, SpawnRadius);

	for (const TSubclassOf<ANPBaseRelic>& RelicClass : RelicsToSpawn)
	{
		if (!RelicClass)
		{
			continue;
		}

		const FVector2D RandomOffset = FMath::RandPointInCircle(ValidSpawnRadius);
		const FVector SpawnLocation = Center + FVector(RandomOffset.X, RandomOffset.Y, 0.0f);
		const FRotator SpawnRotation(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Owner;
		SpawnParameters.Instigator = Owner->GetInstigator();
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ANPBaseRelic* SpawnedRelic = World->SpawnActor<ANPBaseRelic>(
			RelicClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParameters);
		if (!IsValid(SpawnedRelic))
		{
			continue;
		}

		FVector LaunchDirection(
			FMath::FRandRange(-1.0f, 1.0f),
			FMath::FRandRange(-1.0f, 1.0f),
			FMath::FRandRange(0.65f, 1.0f));
		LaunchDirection = LaunchDirection.GetSafeNormal(
			UE_SMALL_NUMBER,
			FVector::UpVector);
		SpawnedRelic->ReleaseWithVelocityImpulse(
			LaunchDirection * FMath::FRandRange(ValidMinimumSpeed, ValidMaximumSpeed));
	}
}

void UNPPinataComponent::OnRep_IsBroken()
{
	if (bIsBroken)
	{
		OnPinataBroken.Broadcast(ReplicatedImpactLocation);
	}
}

void UNPPinataComponent::OnRep_DamageStage()
{
	OnPinataDamageStageChanged.Broadcast(DamageStage);
}

void UNPPinataComponent::DestroyOwner()
{
	if (AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		Owner->Destroy();
	}
}
