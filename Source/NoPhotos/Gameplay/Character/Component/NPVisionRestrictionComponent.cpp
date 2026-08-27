#include "Gameplay/Character/Component/NPVisionRestrictionComponent.h"

#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Engine/Scene.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPVisionRestriction, Log, All);

namespace NPVisionFog
{
	const FName StartDistance(TEXT("FogStartDistance"));
	const FName EndDistance(TEXT("FogEndDistance"));
	const FName Color(TEXT("FogColor"));
}

void UNPVisionRestrictionCameraModifier::Initialize(APawn* InPawn, UMaterialInstanceDynamic* InMaterial)
{
	RestrictedPawn = InPawn;
	FogMaterial = InMaterial;
}

void UNPVisionRestrictionCameraModifier::ModifyPostProcess(
	float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	const APawn* Pawn = RestrictedPawn.Get();
	// 매 프레임 확인: 컴포넌트의 다음 0.1초 갱신 전에도 다른 Pawn/관전 카메라로 번지지 않습니다.
	if (Pawn && Pawn->IsLocallyControlled() && CameraOwner && FogMaterial
		&& Pawn->GetController() == CameraOwner->PCOwner && GetViewTarget() == Pawn)
	{
		PostProcessSettings.AddBlendable(FogMaterial, 1.0f);
		PostProcessBlendWeight = 1.0f;
	}
}

UNPVisionRestrictionComponent::UNPVisionRestrictionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UNPVisionRestrictionComponent::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		UE_LOG(LogNPVisionRestriction, Warning, TEXT("시야 제한 컴포넌트에 ASC가 없습니다: %s"), *GetNameSafe(GetOwner()));
		return;
	}

	VisionTagHandle = AbilitySystem->RegisterGameplayTagEvent(
		NPGameplayTags::State_VisionRestricted, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::HandleVisionTagChanged);
	HandleVisionTagChanged(NPGameplayTags::State_VisionRestricted,
		AbilitySystem->GetTagCount(NPGameplayTags::State_VisionRestricted));
}

void UNPVisionRestrictionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilitySystem) && VisionTagHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(
			NPGameplayTags::State_VisionRestricted, EGameplayTagEventType::NewOrRemoved).Remove(VisionTagHandle);
	}
	SetComponentTickEnabled(false);
	RemovePresentation();
	FogMaterialInstance = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UNPVisionRestrictionComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshPresentation();
}

FNPVisionRestrictionSettings UNPVisionRestrictionComponent::GetVisionRestrictionSettings() const
{
	FNPVisionRestrictionSettings Settings = FogSettings;
	Settings.FogStartDistance = FMath::Max(0.0f, Settings.FogStartDistance);
	Settings.MaxViewDistance = FMath::Max(Settings.FogStartDistance + 1.0f, Settings.MaxViewDistance);
	return Settings;
}

float UNPVisionRestrictionComponent::GetMaxViewDistance() const
{
	return bIsVisionRestricted ? GetVisionRestrictionSettings().MaxViewDistance : 0.0f;
}

void UNPVisionRestrictionComponent::HandleVisionTagChanged(FGameplayTag, int32 NewCount)
{
	const bool bNewRestricted = NewCount > 0;
	const bool bChanged = bIsVisionRestricted != bNewRestricted;
	bIsVisionRestricted = bNewRestricted;
	SetComponentTickEnabled(bIsVisionRestricted && GetNetMode() != NM_DedicatedServer);
	RefreshPresentation();
	if (bChanged)
	{
		OnVisionRestrictionChanged.Broadcast(bIsVisionRestricted);
	}
}

void UNPVisionRestrictionComponent::RefreshPresentation()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!bIsVisionRestricted || GetNetMode() == NM_DedicatedServer || !Pawn
		|| !Pawn->IsLocallyControlled() || !PC || !PC->IsLocalController()
		|| !IsValid(CameraManager) || CameraManager->GetViewTarget() != Pawn)
	{
		RemovePresentation();
		return;
	}

	if (AppliedCameraManager.Get() != CameraManager || !IsValid(CameraModifier))
	{
		RemovePresentation();
	}
	else
	{
		return;
	}

	if (!FogMaterialInstance)
	{
		float UnusedScalar = 0.0f;
		FLinearColor UnusedColor;
		const bool bValidMaterial = VisionFogMaterial && VisionFogMaterial->GetMaterial()
			&& VisionFogMaterial->GetMaterial()->MaterialDomain == MD_PostProcess
			&& VisionFogMaterial->GetScalarParameterValue(FMaterialParameterInfo(NPVisionFog::StartDistance), UnusedScalar)
			&& VisionFogMaterial->GetScalarParameterValue(FMaterialParameterInfo(NPVisionFog::EndDistance), UnusedScalar)
			&& VisionFogMaterial->GetVectorParameterValue(FMaterialParameterInfo(NPVisionFog::Color), UnusedColor);
		if (!bValidMaterial)
		{
			if (!bWarnedInvalidMaterial)
			{
				UE_LOG(LogNPVisionRestriction, Warning,
					TEXT("안개 머티리얼 설정 확인: Pawn=%s Material=%s. Post Process 도메인과 FogStartDistance/FogEndDistance/FogColor 파라미터가 필요합니다. 시야 제한 상태는 유지됩니다."),
					*GetNameSafe(Pawn), *GetNameSafe(VisionFogMaterial));
				bWarnedInvalidMaterial = true;
			}
			return;
		}

		FogMaterialInstance = UMaterialInstanceDynamic::Create(VisionFogMaterial, this);
		if (!FogMaterialInstance)
		{
			return;
		}
		const FNPVisionRestrictionSettings Settings = GetVisionRestrictionSettings();
		FogMaterialInstance->SetScalarParameterValue(NPVisionFog::StartDistance, Settings.FogStartDistance);
		FogMaterialInstance->SetScalarParameterValue(NPVisionFog::EndDistance, Settings.MaxViewDistance);
		FogMaterialInstance->SetVectorParameterValue(NPVisionFog::Color, Settings.FogColor);
	}

	CameraModifier = Cast<UNPVisionRestrictionCameraModifier>(
		CameraManager->AddNewCameraModifier(UNPVisionRestrictionCameraModifier::StaticClass()));
	if (CameraModifier)
	{
		CameraModifier->Initialize(Pawn, FogMaterialInstance);
		AppliedCameraManager = CameraManager;
	}
}

void UNPVisionRestrictionComponent::RemovePresentation()
{
	if (IsValid(CameraModifier))
	{
		CameraModifier->DisableModifier(true);
		if (APlayerCameraManager* CameraManager = AppliedCameraManager.Get())
		{
			CameraManager->RemoveCameraModifier(CameraModifier);
		}
		CameraModifier->AddedToCamera(nullptr);
	}
	CameraModifier = nullptr;
	AppliedCameraManager.Reset();
}
