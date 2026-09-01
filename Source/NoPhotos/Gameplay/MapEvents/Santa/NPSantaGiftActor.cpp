#include "Gameplay/MapEvents/Santa/NPSantaGiftActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPSantaGift, Log, All);

ANPSantaGiftActor::ANPSantaGiftActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->InitBoxExtent(FVector(35.0));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// 선물끼리/유물/플레이어와 충돌하지 않고 월드 바닥과 건물을 검사합니다.
	CollisionBox->SetCollisionObjectType(ECC_PhysicsBody);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionBox->SetGenerateOverlapEvents(false);
	CollisionBox->SetCanEverAffectNavigation(false);
	GrabbableComponent = CreateDefaultSubobject<UGrabbableComponent>(TEXT("GrabbableComponent"));
	GrabbableComponent->SetGrabEnabled(false);
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(CollisionBox);
	ClosedBoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClosedBoxMesh"));
	ClosedBoxMesh->SetupAttachment(VisualRoot);
	OpenBoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OpenBoxMesh"));
	OpenBoxMesh->SetupAttachment(VisualRoot);
	LidPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LidPivot"));
	LidPivot->SetupAttachment(VisualRoot);
	LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
	LidMesh->SetupAttachment(LidPivot);
	for (UStaticMeshComponent* Mesh : {ClosedBoxMesh.Get(), OpenBoxMesh.Get(), LidMesh.Get()})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
	}

	FallingMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("FallingMovement"));
	FallingMovement->SetUpdatedComponent(CollisionBox);
	FallingMovement->Velocity = FVector(0.0, 0.0, -100.0);
	FallingMovement->bInitialVelocityInLocalSpace = false;
	FallingMovement->ProjectileGravityScale = 1.0f;
	FallingMovement->MaxSpeed = 2000.0f;
	FallingMovement->bShouldBounce = false;
	FallingMovement->bRotationFollowsVelocity = false;
	FallingMovement->bForceSubStepping = true;
	FallingMovement->OnProjectileStop.AddDynamic(this, &ThisClass::HandleFallStopped);
}

bool ANPSantaGiftActor::InitializeGift(const TArray<TSubclassOf<ANPBaseRelic>>& InRelicClasses)
{
	if (!HasAuthority() || bInitialized)
	{
		return false;
	}
	for (const TSubclassOf<ANPBaseRelic>& RelicClass : InRelicClasses)
	{
		if (RelicClass && !RelicClass->HasAnyClassFlags(CLASS_Abstract))
		{
			RelicClasses.AddUnique(RelicClass);
		}
	}
	bInitialized = !RelicClasses.IsEmpty();
	return bInitialized;
}

void ANPSantaGiftActor::BeginPlay()
{
	Super::BeginPlay();
	OpeningDuration = FMath::IsFinite(OpeningDuration) ? FMath::Clamp(OpeningDuration, 0.1f, 10.0f) : 1.0f;
	ClosedBoxInitialTransform = ClosedBoxMesh->GetRelativeTransform();
	LidInitialTransform = LidPivot->GetRelativeTransform();
	GrabbableComponent->OnGrabStarted.AddUObject(this, &ThisClass::HandleGrabStarted);
	if (HasAuthority())
	{
		if (!bInitialized)
		{
			UE_LOG(LogNPSantaGift, Warning, TEXT("선물 초기화 누락: Gift=%s. 이벤트가 생성해야 합니다."), *GetName());
			Destroy();
			return;
		}
		const float Timeout = FMath::IsFinite(MaxFallDuration) ? FMath::Max(1.0f, MaxFallDuration) : 45.0f;
		GetWorldTimerManager().SetTimer(FallTimeoutTimer, this, &ThisClass::HandleFallTimeout, Timeout, false);
	}
	else
	{
		// 클라이언트에는 서버 위치 복제만 적용합니다. 중력을 이중 계산하지 않습니다.
		FallingMovement->Deactivate();
		FallingMovement->SetComponentTickEnabled(false);
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	ApplyLandingState();
}

void ANPSantaGiftActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPSantaGiftActor, LandingState);
}

void ANPSantaGiftActor::OnRep_ReplicatedMovement()
{
	if (LandingState.bLanded)
	{
		// 착지 상태보다 늦게 도착한 낙하 위치가 상자를 다시 공중으로 옮기지 않게 합니다.
		SetActorLocationAndRotation(LandingState.Location, LandingState.Rotation);
		return;
	}
	Super::OnRep_ReplicatedMovement();
}

