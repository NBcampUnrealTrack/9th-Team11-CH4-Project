#include "NPEventSpotlight.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NPSpotlightMapEvent.h"
#include "UObject/ConstructorHelpers.h"

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
	Spotlight->SetVolumetricScatteringIntensity(0.0f);
	Spotlight->SetVisibility(false);

	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	BeamMesh->SetupAttachment(Spotlight);
	BeamMesh->SetRelativeScale3D(FVector(4.038712, 4.038712, 16.446335));
	BeamMesh->SetMobility(EComponentMobility::Movable);
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetGenerateOverlapEvents(false);
	BeamMesh->SetCanEverAffectNavigation(false);
	BeamMesh->SetCastShadow(false);
	BeamMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamAsset(
		TEXT("/Game/NoPhotos/Blueprints/MapDesign/Light/S_SpotLight.S_SpotLight"));
	BeamMesh->SetStaticMesh(BeamAsset.Object);
}

void ANPEventSpotlight::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateBeamGeometry();
}

void ANPEventSpotlight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPEventSpotlight, BeamScaleMultiplier);
}

void ANPEventSpotlight::SetBeamScaleMultiplier(const FVector& Multiplier)
{
	if (!HasAuthority())
	{
		return;
	}
	BeamScaleMultiplier = Multiplier;
	OnRep_BeamScaleMultiplier();
	ForceNetUpdate();
}

void ANPEventSpotlight::OnRep_BeamScaleMultiplier()
{
	if (HasActorBegunPlay())
	{
		BeamMesh->SetRelativeScale3D(InitialBeamScale * BeamScaleMultiplier);
		UpdateBeamGeometry();
	}
}

void ANPEventSpotlight::UpdateBeamGeometry()
{
	const UStaticMesh* Mesh = BeamMesh->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector Scale = BeamMesh->GetRelativeScale3D();
	// S_SpotLight의 위쪽 끝을 광원에 맞추고 아래 방향을 조명의 로컬 +X축으로 돌립니다.
	const FQuat Rotation = FRotator(90.0f, 0.0f, 0.0f).Quaternion();
	const FVector MeshTip(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z + Bounds.BoxExtent.Z);
	BeamMesh->SetRelativeTransform(FTransform(Rotation, -Rotation.RotateVector(MeshTip * Scale), Scale));
}

void ANPEventSpotlight::BeginPlay()
{
	Super::BeginPlay();
	InitialLightRotation = Spotlight->GetRelativeRotation().Quaternion();
	InitialLightIntensity = Spotlight->Intensity;
	Spotlight->SetVolumetricScatteringIntensity(0.0f);
	Spotlight->SetIntensity(0.0f);
	Spotlight->SetVisibility(false);
	BeamMesh->SetVisibility(false);
	if (GetNetMode() != NM_DedicatedServer)
	{
		BeamMaterial = BeamMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (BeamMaterial)
		{
			BeamMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		}
	}
	InitialBeamScale = BeamMesh->GetRelativeScale3D();
	BeamMesh->SetRelativeScale3D(InitialBeamScale * BeamScaleMultiplier);
	UpdateBeamGeometry();
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
	UpdateBeam(
		bActive ? Now - Event->GetSpotlightCycle().StartServerWorldTime : 0.0f,
		bActive ? Event->GetSpotlightCycle().EndServerWorldTime - Now : 0.0f,
		bActive);
}

void ANPEventSpotlight::UpdateBeam(
	const float ElapsedSeconds, const float RemainingSeconds, const bool bActive)
{
	Spotlight->SetVisibility(bActive);
	BeamMesh->SetVisibility(bActive && GetNetMode() != NM_DedicatedServer);
	if (!bActive)
	{
		Spotlight->SetIntensity(0.0f);
		if (BeamMaterial)
		{
			BeamMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		}
		return;
	}

	const float FadeAlpha = FadeDuration > 0.0f
		? FMath::SmoothStep(0.0f, 1.0f, FMath::Min(ElapsedSeconds, RemainingSeconds) / FadeDuration)
		: 1.0f;
	Spotlight->SetIntensity(InitialLightIntensity * FadeAlpha);
	if (BeamMaterial)
	{
		BeamMaterial->SetScalarParameterValue(TEXT("Opacity"), FadeAlpha);
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
