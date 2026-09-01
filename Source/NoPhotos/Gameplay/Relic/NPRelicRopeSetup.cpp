#include "Gameplay/Relic/NPRelicRopeSetup.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Gimmick/Components/NPRelicGimmickComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Gameplay/Rope/NPRopeAnchorActor.h"
#include "Gameplay/Rope/NPRopeSegmentActor.h"
#include "Net/UnrealNetwork.h"
#include "NoPhotos.h"
#include "TimerManager.h"

namespace
{
FString GetStableMarkerName(const USceneComponent* Component)
{
	FString ComponentName = Component ? Component->GetName() : FString();
	ComponentName.RemoveFromEnd(TEXT("_GEN_VARIABLE"));
	ComponentName.TrimStartAndEndInline();
	return ComponentName;
}
}

ANPRelicRopeSetup::ANPRelicRopeSetup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	RopeAnchorClass = ANPRopeAnchorActor::StaticClass();
}

void ANPRelicRopeSetup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && bRopeReleaseStarted)
	{
		UpdateRopeRelease(DeltaSeconds);
	}
}

void ANPRelicRopeSetup::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPRelicRopeSetup, RemainingRopeCount);
	DOREPLIFETIME(ANPRelicRopeSetup, RopeBindings);
}

bool ANPRelicRopeSetup::IsRopeAssemblyReady() const
{
	if (!IsValid(Relic) || RopeBindings.IsEmpty())
	{
		return false;
	}

	for (const FNPRelicRopeBinding& Binding : RopeBindings)
	{
		if (!IsValid(Binding.PinActor)
			|| !IsValid(Binding.RopeSegment))
		{
			return false;
		}
	}

	return true;
}

bool ANPRelicRopeSetup::PrepareRelicSetup()
{
	return !bSpawnAssemblyOnBeginPlay || SpawnAssemblyFromMarkers();
}

