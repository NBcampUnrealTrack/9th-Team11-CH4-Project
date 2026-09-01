#include "NPGhostFollowerActor.h"

#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/MapEvents/Possession/NPPossessionMapEvent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameters.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPGhostFollower, Log, All);

ANPGhostFollowerActor::ANPGhostFollowerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	// Roaming 유령이 SpawnActorDeferred 이전부터 네트워크 액터로 등록되도록 기본 복제를 켭니다.
	// 화면별 등 뒤 고스트는 InitializeFollower에서 생성 완료 전에 다시 복제를 끕니다.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 30.0f;
	SetActorEnableCollision(false);

	FollowRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FollowRoot"));
	SetRootComponent(FollowRoot);
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(FollowRoot);
	GhostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
	GhostMesh->SetupAttachment(VisualRoot);
	GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostMesh->SetGenerateOverlapEvents(false);
	GhostMesh->SetCanEverAffectNavigation(false);
	AnimatedGhostMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedGhostMesh"));
	AnimatedGhostMesh->SetupAttachment(VisualRoot);
	AnimatedGhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimatedGhostMesh->SetGenerateOverlapEvents(false);
	AnimatedGhostMesh->SetCanEverAffectNavigation(false);
	RoamingContactSphere = CreateDefaultSubobject<USphereComponent>(TEXT("RoamingContactSphere"));
	RoamingContactSphere->SetupAttachment(FollowRoot);
	RoamingContactSphere->InitSphereRadius(RoamingContactRadius);
	RoamingContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoamingContactSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	RoamingContactSphere->SetGenerateOverlapEvents(false);
	RoamingContactSphere->SetCanEverAffectNavigation(false);
	RoamingContactSphere->OnComponentBeginOverlap.AddDynamic(
		this, &ThisClass::HandleRoamingContactBeginOverlap);
}

void ANPGhostFollowerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPGhostFollowerActor, bRoamingGhost);
	DOREPLIFETIME(ANPGhostFollowerActor, bRoamingGhostConsumed);
}

bool ANPGhostFollowerActor::InitializeFollower(ANPStablePhysicsPawn* InTarget)
{
	if (!IsValid(InTarget) || InTarget->IsActorBeingDestroyed()
		|| InTarget->GetWorld() != GetWorld() || FollowTarget.IsValid())
	{
		return false;
	}
	FollowTarget = InTarget;
	// Deferred spawn 중 SetReplicates 호출은 pre-init 경고를 발생시키므로 직접 설정합니다.
	bReplicates = false;
	SetReplicateMovement(false);
	AddTickPrerequisiteActor(InTarget);
	UpdateFollow(0.0f, true);
	return true;
}

bool ANPGhostFollowerActor::InitializeRoamingGhost()
{
	if (HasActorBegunPlay() || FollowTarget.IsValid())
	{
		UE_LOG(LogNPGhostFollower, Warning,
			TEXT("[GhostTrace] Roaming 초기화 거부: Actor=%s BegunPlay=%d FollowTarget=%s"),
			*GetNameSafe(this), HasActorBegunPlay() ? 1 : 0, *GetNameSafe(FollowTarget.Get()));
		return false;
	}
	bRoamingInitializationRequested = true;
	bRoamingGhost = true;
	// SpawnActorDeferred 상태에서는 SetReplicates가 초기화 전 Actor 경고를 발생시킵니다.
	// PostInitProperties가 이 값을 읽어 RemoteRole을 구성하므로 직접 설정하는 것이 올바른 경로입니다.
	bReplicates = true;
	SetReplicatingMovement(true);
	UE_LOG(LogNPGhostFollower, Display,
		TEXT("[GhostTrace] Roaming 초기화 완료: Actor=%s Requested=%d Roaming=%d Replicates=%d"),
		*GetNameSafe(this), bRoamingInitializationRequested ? 1 : 0,
		bRoamingGhost ? 1 : 0, GetIsReplicated() ? 1 : 0);
	return true;
}