void ANPSantaGiftActor::HandleFallStopped(const FHitResult& Hit)
{
	if (!HasAuthority() || LandingState.bLanded || IsActorBeingDestroyed())
	{
		return;
	}
	const float GroundThreshold = FMath::IsFinite(MinimumGroundNormalZ)
		? FMath::Clamp(MinimumGroundNormalZ, 0.1f, 1.0f) : 0.5f;
	if (!Hit.bBlockingHit || Hit.bStartPenetrating || Hit.ImpactNormal.Z < GroundThreshold)
	{
		UE_LOG(LogNPSantaGift, Warning, TEXT("선물 착지 실패: Gift=%s Hit=%s Normal=%s Penetrating=%d. 벽면/관통에서는 유물을 생성하지 않습니다."),
			*GetName(), *GetNameSafe(Hit.GetActor()), *Hit.ImpactNormal.ToString(), Hit.bStartPenetrating ? 1 : 0);
		Destroy();
		return;
	}
	GetWorldTimerManager().ClearTimer(FallTimeoutTimer);
	LandingState.bLanded = true;
	LandingState.ServerTime = GetServerTime();
	LandingState.Location = GetActorLocation();
	LandingState.Rotation = GetActorRotation();
	ApplyLandingState();
	ForceNetUpdate();
	// 착지는 개봉 조건이 아닙니다. 첫 잡기까지 닫힌 상자로 남습니다.
}

void ANPSantaGiftActor::HandleGrabStarted(UPrimitiveComponent* GrabbedComponent)
{
	if (!HasAuthority() || !LandingState.bLanded || LandingState.bOpening ||
		bOpeningRequested || IsActorBeingDestroyed() || GrabbedComponent != CollisionBox.Get())
	{
		return;
	}
	bOpeningRequested = true;
	// 잡기 등록 콜백이 끝난 다음 틱에 제약/잡기를 해제하고 개봉합니다.
	StartOpeningTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::BeginOpening);
}

void ANPSantaGiftActor::BeginOpening()
{
	if (!HasAuthority() || !LandingState.bLanded || LandingState.bOpening || IsActorBeingDestroyed())
	{
		return;
	}
	FlushNetDormancy();
	LandingState.bOpening = true;
	LandingState.OpeningServerTime = GetServerTime();
	ApplyLandingState();
	if (IsActorBeingDestroyed())
	{
		return;
	}
	ForceNetUpdate();
	GetWorldTimerManager().SetTimer(OpeningTimer, this, &ThisClass::SpawnRelic, OpeningDuration, false);
	const float RemainsLife = FMath::IsFinite(OpenedLifeSpan) ? FMath::Max(0.5f, OpenedLifeSpan) : 3.0f;
	SetLifeSpan(OpeningDuration + RemainsLife);
}

void ANPSantaGiftActor::OnRep_LandingState()
{
	if (HasActorBegunPlay())
	{
		ApplyLandingState();
	}
}

void ANPSantaGiftActor::ApplyLandingState()
{
	GrabbableComponent->SetGrabEnabled(LandingState.bLanded && !LandingState.bOpening);
	if (LandingState.bLanded)
	{
		FallingMovement->Deactivate();
		FallingMovement->SetComponentTickEnabled(false);
		// 기존 잡기 시스템은 PhysicsBody 검색 및 물리 제약을 사용합니다.
		// 상자는 시뮬레이션 없이 착지 위치에 고정하고, 개봉 시 잡기/충돌을 해제합니다.
		CollisionBox->SetSimulatePhysics(false);
		CollisionBox->SetCollisionEnabled(LandingState.bOpening
			? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		SetActorLocationAndRotation(LandingState.Location, LandingState.Rotation);
	}
	UpdateOpeningVisuals();
}

float ANPSantaGiftActor::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

float ANPSantaGiftActor::GetOpeningProgress() const
{
	if (!LandingState.bOpening || (!HasAuthority() && (!GetWorld() || !GetWorld()->GetGameState())))
	{
		return 0.0f;
	}
	return FMath::Clamp((GetServerTime() - LandingState.OpeningServerTime) / FMath::Max(0.1f, OpeningDuration), 0.0f, 1.0f);
}

void ANPSantaGiftActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (LandingState.bLanded)
	{
		UpdateOpeningVisuals();
	}
}