bool ANPRelicRopeSetup::SpawnAssemblyFromMarkers()
{
	if (!GetWorld() || !RelicClass || !PinClass || !RopeClass)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] Spawn classes are incomplete. Relic=%s Pin=%s Rope=%s"),
			*GetNameSafe(this),
			*GetNameSafe(RelicClass.Get()),
			*GetNameSafe(PinClass.Get()),
			*GetNameSafe(RopeClass.Get()));
		return false;
	}

	USceneComponent* RelicSpawnPoint = FindMarker(
		FName(TEXT("RelicSpawnPoint")));
	if (!RelicSpawnPoint)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] RelicSpawnPoint component was not found."),
			*GetNameSafe(this));
		return false;
	}

	USceneComponent* RopeWrapSpawnPoint = FindMarker(
		FName(TEXT("RopeWrapSpawnPoint")));
	if (RopeWrapVisualClass && !RopeWrapSpawnPoint)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] RopeWrapVisualClass is set, but RopeWrapSpawnPoint was not found."),
			*GetNameSafe(this));
		return false;
	}

	TMap<FString, USceneComponent*> PinSpawnPoints;
	TMap<FString, USceneComponent*> RopeAnchorPoints;
	CollectIndexedMarkers(TEXT("PinSpawnPoint"), PinSpawnPoints);
	CollectIndexedMarkers(TEXT("RopeAnchorPoint"), RopeAnchorPoints);
	if (PinSpawnPoints.IsEmpty())
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] No PinSpawnPoint components were found."),
			*GetNameSafe(this));
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TSubclassOf<ANPRopeAnchorActor> EffectiveAnchorClass = RopeAnchorClass;
	if (!EffectiveAnchorClass)
	{
		EffectiveAnchorClass = ANPRopeAnchorActor::StaticClass();
	}

	Relic = GetWorld()->SpawnActor<ANPBaseRelic>(
		RelicClass,
		RelicSpawnPoint->GetComponentTransform(),
		SpawnParameters);
	if (!Relic)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] Failed to spawn Relic."),
			*GetNameSafe(this));
		return false;
	}

	const FTransform RopeReleaseTransform = RopeWrapSpawnPoint
		? RopeWrapSpawnPoint->GetComponentTransform()
		: RelicSpawnPoint->GetComponentTransform();
	SpawnedRopeReleaseRoot = GetWorld()->SpawnActor<ANPRopeAnchorActor>(
		ANPRopeAnchorActor::StaticClass(),
		RopeReleaseTransform,
		SpawnParameters);
	if (!SpawnedRopeReleaseRoot)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] Failed to spawn Rope release root."),
			*GetNameSafe(this));
		return false;
	}
	SpawnedRopeReleaseRoot->AttachToActor(
		Relic,
		FAttachmentTransformRules::KeepWorldTransform);

	SpawnedRopeWrapVisual = nullptr;
	if (RopeWrapVisualClass)
	{
		SpawnedRopeWrapVisual = GetWorld()->SpawnActor<AActor>(
			RopeWrapVisualClass,
			RopeReleaseTransform,
			SpawnParameters);
		if (!SpawnedRopeWrapVisual)
		{
			UE_LOG(
				LogNoPhotos,
				Warning,
				TEXT("[%s] Failed to spawn Rope wrap visual."),
				*GetNameSafe(this));
			return false;
		}
		SpawnedRopeWrapVisual->SetReplicates(true);
		SpawnedRopeWrapVisual->SetReplicateMovement(true);
		SpawnedRopeWrapVisual->AttachToActor(
			SpawnedRopeReleaseRoot,
			FAttachmentTransformRules::KeepWorldTransform);
	}

	RopeBindings.Reset();
	SpawnedPins.Reset();
	SpawnedRopes.Reset();
	SpawnedAnchors.Reset();

	TArray<FString> ConnectionIds;
	PinSpawnPoints.GetKeys(ConnectionIds);
	ConnectionIds.Sort();
	for (const FString& ConnectionId : ConnectionIds)
	{
		USceneComponent* const* PinSpawnPoint = PinSpawnPoints.Find(ConnectionId);
		USceneComponent* const* RopeAnchorPoint = RopeAnchorPoints.Find(ConnectionId);
		if (!PinSpawnPoint || !*PinSpawnPoint || !RopeAnchorPoint || !*RopeAnchorPoint)
		{
			UE_LOG(
				LogNoPhotos,
				Warning,
				TEXT("[%s] Marker pair is incomplete for id '%s'."),
				*GetNameSafe(this),
				*ConnectionId);
			continue;
		}

		AActor* Pin = GetWorld()->SpawnActor<AActor>(
			PinClass,
			(*PinSpawnPoint)->GetComponentTransform(),
			SpawnParameters);
		ANPRopeAnchorActor* Anchor = GetWorld()->SpawnActor<ANPRopeAnchorActor>(
			EffectiveAnchorClass,
			(*RopeAnchorPoint)->GetComponentTransform(),
			SpawnParameters);
		ANPRopeSegmentActor* Rope = GetWorld()->SpawnActor<ANPRopeSegmentActor>(
			RopeClass,
			(*RopeAnchorPoint)->GetComponentTransform(),
			SpawnParameters);
		if (!Pin || !Anchor || !Rope)
		{
			UE_LOG(
				LogNoPhotos,
				Warning,
				TEXT("[%s] Failed to spawn connection '%s'. Pin=%s Anchor=%s Rope=%s"),
				*GetNameSafe(this),
				*ConnectionId,
				*GetNameSafe(Pin),
				*GetNameSafe(Anchor),
				*GetNameSafe(Rope));
			continue;
		}

		// PinClass Blueprint의 설정 누락과 관계없이 서버가 생성한 Pin과
		// 그 이동을 모든 클라이언트에 전달합니다.
		Pin->SetReplicates(true);
		Pin->SetReplicateMovement(true);

		Anchor->AttachToActor(
			SpawnedRopeReleaseRoot,
			FAttachmentTransformRules::KeepWorldTransform);

		FNPRopeEndpoint StartEndpoint;
		StartEndpoint.TargetActor = Anchor;
		StartEndpoint.bPreserveWorldLocationOnAttach = false;
		Rope->SetStartEndpoint(StartEndpoint);

		FNPRopeEndpoint EndEndpoint;
		EndEndpoint.TargetActor = Pin;
		EndEndpoint.ComponentTag = PinRopeComponentTag;
		EndEndpoint.SocketName = PinRopeSocketName;
		EndEndpoint.LocalOffset = PinRopeLocalOffset;
		EndEndpoint.bPreserveWorldLocationOnAttach = false;
		Rope->SetEndEndpoint(EndEndpoint);

		FNPRelicRopeBinding Binding;
		Binding.PinActor = Pin;
		Binding.RopeSegment = Rope;
		RopeBindings.Add(Binding);
		SpawnedPins.Add(Pin);
		SpawnedRopes.Add(Rope);
		SpawnedAnchors.Add(Anchor);
	}

	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Spawned rope relic assembly. Relic=%s Connections=%d"),
		*GetNameSafe(this),
		*GetNameSafe(Relic),
		RopeBindings.Num());
	if (RopeBindings.IsEmpty())
	{
		return false;
	}

	OnRep_RopeAssembly();
	ForceNetUpdate();
	return true;
}

