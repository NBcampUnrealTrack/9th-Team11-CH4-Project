#include "Gameplay/Relic/Components/NPScanOutlineComponent.h"

#include "Components/MeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

UNPScanOutlineComponent::UNPScanOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UNPScanOutlineComponent::PlayOutline()
{
	UMeshComponent* Mesh = ResolveTargetMesh();
	if (!IsValid(Mesh) || !IsValid(OverlayMaterial) || !IsValid(ExpansionCurve))
	{
		return;
	}

	if (!IsValid(OverlayMaterialInstance))
	{
		OverlayMaterialInstance = UMaterialInstanceDynamic::Create(
			OverlayMaterial,
			this);
	}
	if (!IsValid(OverlayMaterialInstance))
	{
		return;
	}

	ElapsedTime = 0.0f;
	SetExpansionRatio(
		FMath::Max(0.0f, ExpansionCurve->GetFloatValue(0.0f))
		* MaxExpansionRatio);
	Mesh->SetOverlayMaterial(OverlayMaterialInstance);
	SetComponentTickEnabled(true);
}

void UNPScanOutlineComponent::StopOutline()
{
	SetComponentTickEnabled(false);
	ElapsedTime = 0.0f;
	SetExpansionRatio(0.0f);

	if (UMeshComponent* Mesh = ResolveTargetMesh())
	{
		Mesh->SetOverlayMaterial(nullptr);
	}
}

void UNPScanOutlineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopOutline();
	Super::EndPlay(EndPlayReason);
}

void UNPScanOutlineComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(OverlayMaterialInstance) || !IsValid(ExpansionCurve))
	{
		StopOutline();
		return;
	}

	ElapsedTime += DeltaTime;
	const float Duration = FMath::Max(0.01f, AnimationDuration);
	const float NormalizedTime = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	const float CurveValue = FMath::Max(
		0.0f,
		ExpansionCurve->GetFloatValue(NormalizedTime));
	SetExpansionRatio(CurveValue * MaxExpansionRatio);

	if (NormalizedTime >= 1.0f)
	{
		StopOutline();
	}
}

UMeshComponent* UNPScanOutlineComponent::ResolveTargetMesh()
{
	if (TargetMesh.IsValid())
	{
		return TargetMesh.Get();
	}

	AActor* Owner = GetOwner();
	UMeshComponent* Mesh = Owner
		? Cast<UMeshComponent>(Owner->GetRootComponent())
		: nullptr;
	TargetMesh = Mesh;
	return Mesh;
}

void UNPScanOutlineComponent::SetExpansionRatio(const float Ratio)
{
	if (IsValid(OverlayMaterialInstance))
	{
		OverlayMaterialInstance->SetScalarParameterValue(
			TEXT("ExpansionRatio"),
			Ratio);
	}
}
