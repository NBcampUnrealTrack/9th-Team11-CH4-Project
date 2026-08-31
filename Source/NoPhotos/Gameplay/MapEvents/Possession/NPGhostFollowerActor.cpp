#include "NPGhostFollowerActor.h"

#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameters.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPGhostFollower, Log, All);

ANPGhostFollowerActor::ANPGhostFollowerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;
	SetReplicateMovement(false);
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
}

bool ANPGhostFollowerActor::InitializeFollower(ANPStablePhysicsPawn* InTarget)
{
	if (!IsValid(InTarget) || InTarget->IsActorBeingDestroyed()
		|| InTarget->GetWorld() != GetWorld() || FollowTarget.IsValid())
	{
		return false;
	}
	FollowTarget = InTarget;
	SetReplicates(false);
	SetReplicateMovement(false);
	AddTickPrerequisiteActor(InTarget);
	UpdateFollow(0.0f, true);
	return true;
}

void ANPGhostFollowerActor::BeginPlay()
{
	Super::BeginPlay();
	if (IsActorBeingDestroyed())
	{
		return;
	}
	if (!FollowTarget.IsValid() || GetNetMode() == NM_DedicatedServer)
	{
		Destroy();
		return;
	}
	// BP에서 추가한 외형도 캐릭터나 트레이스의 장애물이 되지 않게 합니다.
	SetActorEnableCollision(false);
	UpdateFollow(0.0f, true);
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
	if (!bGhostFadingOut && !IsActorBeingDestroyed())
	{
		UpdateFollow(DeltaSeconds, false);
	}
	UpdateFade(DeltaSeconds);
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
	StopFollowing();
	bGhostFadeRunning = false;
	FadeMaterials.Reset();
	Super::EndPlay(EndPlayReason);
}