USceneComponent* ANPRelicRopeSetup::FindMarker(const FName& MarkerName) const
{
	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component
			&& GetStableMarkerName(Component) == MarkerName.ToString())
		{
			return Component;
		}
	}

	return nullptr;
}

void ANPRelicRopeSetup::CollectIndexedMarkers(
	const FString& Prefix,
	TMap<FString, USceneComponent*>& OutMarkers) const
{
	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (!Component)
		{
			continue;
		}

		const FString ComponentName = GetStableMarkerName(Component);
		if (!ComponentName.StartsWith(Prefix))
		{
			continue;
		}

		const FString ConnectionId = ComponentName.RightChop(Prefix.Len());
		if (!ConnectionId.IsEmpty())
		{
			OutMarkers.Add(ConnectionId, Component);
		}
	}
}

void ANPRelicRopeSetup::CollectGimmicks()
{
	Super::CollectGimmicks();

	for (const FNPRelicRopeBinding& Binding : RopeBindings)
	{
		CollectGimmicksFromActor(Binding.PinActor);
	}
}

void ANPRelicRopeSetup::RefreshRopeBindings()
{
	int32 NewRemainingRopeCount = 0;
	for (const FNPRelicRopeBinding& Binding : RopeBindings)
	{
		if (!IsRopeBindingCompleted(Binding))
		{
			++NewRemainingRopeCount;
			continue;
		}

		if (Binding.RopeSegment
			&& Binding.RopeSegment->GetEndState()
				!= ENPRopeEndpointState::Free)
		{
			Binding.RopeSegment->ReleaseEnd();
		}
	}

	if (RemainingRopeCount == NewRemainingRopeCount)
	{
		return;
	}

	RemainingRopeCount = NewRemainingRopeCount;
	OnRep_RemainingRopeCount();
	ForceNetUpdate();
}

void ANPRelicRopeSetup::RefreshRelicLock()
{
	RefreshRopeBindings();
	const bool bAllGimmicksCompleted =
		RemainingRopeCount == 0 && AreAllGimmicksCompleted();

	if (bAllGimmicksCompleted)
	{
		ScheduleRopeRelease();
		return;
	}

	Relic->SetUnlocked(false);
}

void ANPRelicRopeSetup::ScheduleRopeRelease()
{
	if (bRopeReleaseScheduled || bRopeReleaseStarted)
	{
		return;
	}

	if (RopeReleaseDelay <= UE_SMALL_NUMBER || !GetWorld())
	{
		StartRopeRelease();
		return;
	}

	bRopeReleaseScheduled = true;
	GetWorldTimerManager().SetTimer(
		RopeReleaseDelayTimer,
		this,
		&ANPRelicRopeSetup::StartRopeRelease,
		RopeReleaseDelay,
		false);

	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Scheduled Rope wrap release in %.2f seconds."),
		*GetNameSafe(this),
		RopeReleaseDelay);
}

