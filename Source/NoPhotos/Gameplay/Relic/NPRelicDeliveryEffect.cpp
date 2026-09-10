#include "Gameplay/Relic/NPRelicDeliveryEffect.h"

#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ANPRelicDeliveryEffect::ANPRelicDeliveryEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(
		TEXT("NiagaraComponent"));
	SetRootComponent(NiagaraComponent);
	NiagaraComponent->SetAutoActivate(false);
}

void ANPRelicDeliveryEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTargetLocation();
}

void ANPRelicDeliveryEffect::InitializeEffect(
	UStaticMesh* RelicMesh,
	AActor* InTargetActor,
	const int32 RelicPrice)
{
	if (!IsValid(RelicMesh) || !NiagaraComponent->GetAsset())
	{
		Destroy();
		return;
	}

	TargetActor = InTargetActor;
	TargetMesh = IsValid(TargetActor)
		? TargetActor->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMesh(
		NiagaraComponent,
		TEXT("User.Static Mesh"),
		RelicMesh);
	NiagaraComponent->SetVariableFloat(
		TEXT("User.EmitterScale"),
		EmitterScale);
	const double SafeMaximumRelicPrice = FMath::Max(
		static_cast<double>(MaximumRelicPrice),
		static_cast<double>(MinimumRelicPrice) + 1.0);
	const double SafeMaximumCoinMultifly = FMath::Max(
		static_cast<double>(MaximumCoinMultifly),
		1.0);
	const float CoinMultifly = static_cast<float>(
		FMath::GetMappedRangeValueClamped(
			FVector2D(MinimumRelicPrice, SafeMaximumRelicPrice),
			FVector2D(1.0, SafeMaximumCoinMultifly),
			static_cast<double>(RelicPrice)));
	NiagaraComponent->SetVariableFloat(
		TEXT("User.Coin Multifly"),
		CoinMultifly);
	UpdateTargetLocation();
	SetActorTickEnabled(IsValid(TargetActor));
	NiagaraComponent->OnSystemFinished.AddUniqueDynamic(
		this,
		&ThisClass::HandleSystemFinished);
	NiagaraComponent->ReinitializeSystem();
}

void ANPRelicDeliveryEffect::UpdateTargetLocation()
{
	if (!IsValid(TargetActor))
	{
		SetActorTickEnabled(false);
		return;
	}

	NiagaraComponent->SetVariablePosition(
		TEXT("User.Move Location"),
		IsValid(TargetMesh)
			? TargetMesh->Bounds.Origin
			: TargetActor->GetActorLocation());
}

void ANPRelicDeliveryEffect::HandleSystemFinished(UNiagaraComponent*)
{
	Destroy();
}
