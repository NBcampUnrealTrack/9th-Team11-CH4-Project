#include "NPJumpPad.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/AbilitySystem/Effects/NPLavaGameplayEffect.h"

ANPJumpPad::ANPJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);

	LaunchVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("LaunchVolume"));
	LaunchVolume->SetupAttachment(SceneRoot);
	LaunchVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	LaunchVolume->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	LaunchVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LaunchVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	LaunchVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	LaunchVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	LaunchVolume->SetGenerateOverlapEvents(true);
}

void ANPJumpPad::BeginPlay()
{
	Super::BeginPlay();
	LaunchVolume->OnComponentBeginOverlap.AddDynamic(this, &ANPJumpPad::HandleLaunchOverlap);
}

void ANPJumpPad::HandleLaunchOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!HasAuthority() || !IsValid(Pawn) || IsOnCooldown(Pawn))
	{
		return;
	}

	if (!LaunchPawn(Pawn))
	{
		return;
	}

	RecordLaunch(Pawn);
}

bool ANPJumpPad::IsOnCooldown(const APawn* Pawn) const
{
	if (!IsValid(Pawn) || !GetWorld())
	{
		return true;
	}

	const TWeakObjectPtr<APawn> PawnKey(const_cast<APawn*>(Pawn));
	const double* LastLaunchTime = LastLaunchTimes.Find(PawnKey);
	return LastLaunchTime && GetWorld()->GetTimeSeconds() < *LastLaunchTime + Cooldown;
}

void ANPJumpPad::RecordLaunch(APawn* Pawn)
{
	if (IsValid(Pawn) && GetWorld())
	{
		const TWeakObjectPtr<APawn> PawnKey(Pawn);
		LastLaunchTimes.FindOrAdd(PawnKey) = GetWorld()->GetTimeSeconds();
	}
}

bool ANPJumpPad::LaunchPawn(APawn* Pawn) const
{
	if (!IsValid(Pawn))
	{
		return false;
	}
	UNPAbilitySystemComponent* AbilitySystem = Pawn->FindComponentByClass<UNPAbilitySystemComponent>();
	if (!IsValid(AbilitySystem) || AbilitySystem->GetAvatarActor() != Pawn)
	{
		return false;
	}
	FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
		UNPLavaGameplayEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	const float Duration = AbilitySystem->GetLavaBurnDuration();
	Spec.Data->SetDuration(FMath::IsFinite(Duration) ? FMath::Max(0.01f, Duration) : 0.5f, true);
	return AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).IsValid();
}