void ANPRelicRopeSetup::StartRopeRelease()
{
	if (bRopeReleaseStarted)
	{
		return;
	}

	bRopeReleaseScheduled = false;
	bRopeReleaseStarted = true;
	ReleaseRelicThroughGrabContract();

	if (!SpawnedRopeReleaseRoot)
	{
		return;
	}

	RopeReleaseElapsedTime = 0.0f;
	RopeReleaseStartLocation = SpawnedRopeReleaseRoot->GetActorLocation();
	SpawnedRopeReleaseRoot->DetachFromActor(
		FDetachmentTransformRules::KeepWorldTransform);

	// 마지막 고정이 풀리면 Cable 양끝을 모두 자유 상태로 전환합니다.
	// RopeSegment는 이 상태에서만 충돌을 켜므로 연결 중 비용은 그대로 유지됩니다.
	for (const FNPRelicRopeBinding& Binding : RopeBindings)
	{
		if (!Binding.RopeSegment)
		{
			continue;
		}

		if (Binding.RopeSegment->GetEndState()
			!= ENPRopeEndpointState::Free)
		{
			Binding.RopeSegment->ReleaseEnd();
		}
		if (Binding.RopeSegment->GetStartState()
			!= ENPRopeEndpointState::Free)
		{
			Binding.RopeSegment->ReleaseStart();
		}
	}

	RopeReleaseTargetLocation = ResolveRopeReleaseTargetLocation();
	SetActorTickEnabled(true);

	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Started Rope wrap release. Root=%s Start=%s Target=%s GroundSearch=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedRopeReleaseRoot),
		*RopeReleaseStartLocation.ToCompactString(),
		*RopeReleaseTargetLocation.ToCompactString(),
		bReleaseRopeWrapToGround ? TEXT("true") : TEXT("false"));
}

void ANPRelicRopeSetup::ReleaseRelicThroughGrabContract()
{
	if (!Relic)
	{
		return;
	}

	// BaseRelic을 수정하지 않고 기존 Grab 경로를 통해
	// 내부 Display 상태와 물리 상태를 함께 전환합니다.
	Relic->SetUnlocked(true);
	if (!Relic->IsDisplayed())
	{
		return;
	}

	UGrabbableComponent* Grabbable =
		Relic->FindComponentByClass<UGrabbableComponent>();
	UPrimitiveComponent* RootPrimitive =
		Cast<UPrimitiveComponent>(Relic->GetRootComponent());
	if (!Grabbable || !RootPrimitive || !Grabbable->CanBeGrabbed())
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] Could not release Relic through the Grab contract. Relic=%s Grabbable=%s RootPrimitive=%s CanGrab=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Relic),
			*GetNameSafe(Grabbable),
			*GetNameSafe(RootPrimitive),
			Grabbable && Grabbable->CanBeGrabbed()
				? TEXT("true")
				: TEXT("false"));
		return;
	}

	Grabbable->NotifyGrabStarted(RootPrimitive);
	Grabbable->NotifyGrabEnded();
	if (Relic->IsDisplayed())
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] Relic remained displayed after the Grab release signal. Relic=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Relic));
		return;
	}

	const bool bSimulatingPhysics = RootPrimitive->IsSimulatingPhysics();
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Relic Rope release applied. Relic=%s Root=%s SimulatingPhysics=%s CanEditSimulatePhysics=%s CollisionEnabled=%d"),
		*GetNameSafe(this),
		*GetNameSafe(Relic),
		*GetNameSafe(RootPrimitive),
		bSimulatingPhysics ? TEXT("true") : TEXT("false"),
		RootPrimitive->CanEditSimulatePhysics()
			? TEXT("true")
			: TEXT("false"),
		static_cast<int32>(RootPrimitive->GetCollisionEnabled()));
	if (!bSimulatingPhysics)
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] Relic release completed, but RootComponent is not simulating physics. Check the RelicMesh asset and physics collision settings."),
			*GetNameSafe(this));
	}
}