bool ANPGhostFollowerActor::SetRoamingChaseTarget(ANPStablePhysicsPawn* InTarget)
{
	if (!HasAuthority() || !bRoamingGhost || !IsValid(InTarget)
		|| InTarget->IsActorBeingDestroyed() || !InTarget->IsPlayerControlled()
		|| InTarget->GetWorld() != GetWorld())
	{
		return false;
	}

	RoamingChaseTarget = InTarget;
	SetActorTickEnabled(true);
	UE_LOG(LogNPGhostFollower, Display,
		TEXT("[GhostTrace] Roaming 추격 대상 지정: Ghost=%s Target=%s Speed=%.1f"),
		*GetNameSafe(this), *GetNameSafe(InTarget), RoamingChaseSpeed);
	return true;
}

void ANPGhostFollowerActor::SetRoamingContactDelay(const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	const float SafeDelay = FMath::IsFinite(DelaySeconds) ? FMath::Max(0.0f, DelaySeconds) : 0.0f;
	RoamingContactEnableWorldTime = World->GetTimeSeconds() + SafeDelay;
	if (SafeDelay > 0.0f)
	{
		UE_LOG(LogNPGhostFollower, Display,
			TEXT("[GhostTrace] Roaming 접촉 유예 시작: Ghost=%s Delay=%.2fs EnableTime=%.2f"),
			*GetNameSafe(this), SafeDelay, RoamingContactEnableWorldTime);
	}
}

void ANPGhostFollowerActor::ConsumeRoamingGhost(const float DestroyDelay)
{
	if (!HasAuthority() || bRoamingGhostConsumed)
	{
		return;
	}

	bRoamingGhostConsumed = true;
	bRoamingGhost = false;
	RoamingChaseTarget.Reset();
	MulticastConsumeRoamingGhost();
	ForceNetUpdate();

	const float SafeDestroyDelay = FMath::IsFinite(DestroyDelay)
		? FMath::Max(0.1f, DestroyDelay) : 0.25f;
	SetLifeSpan(SafeDestroyDelay);
	UE_LOG(LogNPGhostFollower, Display,
		TEXT("[GhostTrace] Roaming 유령 소비 상태 복제: Ghost=%s DestroyDelay=%.2fs"),
		*GetNameSafe(this), SafeDestroyDelay);
}

void ANPGhostFollowerActor::OnRep_RoamingGhostConsumed()
{
	if (bRoamingGhostConsumed)
	{
		ApplyRoamingGhostConsumedState();
	}
}

void ANPGhostFollowerActor::MulticastConsumeRoamingGhost_Implementation()
{
	ApplyRoamingGhostConsumedState();
}

void ANPGhostFollowerActor::ApplyRoamingGhostConsumedState()
{
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	RoamingContactSphere->SetGenerateOverlapEvents(false);
	RoamingContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANPGhostFollowerActor::BeginPlay()
{
	Super::BeginPlay();
	// 같은 BP를 Roaming(복제)과 화면별 Follower(비복제)에 함께 사용합니다.
	// Deferred FinishSpawning의 Blueprint Construction이 bReplicates를 CDO 값으로 되돌릴 수 있으므로
	// FollowTarget이 있는 로컬 Follower는 초기화 완료 시점에 다시 확실히 복제를 끕니다.
	if (FollowTarget.IsValid())
	{
		SetReplicates(false);
		SetReplicateMovement(false);
	}
	UE_LOG(LogNPGhostFollower, Display,
		TEXT("[GhostTrace] BeginPlay 진입: Actor=%s Requested=%d Roaming=%d FollowTarget=%s Authority=%d"),
		*GetNameSafe(this), bRoamingInitializationRequested ? 1 : 0,
		bRoamingGhost ? 1 : 0, *GetNameSafe(FollowTarget.Get()), HasAuthority() ? 1 : 0);
	if (bRoamingInitializationRequested)
	{
		// Blueprint Construction이 복제 UPROPERTY를 CDO 값으로 되돌린 경우를 복원합니다.
		bRoamingGhost = true;
		bRoamingInitializationRequested = false;
	}
	if (IsActorBeingDestroyed())
	{
		return;
	}
	if ((!FollowTarget.IsValid() && !bRoamingGhost)
		|| (GetNetMode() == NM_DedicatedServer && !bRoamingGhost))
	{
		Destroy();
		return;
	}
	// Roaming 유령도 물리 충돌은 전혀 사용하지 않습니다. 접촉은 서버 거리 검사로만 판정합니다.
	// Actor collision을 켜면 BP에서 추가된 메시/콜라이더까지 활성화되어 플레이어와 서로 밀게 됩니다.
	SetActorEnableCollision(false);
	RoamingContactSphere->SetSphereRadius(FMath::Max(1.0f, RoamingContactRadius));
	RoamingContactSphere->SetGenerateOverlapEvents(false);
	RoamingContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Point에 대기 중인 Roaming 유령은 아직 추적 대상이 없습니다.
	// 대상 없는 상태에서 UpdateFollow를 호출하면 RequestFadeOut을 거쳐 즉시 파괴됩니다.
	if (FollowTarget.IsValid())
	{
		UpdateFollow(0.0f, true);
	}
	if (IsActorBeingDestroyed())
	{
		return;
	}
	InitializeFadeMaterials();
	FadeTargetOpacity = FMath::IsFinite(GhostMaxOpacity) ? FMath::Clamp(GhostMaxOpacity, 0.0f, 1.0f) : 0.35f;
	FadeDuration = FMath::IsFinite(GhostFadeInDuration) ? FMath::Max(0.0f, GhostFadeInDuration) : 0.0f;
	bGhostFadeRunning = true;
	ApplyGhostOpacity(0.0f);
	UpdateFade(0.0f);
}

void ANPGhostFollowerActor::HandleRoamingContactBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryHandleRoamingPlayerContact(Cast<ANPStablePhysicsPawn>(OtherActor));
}

