#include "NPInvisibilityMapEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/AbilitySystem/Effects/NPInvisibilityGameplayEffect.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "Net/UnrealNetwork.h"
#include "NPMapEventManager.h"
#include "NPMapEventSpawnVolume.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPInvisibilityEvent, Log, All);

ANPInvisibilityMapEvent::ANPInvisibilityMapEvent()
{
	LocationSource = ENPMapEventLocationSource::Volume;
	InvisibilitySpawnGroup = FGameplayTag::RequestGameplayTag(FName(TEXT("Invisibility")), false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RegionFogRoot")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	RegionFogMesh = CubeAsset.Object;
}

void ANPInvisibilityMapEvent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPInvisibilityMapEvent, FogRegions);
}

void ANPInvisibilityMapEvent::BeginPlay()
{
	Super::BeginPlay();
	// 초기 복제가 BeginPlay보다 먼저 온 경우도 처리합니다.
	RefreshRegionFog();
}

void ANPInvisibilityMapEvent::ApplyEventState_Implementation(const bool bNewActive)
{
	if (!HasAuthority())
	{
		// bIsActive와 FogRegions의 RepNotify 순서는 보장되지 않으므로 양쪽에서 갱신합니다.
		RefreshRegionFog();
		return;
	}

	if (bNewActive)
	{
		bWarnedMissingVolumes = false;
		RefreshAffectedPlayers();
		// 진입/이탈뿐 아니라 중도 참가 및 리스폰으로 생긴 새 ASC도 함께 처리합니다.
		GetWorldTimerManager().SetTimer(
			PlayerRefreshTimer, this, &ThisClass::RefreshAffectedPlayers,
			FMath::Max(0.02f, VolumeCheckInterval), true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
		RemoveAppliedEffects();
		FogRegions.Reset();
		ClearRegionFog();
		ForceNetUpdate();
	}
}

void ANPInvisibilityMapEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRegionFog();
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(PlayerRefreshTimer);
		RemoveAppliedEffects();
	}
	Super::EndPlay(EndPlayReason);
}

void ANPInvisibilityMapEvent::RefreshAffectedPlayers()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsEventActive())
	{
		return;
	}

	const UNPMapEventManagerComponent* EventManager = GetOwner()
		? GetOwner()->FindComponentByClass<UNPMapEventManagerComponent>()
		: nullptr;
	TArray<ANPMapEventSpawnVolume*> ActiveVolumes;
	if (EventManager)
	{
		// Collector 기본 그룹 상속과 가중치 0 제외 규칙을 기존 생성 이벤트와 공유합니다.
		EventManager->GetSpawnVolumesForGroup(InvisibilitySpawnGroup, ActiveVolumes);
	}
	if (ActiveVolumes.IsEmpty() && !bWarnedMissingVolumes)
	{
		UE_LOG(LogNPInvisibilityEvent, Warning,
			TEXT("투명화 영역 없음: Event=%s Manager=%s Group=%s. Collector/Volume 그룹과 가중치를 확인하세요. 전역 적용하지 않습니다."),
			*GetNameSafe(this), *GetNameSafe(EventManager), *InvisibilitySpawnGroup.ToString());
		bWarnedMissingVolumes = true;
	}
	UpdateFogRegions(ActiveVolumes);

	TSet<TWeakObjectPtr<UAbilitySystemComponent>> EligiblePlayerSystems;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		const FVector PawnLocation = Pawn->GetActorLocation();
		const bool bInsideAnyVolume = ActiveVolumes.ContainsByPredicate(
			[&PawnLocation](const ANPMapEventSpawnVolume* Volume)
			{
				return IsValid(Volume) && Volume->IsInsideSpawnBounds(PawnLocation);
			});
		if (!bInsideAnyVolume)
		{
			continue;
		}

		UAbilitySystemComponent* AbilitySystem =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
		if (!IsValid(AbilitySystem) || AbilitySystem->GetAvatarActor() != Pawn)
		{
			continue;
		}

		const TWeakObjectPtr<UAbilitySystemComponent> SystemKey(AbilitySystem);
		EligiblePlayerSystems.Add(SystemKey);
		const FActiveGameplayEffectHandle* ExistingHandle = AppliedEffects.Find(SystemKey);
		if (ExistingHandle && AbilitySystem->GetActiveGameplayEffect(*ExistingHandle))
		{
			continue;
		}

		FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
			UNPInvisibilityGameplayEffect::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			const FActiveGameplayEffectHandle Handle =
				AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (Handle.IsValid())
			{
				AppliedEffects.Add(SystemKey, Handle);
			}
		}
	}

	// 모든 적용 영역에서 이탈했거나, 이전 Pawn/접속 종료로 대상에서 빠졌다면 제거합니다.
	// 볼륨이 겹쳐도 하나의 GE만 유지하며, 영역이 없어져도 기존 효과를 정리합니다.
	for (auto Iterator = AppliedEffects.CreateIterator(); Iterator; ++Iterator)
	{
		if (!EligiblePlayerSystems.Contains(Iterator.Key()))
		{
			if (UAbilitySystemComponent* AbilitySystem = Iterator.Key().Get())
			{
				AbilitySystem->RemoveActiveGameplayEffect(Iterator.Value());
			}
			Iterator.RemoveCurrent();
		}
	}
}