void ANPRelicRopeSetup::UpdateRopeRelease(float DeltaSeconds)
{
	if (!SpawnedRopeReleaseRoot)
	{
		bRopeReleaseStarted = false;
		SetActorTickEnabled(false);
		return;
	}

	RopeReleaseElapsedTime += DeltaSeconds;
	const float Alpha = RopeReleaseDuration <= UE_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(
			RopeReleaseElapsedTime / RopeReleaseDuration,
			0.0f,
			1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	SpawnedRopeReleaseRoot->SetActorLocation(
		FMath::Lerp(
			RopeReleaseStartLocation,
			RopeReleaseTargetLocation,
			SmoothAlpha));

	if (Alpha >= 1.0f)
	{
		bRopeReleaseStarted = false;
		SetActorTickEnabled(false);
	}
}

FVector ANPRelicRopeSetup::ResolveRopeReleaseTargetLocation() const
{
	FVector TargetLocation =
		RopeReleaseStartLocation + RopeReleaseWorldOffset;
	if (!bReleaseRopeWrapToGround || !GetWorld())
	{
		return TargetLocation;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(RelicRopeReleaseGround),
		false,
		this);
	QueryParams.AddIgnoredActor(Relic);
	QueryParams.AddIgnoredActor(SpawnedRopeReleaseRoot);
	QueryParams.AddIgnoredActor(SpawnedRopeWrapVisual);
	for (const AActor* Pin : SpawnedPins)
	{
		QueryParams.AddIgnoredActor(Pin);
	}
	for (const ANPRopeSegmentActor* Rope : SpawnedRopes)
	{
		QueryParams.AddIgnoredActor(Rope);
	}
	for (const ANPRopeAnchorActor* Anchor : SpawnedAnchors)
	{
		QueryParams.AddIgnoredActor(Anchor);
	}

	const FVector TraceStart = RopeReleaseStartLocation;
	const FVector TraceEnd = TraceStart
		- FVector::UpVector * RopeReleaseGroundTraceDistance;
	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_WorldStatic,
		QueryParams))
	{
		TargetLocation.Z = GroundHit.ImpactPoint.Z
			+ RopeReleaseGroundClearance;
	}

	return TargetLocation;
}

bool ANPRelicRopeSetup::IsRopeBindingCompleted(
	const FNPRelicRopeBinding& Binding) const
{
	if (!Binding.PinActor)
	{
		return false;
	}

	TArray<UNPRelicGimmickComponent*> PinGimmicks;
	Binding.PinActor->GetComponents<UNPRelicGimmickComponent>(PinGimmicks);
	if (PinGimmicks.IsEmpty())
	{
		return false;
	}

	for (const UNPRelicGimmickComponent* Gimmick : PinGimmicks)
	{
		if (!Gimmick || !Gimmick->IsCompleted())
		{
			return false;
		}
	}

	return true;
}

void ANPRelicRopeSetup::OnRep_RemainingRopeCount()
{
	OnRemainingRopeCountChanged.Broadcast(RemainingRopeCount);
}

void ANPRelicRopeSetup::OnRep_RopeAssembly()
{
	if (!IsRopeAssemblyReady())
	{
		UE_LOG(
			LogNoPhotos,
			Warning,
			TEXT("[%s] Rope assembly replication received before all references were ready. Role=%s Relic=%s Bindings=%d"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("Authority") : TEXT("Client"),
			*GetNameSafe(Relic),
			RopeBindings.Num());
		return;
	}

	if (bHasBroadcastRopeAssemblyReady
		|| bRopeAssemblyReadyNotificationPending
		|| !GetWorld())
	{
		return;
	}

	// 초기 복제의 RepNotify는 Blueprint BeginPlay보다 먼저 실행될 수
	// 있으므로 다음 Tick에 알림을 보내 Blueprint가 바인딩할 시간을 줍니다.
	bRopeAssemblyReadyNotificationPending = true;
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this,
			&ANPRelicRopeSetup::BroadcastRopeAssemblyReady));
}

void ANPRelicRopeSetup::BroadcastRopeAssemblyReady()
{
	bRopeAssemblyReadyNotificationPending = false;
	if (bHasBroadcastRopeAssemblyReady || !IsRopeAssemblyReady())
	{
		return;
	}

	bHasBroadcastRopeAssemblyReady = true;
	UE_LOG(
		LogNoPhotos,
		Log,
		TEXT("[%s] Rope assembly ready. Role=%s Relic=%s Bindings=%d"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("Authority") : TEXT("Client"),
		*GetNameSafe(Relic),
		RopeBindings.Num());
	OnRopeAssemblyReady.Broadcast();
}