void ANPGhostFollowerActor::CheckRoamingPlayerContacts()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !bRoamingGhost || bRoamingGhostConsumed
		|| IsActorBeingDestroyed())
	{
		return;
	}
	if (World->GetTimeSeconds() < RoamingContactEnableWorldTime)
	{
		return;
	}

	const float ContactRadius = FMath::IsFinite(RoamingContactRadius)
		? FMath::Max(1.0f, RoamingContactRadius) : 100.0f;
	const FVector GhostLocation = GetActorLocation();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		ANPStablePhysicsPawn* PlayerPawn = IsValid(PC)
			? Cast<ANPStablePhysicsPawn>(PC->GetPawn()) : nullptr;
		if (IsValid(PlayerPawn) && !PlayerPawn->IsActorBeingDestroyed()
			&& FVector::DistSquared(GhostLocation, PlayerPawn->GetActorLocation())
				<= FMath::Square(ContactRadius))
		{
			TryHandleRoamingPlayerContact(PlayerPawn);
			return;
		}
	}
}

void ANPGhostFollowerActor::TryHandleRoamingPlayerContact(ANPStablePhysicsPawn* PlayerPawn)
{
	if (!HasAuthority() || !bRoamingGhost || bRoamingGhostConsumed || IsActorBeingDestroyed()
		|| !IsValid(PlayerPawn) || PlayerPawn->IsActorBeingDestroyed()
		|| !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	ANPPossessionMapEvent* PossessionEvent = Cast<ANPPossessionMapEvent>(GetOwner());
	if (!IsValid(PossessionEvent))
	{
		return;
	}

	UE_LOG(LogNPGhostFollower, Display,
		TEXT("[GhostTrace] Roaming 유령 플레이어 접촉: Ghost=%s Player=%s Radius=%.1f"),
		*GetNameSafe(this), *GetNameSafe(PlayerPawn), RoamingContactRadius);
	PossessionEvent->HandleRoamingGhostContact(this, PlayerPawn);
}

void ANPGhostFollowerActor::InitializeFadeMaterials()
{
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	bool bMissingOpacityParameter = false;
	for (UMeshComponent* Mesh : Meshes)
	{
		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			UMaterialInterface* Material = Mesh->GetMaterial(Index);
			if (!IsValid(Material))
			{
				continue;
			}
			float ExistingOpacity = 0.0f;
			if (GhostOpacityParameterName.IsNone()
				|| !Material->GetScalarParameterValue(FMaterialParameterInfo(GhostOpacityParameterName), ExistingOpacity))
			{
				bMissingOpacityParameter = true;
				continue;
			}
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
			{
				FadeMaterials.AddUnique(MID);
			}
		}
	}
	if (bMissingOpacityParameter || FadeMaterials.IsEmpty())
	{
		UE_LOG(LogNPGhostFollower, Warning, TEXT("유령 페이드 머티리얼 확인: Actor=%s Parameter=%s. 각 메시 슬롯의 반투명 머티리얼에 해당 Scalar Parameter를 Opacity로 연결하세요."),
			*GetName(), *GhostOpacityParameterName.ToString());
	}
}