void ANPInvisibilityMapEvent::RemoveAppliedEffects()
{
	for (const auto& Entry : AppliedEffects)
	{
		if (UAbilitySystemComponent* AbilitySystem = Entry.Key.Get())
		{
			AbilitySystem->RemoveActiveGameplayEffect(Entry.Value);
		}
	}
	AppliedEffects.Reset();
}

void ANPInvisibilityMapEvent::UpdateFogRegions(const TArray<ANPMapEventSpawnVolume*>& ActiveVolumes)
{
	TArray<FNPInvisibilityFogRegion> NewRegions;
	if (bShowRegionFog)
	{
		NewRegions.Reserve(ActiveVolumes.Num());
		for (const ANPMapEventSpawnVolume* Volume : ActiveVolumes)
		{
			// SpawnBounds는 기본 클래스의 Root입니다. 표시/충돌/복제 설정은 전혀 변경하지 않습니다.
			const UBoxComponent* Bounds = IsValid(Volume) ? Cast<UBoxComponent>(Volume->GetRootComponent()) : nullptr;
			if (!Bounds)
			{
				continue;
			}
			FNPInvisibilityFogRegion& Region = NewRegions.AddDefaulted_GetRef();
			Region.BoundsTransform = Bounds->GetComponentTransform();
			Region.BoxExtent = Bounds->GetUnscaledBoxExtent();
		}
	}

	bool bChanged = NewRegions.Num() != FogRegions.Num();
	for (int32 Index = 0; !bChanged && Index < NewRegions.Num(); ++Index)
	{
		bChanged = !NewRegions[Index].BoundsTransform.Equals(FogRegions[Index].BoundsTransform)
			|| !NewRegions[Index].BoxExtent.Equals(FogRegions[Index].BoxExtent);
	}
	if (bChanged)
	{
		FogRegions = MoveTemp(NewRegions);
		ForceNetUpdate();
		RefreshRegionFog(); // Listen Server 호스트에도 동일하게 표시합니다.
	}
}

void ANPInvisibilityMapEvent::OnRep_FogRegions()
{
	RefreshRegionFog();
}

