#include "Gameplay/MapEvents/Santa/NPSantaFlightActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

ANPSantaFlightActor::ANPSantaFlightActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	FlightRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FlightRoot"));
	SetRootComponent(FlightRoot);
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(FlightRoot);

	SleighMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SleighMesh"));
	SleighMesh->SetupAttachment(VisualRoot);
	SleighMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SleighMesh->SetGenerateOverlapEvents(false);
	SleighMesh->SetCanEverAffectNavigation(false);
	SantaMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SantaMesh"));
	SantaMesh->SetupAttachment(VisualRoot);
	SantaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SantaMesh->SetGenerateOverlapEvents(false);
	SantaMesh->SetCanEverAffectNavigation(false);
}

void ANPSantaFlightActor::BeginPlay()
{
	Super::BeginPlay();
	UpdateFlight();
}

void ANPSantaFlightActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFlight();
}

void ANPSantaFlightActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPSantaFlightActor, FlightPlan);
}

bool ANPSantaFlightActor::InitializeFlight(const FNPSantaFlightPlan& InPlan)
{
	if (!HasAuthority() || FlightPlan.IsValid() || !InPlan.IsValid())
	{
		return false;
	}
	FlightPlan = InPlan;
	ForceNetUpdate();
	if (HasActorBegunPlay())
	{
		UpdateFlight();
	}
	return true;
}

void ANPSantaFlightActor::OnRep_FlightPlan()
{
	if (HasActorBegunPlay())
	{
		UpdateFlight();
	}
}

bool ANPSantaFlightActor::TryGetServerTime(float& OutTime) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		OutTime = GameState->GetServerWorldTimeSeconds();
		return FMath::IsFinite(OutTime);
	}
	// 클라이언트 로컬 시간으로 대체하면 중도 접속 시 출발점으로 되돌아갈 수 있습니다.
	if (HasAuthority())
	{
		OutTime = World->GetTimeSeconds();
		return FMath::IsFinite(OutTime);
	}
	return false;
}

float ANPSantaFlightActor::GetFlightProgress() const
{
	float ServerTime = 0.0f;
	return TryGetServerTime(ServerTime) ? FlightPlan.GetProgress(ServerTime) : 0.0f;
}

void ANPSantaFlightActor::UpdateFlight()
{
	float ServerTime = 0.0f;
	if (!FlightPlan.IsValid() || !TryGetServerTime(ServerTime))
	{
		// 에디터에서는 외형을 보여주되 게임에서는 유효한 계획/시간을 받을 때까지 숨깁니다.
		VisualRoot->SetVisibility(false, true);
		return;
	}

	const FTransform FlightTransform = FlightPlan.GetTransform(FlightPlan.GetProgress(ServerTime));
	SetActorLocationAndRotation(FlightTransform.GetLocation(), FlightTransform.GetRotation());
	const bool bInFlight = ServerTime >= FlightPlan.StartServerTime
		&& ServerTime < FlightPlan.StartServerTime + FlightPlan.Duration;
	VisualRoot->SetVisibility(bInFlight, true);
	// 수명은 이벤트가 관리합니다. 클라이언트가 독자적으로 액터를 Destroy하지 않습니다.
}
