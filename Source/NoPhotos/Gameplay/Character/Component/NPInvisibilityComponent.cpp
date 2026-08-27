#include "Gameplay/Character/Component/NPInvisibilityComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPInvisibility, Log, All);

UNPInvisibilityComponent::UNPInvisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UNPInvisibilityComponent::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		UE_LOG(LogNPInvisibility, Warning, TEXT("투명화 컴포넌트에 ASC가 없습니다: %s"), *GetNameSafe(GetOwner()));
		return;
	}

	InvisibilityTagHandle = AbilitySystem->RegisterGameplayTagEvent(
		NPGameplayTags::State_Invisible, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::HandleInvisibilityTagChanged);

	// 중도 참가 시 태그 복제가 BeginPlay보다 먼저 끝난 경우도 처리합니다.
	HandleInvisibilityTagChanged(
		NPGameplayTags::State_Invisible, AbilitySystem->GetTagCount(NPGameplayTags::State_Invisible));
}

void UNPInvisibilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilitySystem) && InvisibilityTagHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(
			NPGameplayTags::State_Invisible, EGameplayTagEventType::NewOrRemoved).Remove(InvisibilityTagHandle);
	}
	SetComponentTickEnabled(false);
	RestoreMeshes();
	Super::EndPlay(EndPlayReason);
}

void UNPInvisibilityComponent::TickComponent(
	const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 활성 중에만 빙의/Controller 복제 순서 변경과 새로 추가된 캐릭터 메시를 확인합니다.
	RefreshPresentation();
}

void UNPInvisibilityComponent::HandleInvisibilityTagChanged(FGameplayTag, const int32 NewCount)
{
	const bool bNewInvisible = NewCount > 0;
	const bool bChanged = bIsInvisible != bNewInvisible;
	bIsInvisible = bNewInvisible;
	SetComponentTickEnabled(bIsInvisible && GetNetMode() != NM_DedicatedServer);
	RefreshPresentation();
	if (bChanged)
	{
		OnInvisibilityChanged.Broadcast(bIsInvisible);
	}
}

void UNPInvisibilityComponent::RefreshPresentation()
{
	if (!bIsInvisible)
	{
		RestoreMeshes();
		return;
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	const bool bLocallyControlled = Pawn->IsLocallyControlled();
	if (bLastLocallyControlled != bLocallyControlled)
	{
		RestoreMeshes();
		bLastLocallyControlled = bLocallyControlled;
	}

	MeshStates.RemoveAll([](const FNPInvisibilityMeshState& State) { return !State.Mesh.IsValid(); });
	// 자식 Actor를 순회하거나 Visibility를 자식 컴포넌트로 전파하지 않습니다.
	// UWidgetComponent도 UMeshComponent이므로 Skeletal/Static Mesh만 명시적으로 선택합니다.
	TInlineComponentArray<UMeshComponent*> Meshes(GetOwner());
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || Mesh->GetOwner() != GetOwner()
			|| (!Mesh->IsA<USkeletalMeshComponent>() && !Mesh->IsA<UStaticMeshComponent>()))
		{
			continue;
		}
		if (!MeshStates.ContainsByPredicate(
			[Mesh](const FNPInvisibilityMeshState& State) { return State.Mesh.Get() == Mesh; }))
		{
			ApplyToMesh(Mesh);
		}
	}
}

void UNPInvisibilityComponent::ApplyToMesh(UMeshComponent* Mesh)
{
	FNPInvisibilityMeshState& State = MeshStates.AddDefaulted_GetRef();
	State.Mesh = Mesh;
	State.bWasHiddenInGame = Mesh->bHiddenInGame;
	State.bCastShadow = Mesh->CastShadow;
	// 숨겨진 캐릭터의 실루엣이 그림자로 남지 않게 합니다. 물리/애니메이션은 그대로 둡니다.
	Mesh->SetCastShadow(false);

	if (!bLastLocallyControlled)
	{
		State.bDidHideMesh = true;
		Mesh->SetHiddenInGame(true, false);
		return;
	}

	const int32 SlotCount = Mesh->GetNumMaterials();
	State.OriginalMaterials.SetNum(SlotCount);
	State.AppliedMaterials.SetNum(SlotCount);
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		UMaterialInterface* OriginalMaterial = Mesh->GetMaterial(Slot);
		State.OriginalMaterials[Slot] = OriginalMaterial;
		UMaterialInterface* SourceMaterial = SelfMaterialOverride ? SelfMaterialOverride.Get() : OriginalMaterial;
		float ExistingOpacity = 1.0f;
		if (!IsValid(SourceMaterial) || SourceMaterial->GetBlendMode() == BLEND_Opaque
			|| !SourceMaterial->GetScalarParameterValue(
				FHashedMaterialParameterInfo(OpacityParameterName), ExistingOpacity))
		{
			UE_LOG(LogNPInvisibility, Warning,
				TEXT("자기 반투명 머티리얼 설정 필요: Pawn=%s Mesh=%s Slot=%d Material=%s Parameter=%s. 원본을 유지합니다."),
				*GetNameSafe(GetOwner()), *GetNameSafe(Mesh), Slot,
				*GetNameSafe(SourceMaterial), *OpacityParameterName.ToString());
			continue;
		}

		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SourceMaterial, this);
		if (Material)
		{
			Material->SetScalarParameterValue(OpacityParameterName, FMath::Clamp(SelfOpacity, 0.0f, 1.0f));
			State.AppliedMaterials[Slot] = Material;
			Mesh->SetMaterial(Slot, Material);
		}
	}
}

void UNPInvisibilityComponent::RestoreMeshes()
{
	for (const FNPInvisibilityMeshState& State : MeshStates)
	{
		UMeshComponent* Mesh = State.Mesh.Get();
		if (!Mesh)
		{
			continue;
		}
		if (State.bDidHideMesh && Mesh->bHiddenInGame)
		{
			Mesh->SetHiddenInGame(State.bWasHiddenInGame, false);
		}
		if (!Mesh->CastShadow)
		{
			Mesh->SetCastShadow(State.bCastShadow);
		}
		const int32 SlotCount = FMath::Min(Mesh->GetNumMaterials(), State.AppliedMaterials.Num());
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			// 다른 시스템이 교체한 슬롯은 덮어쓰지 않습니다.
			if (State.AppliedMaterials[Slot] && Mesh->GetMaterial(Slot) == State.AppliedMaterials[Slot])
			{
				Mesh->SetMaterial(Slot, State.OriginalMaterials[Slot]);
			}
		}
	}
	MeshStates.Reset();
}