void ANPSantaGiftActor::UpdateOpeningVisuals()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const bool bSeparateLid = OpenBoxMesh->GetStaticMesh() && LidMesh->GetStaticMesh();
	const float Progress = GetOpeningProgress();
	// 닫힌 일체형 메시를 사용하지 않는 경우 본체+뚜껑만으로 낙하 외형도 구성합니다.
	const bool bUseParts = bSeparateLid && (LandingState.bOpening || !ClosedBoxMesh->GetStaticMesh());
	ClosedBoxMesh->SetVisibility(!bUseParts && Progress < 1.0f);
	OpenBoxMesh->SetVisibility(bUseParts);
	LidMesh->SetVisibility(bUseParts);
	if (!LandingState.bLanded)
	{
		return;
	}
	if (!bLandedPresentationStarted)
	{
		bLandedPresentationStarted = true;
		OnGiftLanded();
	}
	if (!LandingState.bOpening)
	{
		return;
	}
	const float Ease = Progress * Progress * (3.0f - 2.0f * Progress);
	if (bSeparateLid)
	{
		LidPivot->SetRelativeLocation(LidInitialTransform.GetLocation() + FVector::UpVector * LidLiftHeight * Ease);
		LidPivot->SetRelativeRotation(LidInitialTransform.GetRotation()
			* FQuat::Slerp(FQuat::Identity, LidOpenRotation.Quaternion(), Ease));
	}
	else
	{
		// 일체형 메시만 있으면 팝업/축소 후 사라지는 대체 연출을 제공합니다.
		const float Scale = Progress < 0.35f ? FMath::Lerp(1.0f, 1.15f, Progress / 0.35f)
			: FMath::Lerp(1.15f, 0.0f, (Progress - 0.35f) / 0.65f);
		ClosedBoxMesh->SetRelativeScale3D(ClosedBoxInitialTransform.GetScale3D() * Scale);
	}
	if (Progress >= 1.0f && !bOpenedPresentationStarted)
	{
		bOpenedPresentationStarted = true;
		OnGiftOpened();
	}
}

void ANPSantaGiftActor::SpawnRelic()
{
	if (!HasAuthority() || !LandingState.bOpening || bRelicSpawnAttempted || RelicClasses.IsEmpty())
	{
		return;
	}
	bRelicSpawnAttempted = true;
	const TSubclassOf<ANPBaseRelic> RelicClass = RelicClasses[FMath::RandRange(0, RelicClasses.Num() - 1)];
	FActorSpawnParameters Params;
	// 이벤트/선물 상자의 제거와 무관하게 월드에 남는 유물입니다. Owner도 상자로 지정하지 않습니다.
	Params.OverrideLevel = GetWorld()->PersistentLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	const float SpawnHeight = FMath::IsFinite(RelicSpawnHeight) ? FMath::Max(0.0f, RelicSpawnHeight) : 100.0f;
	FVector SpawnLocation = LandingState.Location + FVector::UpVector * SpawnHeight;
	ANPBaseRelic* Relic = nullptr;
	// Retry the SAME reward at higher positions if the player or floor blocks the opening.
	// Keep collision rejection enabled: AlwaysSpawn could trap physics relics inside the level.
	const ANPBaseRelic* Defaults = RelicClass.GetDefaultObject();
	for (int32 Attempt = 0; Attempt < 6 && !IsValid(Relic); ++Attempt)
	{
		SpawnLocation = LandingState.Location + FVector::UpVector * (SpawnHeight + Attempt * 50.0f);
		if (Defaults && GetWorld()->EncroachingBlockingGeometry(Defaults, SpawnLocation, LandingState.Rotation))
		{
			continue;
		}
		Relic = GetWorld()->SpawnActor<ANPBaseRelic>(RelicClass, SpawnLocation, LandingState.Rotation, Params);
	}
	if (!IsValid(Relic))
	{
		UE_LOG(LogNPSantaGift, Warning, TEXT("선물 유물 생성 실패: Gift=%s Class=%s Location=%s. 유물 크기/충돌과 RelicSpawnHeight를 확인하세요."),
			*GetName(), *GetNameSafe(RelicClass.Get()), *SpawnLocation.ToString());
		return;
	}
	Relic->SetReplicates(true);
	Relic->SetReplicateMovement(true);
	Relic->SetUnlocked(true);
	const float PopSpeed = FMath::IsFinite(RelicPopUpSpeed) ? FMath::Max(0.0f, RelicPopUpSpeed) : 150.0f;
	if (!Relic->ReleaseWithVelocityImpulse(FVector::UpVector * PopSpeed))
	{
		UE_LOG(LogNPSantaGift, Warning, TEXT("선물 유물 물리 활성화 실패: Relic=%s. 유물 BP의 Physics Collision/Simple Collision을 확인하세요."), *GetNameSafe(Relic));
	}
	UE_LOG(LogNPSantaGift, Log, TEXT("선물 개봉/랜덤 유물 생성: Gift=%s Relic=%s"), *GetName(), *GetNameSafe(Relic));
}

void ANPSantaGiftActor::HandleFallTimeout()
{
	UE_LOG(LogNPSantaGift, Warning, TEXT("선물 낙하 시간 초과: Gift=%s. 경로 아래 바닥 충돌/높이를 확인하세요."), *GetName());
	Destroy();
}

void ANPSantaGiftActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OpeningTimer);
	GetWorldTimerManager().ClearTimer(StartOpeningTimer);
	GetWorldTimerManager().ClearTimer(FallTimeoutTimer);
	GrabbableComponent->OnGrabStarted.RemoveAll(this);
	GrabbableComponent->SetGrabEnabled(false);
	Super::EndPlay(EndPlayReason);
}