float ANPGhostFollowerActor::CalculateFadeOpacity(float StartOpacity, float TargetOpacity, float Elapsed, float Duration)
{
	const float Start = FMath::IsFinite(StartOpacity) ? FMath::Clamp(StartOpacity, 0.0f, 1.0f) : 0.0f;
	const float Target = FMath::IsFinite(TargetOpacity) ? FMath::Clamp(TargetOpacity, 0.0f, 1.0f) : 0.0f;
	if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		return Target;
	}
	const float Progress = FMath::IsFinite(Elapsed) ? FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f) : 1.0f;
	const float SmoothProgress = Progress * Progress * (3.0f - 2.0f * Progress);
	return FMath::Lerp(Start, Target, SmoothProgress);
}

void ANPGhostFollowerActor::ApplyGhostOpacity(float Opacity)
{
	CurrentGhostOpacity = Opacity;
	for (UMaterialInstanceDynamic* MID : FadeMaterials)
	{
		if (IsValid(MID))
		{
			MID->SetScalarParameterValue(GhostOpacityParameterName, Opacity);
		}
	}
}

void ANPGhostFollowerActor::UpdateFade(float DeltaSeconds)
{
	if (!bGhostFadeRunning || IsActorBeingDestroyed())
	{
		return;
	}
	FadeElapsed += FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.0f, DeltaSeconds) : 0.0f;
	ApplyGhostOpacity(CalculateFadeOpacity(FadeStartOpacity, FadeTargetOpacity, FadeElapsed, FadeDuration));
	if (FadeElapsed >= FadeDuration)
	{
		bGhostFadeRunning = false;
		if (bGhostFadingOut)
		{
			Destroy();
		}
	}
}

void ANPGhostFollowerActor::RequestFadeOut()
{
	if (bGhostFadingOut || IsActorBeingDestroyed())
	{
		return;
	}
	bGhostFadingOut = true;
	StopFollowing();
	SetOwner(nullptr);
	FadeStartOpacity = CurrentGhostOpacity;
	FadeTargetOpacity = 0.0f;
	FadeElapsed = 0.0f;
	FadeDuration = FMath::IsFinite(GhostFadeOutDuration) ? FMath::Max(0.0f, GhostFadeOutDuration) : 0.0f;
	if (!HasActorBegunPlay() || FadeDuration <= 0.0f || CurrentGhostOpacity <= 0.0f)
	{
		Destroy();
		return;
	}
	bGhostFadeRunning = true;
	SetActorTickEnabled(true);
	// 외부 BP에서 Tick을 꺼도 이벤트 소유권에서 분리된 유령이 영구히 남지 않게 합니다.
	SetLifeSpan(FadeDuration + 0.1f);
}

FTransform ANPGhostFollowerActor::CalculateFollowTransform(const FVector& TargetLocation,
	const FVector& Forward, float Distance, float Height)
{
	if (TargetLocation.ContainsNaN())
	{
		return FTransform::Identity;
	}
	FVector HorizontalForward(Forward.X, Forward.Y, 0.0);
	if (HorizontalForward.ContainsNaN() || !HorizontalForward.Normalize())
	{
		HorizontalForward = FVector::ForwardVector;
	}
	const float SafeDistance = FMath::IsFinite(Distance) ? FMath::Max(0.0f, Distance) : 150.0f;
	const float SafeHeight = FMath::IsFinite(Height) ? Height : 70.0f;
	return FTransform(HorizontalForward.Rotation(),
		TargetLocation - HorizontalForward * SafeDistance + FVector::UpVector * SafeHeight);
}

void ANPGhostFollowerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bGhostFadingOut && !IsActorBeingDestroyed() && bRoamingGhost && HasAuthority())
	{
		UpdateRoamingChase(DeltaSeconds);
		CheckRoamingPlayerContacts();
	}
	else if (!bGhostFadingOut && !IsActorBeingDestroyed() && FollowTarget.IsValid())
	{
		UpdateFollow(DeltaSeconds, false);
	}
	UpdateFade(DeltaSeconds);
}

void ANPGhostFollowerActor::UpdateRoamingChase(const float DeltaSeconds)
{
	ANPStablePhysicsPawn* Target = RoamingChaseTarget.Get();
	if (!IsValid(Target) || Target->IsActorBeingDestroyed() || !Target->IsPlayerControlled())
	{
		RoamingChaseTarget.Reset();
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	if (CurrentLocation.ContainsNaN() || TargetLocation.ContainsNaN())
	{
		return;
	}

	const float SafeDeltaSeconds = FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.0f, DeltaSeconds) : 0.0f;
	const float SafeSpeed = FMath::IsFinite(RoamingChaseSpeed) ? FMath::Max(0.0f, RoamingChaseSpeed) : 350.0f;
	const FVector NewLocation = FMath::VInterpConstantTo(
		CurrentLocation, TargetLocation, SafeDeltaSeconds, SafeSpeed);
	SetActorLocation(NewLocation, true);

	FVector Direction = TargetLocation - CurrentLocation;
	Direction.Z = 0.0f;
	if (!Direction.ContainsNaN() && Direction.Normalize())
	{
		const float RotationSpeed = FMath::IsFinite(RoamingRotationInterpSpeed)
			? FMath::Max(0.0f, RoamingRotationInterpSpeed) : 8.0f;
		SetActorRotation(FMath::RInterpTo(
			GetActorRotation(), Direction.Rotation(), SafeDeltaSeconds, RotationSpeed));
	}
}

void ANPGhostFollowerActor::UpdateFollow(float DeltaSeconds, bool bSnap)
{
	const ANPStablePhysicsPawn* Target = FollowTarget.Get();
	if (!IsValid(Target) || Target->IsActorBeingDestroyed())
	{
		RequestFadeOut();
		return;
	}
	FVector Forward = Target->GetVisualForwardDirection();
	Forward.Z = 0.0;
	if (!Forward.ContainsNaN() && Forward.Normalize())
	{
		LastHorizontalForward = Forward;
	}
	const FVector TargetLocation = Target->GetActorLocation();
	if (TargetLocation.ContainsNaN())
	{
		return;
	}
	const FTransform Desired = CalculateFollowTransform(TargetLocation, LastHorizontalForward, FollowDistance, HeightOffset);
	const float SafeSnapDistance = FMath::IsFinite(SnapDistance) ? FMath::Max(1.0f, SnapDistance) : 600.0f;
	const float Speed = FMath::IsFinite(FollowInterpSpeed) ? FMath::Max(0.0f, FollowInterpSpeed) : 8.0f;
	if (bSnap || Speed <= 0.0f || FVector::DistSquared(GetActorLocation(), Desired.GetLocation()) > FMath::Square(SafeSnapDistance))
	{
		SetActorLocationAndRotation(Desired.GetLocation(), Desired.GetRotation());
		return;
	}
	SetActorLocationAndRotation(
		FMath::VInterpTo(GetActorLocation(), Desired.GetLocation(), DeltaSeconds, Speed),
		FMath::RInterpTo(GetActorRotation(), Desired.Rotator(), DeltaSeconds, Speed));
}

void ANPGhostFollowerActor::StopFollowing()
{
	if (ANPStablePhysicsPawn* Target = FollowTarget.Get())
	{
		RemoveTickPrerequisiteActor(Target);
	}
	FollowTarget.Reset();
}

void ANPGhostFollowerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogNPGhostFollower, Display,
		TEXT("[GhostTrace] EndPlay: Actor=%s Reason=%d Roaming=%d FollowTarget=%s"),
		*GetNameSafe(this), static_cast<int32>(EndPlayReason), bRoamingGhost ? 1 : 0,
		*GetNameSafe(FollowTarget.Get()));
	StopFollowing();
	RoamingChaseTarget.Reset();
	bGhostFadeRunning = false;
	FadeMaterials.Reset();
	Super::EndPlay(EndPlayReason);
}
