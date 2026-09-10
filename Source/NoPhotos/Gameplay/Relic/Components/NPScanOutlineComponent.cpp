#include "Gameplay/Relic/Components/NPScanOutlineComponent.h"

#include "Components/MeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"

UNPScanOutlineComponent::UNPScanOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UNPScanOutlineComponent::PlayOutline()
{
	UMeshComponent* Mesh = ResolveTargetMesh();
	if (!IsValid(Mesh) || !IsValid(ExpansionCurve))
	{
		return;
	}

	if (!bOutlineActive)
	{
		PreviousStencilValue = Mesh->CustomDepthStencilValue;
		bPreviousRenderCustomDepth = Mesh->bRenderCustomDepth;
		bOutlineActive = true;
	}

	ElapsedTime = 0.0f;
	SetExpansionRatio(ExpansionCurve->GetFloatValue(0.0f));
	Mesh->SetRenderCustomDepth(true);
	SetComponentTickEnabled(true);
}

void UNPScanOutlineComponent::StopOutline()
{
	SetComponentTickEnabled(false);
	ElapsedTime = 0.0f;
	if (bOutlineActive)
	{
		if (UMeshComponent* Mesh = TargetMesh.Get())
		{
			Mesh->SetRenderCustomDepth(bPreviousRenderCustomDepth);
			Mesh->SetCustomDepthStencilValue(PreviousStencilValue);
		}
		bOutlineActive = false;
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

	if (!bOutlineActive || !TargetMesh.IsValid() || !IsValid(ExpansionCurve))
	{
		StopOutline();
		return;
	}

	ElapsedTime += DeltaTime;
	const float Duration = FMath::Max(0.01f, AnimationDuration);
	const float NormalizedTime = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	SetExpansionRatio(ExpansionCurve->GetFloatValue(NormalizedTime));

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
	if (UMeshComponent* Mesh = TargetMesh.Get())
	{
		const int32 StencilValue = FMath::RoundToInt(FMath::Clamp(Ratio, 0.0f, 1.0f) * 128.0f);
		if (Mesh->CustomDepthStencilValue != StencilValue)
		{
			Mesh->SetCustomDepthStencilValue(StencilValue);
		}
	}
}
