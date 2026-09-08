#include "NPEventSpotlight.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "NPSpotlightMapEvent.h"

ANPEventSpotlight::ANPEventSpotlight()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SpotlightRoot")));

	Spotlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Spotlight"));
	Spotlight->SetupAttachment(GetRootComponent());
	Spotlight->SetMobility(EComponentMobility::Movable);
	Spotlight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Spotlight->SetAttenuationRadius(2500.0f);
	Spotlight->SetInnerConeAngle(18.0f);
	Spotlight->SetOuterConeAngle(22.0f);
	Spotlight->SetIntensity(100000.0f);
	Spotlight->SetVisibility(false);
}

void ANPEventSpotlight::BeginPlay()
{
	Super::BeginPlay();
	InitialLightRotation = Spotlight->GetRelativeRotation().Quaternion();
	Spotlight->SetVisibility(false);
	SetActorTickEnabled(!HasAuthority());
}

void ANPEventSpotlight::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 서버는 이벤트 Tick에서 판정 직전에 같은 회전을 적용합니다.
	if (HasAuthority())
	{
		return;
	}

	const ANPSpotlightMapEvent* Event = Cast<ANPSpotlightMapEvent>(GetOwner());
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const bool bActive = IsValid(Event) && Event->IsEventActive()
		&& Event->GetSpotlightCycle().ActiveSpotlight == this
		&& Now < Event->GetSpotlightCycle().EndServerWorldTime;
	UpdateBeam(bActive ? Now - Event->GetSpotlightCycle().StartServerWorldTime : 0.0f, bActive);
}

void ANPEventSpotlight::UpdateBeam(const float ElapsedSeconds, const bool bActive)
{
	Spotlight->SetVisibility(bActive);
	if (!bActive)
	{
		return;
	}

	const float Angle = FMath::DegreesToRadians(SweepAngle)
		* FMath::Sin(2.0f * PI * FMath::Max(0.0f, ElapsedSeconds) / FMath::Max(0.1f, SweepPeriod));
	Spotlight->SetRelativeRotation(FQuat(FVector::ForwardVector, Angle) * InitialLightRotation);
}

bool ANPEventSpotlight::IsIlluminatingPawn(
	const ANPStablePhysicsPawn* Pawn, const AActor* HeldRelic) const
{
	const UPrimitiveComponent* PhysicsBody = IsValid(Pawn)
		? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	if (!PhysicsBody || !Spotlight->AffectsBounds(PhysicsBody->Bounds))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpotlightOcclusion), false, this);
	QueryParams.AddIgnoredActor(Pawn);
	QueryParams.AddIgnoredActor(HeldRelic);
	return !GetWorld()->LineTraceTestByChannel(
		Spotlight->GetComponentLocation(), PhysicsBody->Bounds.Origin, ECC_Visibility, QueryParams);
}