void ANPInvisibilityMapEvent::RefreshRegionFog()
{
	if (!HasActorBegunPlay() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!IsEventActive() || !bShowRegionFog || FogRegions.IsEmpty())
	{
		ClearRegionFog();
		return;
	}

	if (RegionFogMIDs.IsEmpty())
	{
		float UnusedOpacity = 0.0f;
		FLinearColor UnusedColor;
		bool bValidMaterial = RegionFogMaterial && RegionFogMaterial->GetMaterial()
			&& RegionFogMaterial->GetMaterial()->MaterialDomain == MD_Surface
			&& IsTranslucentBlendMode(RegionFogMaterial->GetBlendMode())
			&& RegionFogMaterial->IsTwoSided()
			&& RegionFogMaterial->GetMaterial()->bDisableDepthTest
			&& RegionFogMaterial->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RegionFogOpacity")), UnusedOpacity)
			&& RegionFogMaterial->GetVectorParameterValue(FMaterialParameterInfo(TEXT("RegionFogColor")), UnusedColor);
		// 이전의 균일 반투명 머티리얼을 그대로 사용하면 다시 박스 벽처럼 보입니다.
		for (const TCHAR* Parameter : { TEXT("RegionCenter"), TEXT("RegionAxisX"), TEXT("RegionAxisY"),
			TEXT("RegionAxisZ"), TEXT("RegionHalfExtent"), TEXT("RegionFogShape") })
		{
			bValidMaterial = bValidMaterial && RegionFogMaterial->GetVectorParameterValue(
				FMaterialParameterInfo(Parameter), UnusedColor);
		}
		if (!RegionFogMesh || !bValidMaterial)
		{
			if (!bWarnedRegionFogSetup)
			{
				UE_LOG(LogNPInvisibilityEvent, Warning,
					TEXT("영역 안개 머티리얼 갱신 필요: Event=%s Material=%s. Surface/Translucent/Two Sided/Disable Depth Test 및 RegionFogOpacity/Color, RegionCenter, RegionAxisX/Y/Z, RegionHalfExtent, RegionFogShape를 확인하세요. SoftRegionFog.md 참고. 영역 판정은 유지됩니다."),
					*GetNameSafe(this), *GetNameSafe(RegionFogMaterial));
				bWarnedRegionFogSetup = true;
			}
			return;
		}
	}

	// 개수가 바뀔 때만 생성/제거합니다. 움직이는 볼륨은 기존 메시의 Transform만 갱신합니다.
	while (RegionFogComponents.Num() > FogRegions.Num())
	{
		UStaticMeshComponent* Mesh = RegionFogComponents.Pop();
		RegionFogMIDs.Pop();
		if (IsValid(Mesh))
		{
			Mesh->DestroyComponent();
		}
	}
	while (RegionFogComponents.Num() < FogRegions.Num())
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(RegionFogMaterial, this);
		if (!Material)
		{
			ClearRegionFog();
			return;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Mesh->SetIsReplicated(false);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->SetAffectDistanceFieldLighting(false);
		Mesh->SetAffectDynamicIndirectLighting(false);
		Mesh->SetReceivesDecals(false);
		Mesh->SetStaticMesh(RegionFogMesh);
		Mesh->SetMaterial(0, Material);
		Mesh->SetupAttachment(GetRootComponent());
		// 이벤트 액터의 Transform과 관계없이 서버 판정 박스의 월드 좌표를 사용합니다.
		Mesh->SetAbsolute(true, true, true);
		RegionFogComponents.Add(Mesh);
		RegionFogMIDs.Add(Material);
	}

	const FBox MeshBounds = RegionFogMesh->GetBoundingBox();
	const FVector MeshExtent = MeshBounds.GetExtent().ComponentMax(FVector(UE_SMALL_NUMBER));
	for (int32 Index = 0; Index < FogRegions.Num(); ++Index)
	{
		const FNPInvisibilityFogRegion& Region = FogRegions[Index];
		UStaticMeshComponent* Mesh = RegionFogComponents[Index];
		UMaterialInstanceDynamic* Material = RegionFogMIDs[Index];
		const FVector HalfExtent = Region.BoxExtent * Region.BoundsTransform.GetScale3D().GetAbs();
		const FQuat Rotation = Region.BoundsTransform.GetRotation();
		const auto AsColor = [](const FVector& Value) { return FLinearColor(Value.X, Value.Y, Value.Z, 0.0f); };
		Material->SetVectorParameterValue(TEXT("RegionCenter"), AsColor(Region.BoundsTransform.GetLocation()));
		Material->SetVectorParameterValue(TEXT("RegionAxisX"), AsColor(Rotation.GetAxisX()));
		Material->SetVectorParameterValue(TEXT("RegionAxisY"), AsColor(Rotation.GetAxisY()));
		Material->SetVectorParameterValue(TEXT("RegionAxisZ"), AsColor(Rotation.GetAxisZ()));
		Material->SetVectorParameterValue(TEXT("RegionHalfExtent"), AsColor(HalfExtent));
		Material->SetVectorParameterValue(TEXT("RegionFogShape"), FLinearColor(
			FMath::Max(1.0f, RegionFogEdgeFadeDistance), FMath::Max(0.0f, RegionFogCornerRadius),
			FMath::Max(0.0001f, RegionFogNoiseScale), FMath::Clamp(RegionFogNoiseStrength, 0.0f, 1.0f)));
		Material->SetScalarParameterValue(TEXT("RegionFogOpacity"), FMath::Clamp(RegionFogOpacity, 0.0f, 1.0f));
		Material->SetVectorParameterValue(TEXT("RegionFogColor"), RegionFogColor);
		FTransform MeshTransform = Region.BoundsTransform;
		MeshTransform.SetScale3D(Region.BoundsTransform.GetScale3D() * (Region.BoxExtent / MeshExtent));
		MeshTransform.SetLocation(Region.BoundsTransform.GetLocation() - MeshTransform.TransformVector(MeshBounds.GetCenter()));
		Mesh->SetWorldTransform(MeshTransform);
		if (!Mesh->IsRegistered())
		{
			Mesh->RegisterComponent();
		}
	}
}

void ANPInvisibilityMapEvent::ClearRegionFog()
{
	for (UStaticMeshComponent* Mesh : RegionFogComponents)
	{
		if (IsValid(Mesh))
		{
			Mesh->DestroyComponent();
		}
	}
	RegionFogComponents.Reset();
	RegionFogMIDs.Reset();
}
