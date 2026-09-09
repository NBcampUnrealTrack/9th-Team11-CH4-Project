#include "Gameplay/Relic/NPBaseRelic.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/Structs/NPRelicData.h"
#include "Engine/CollisionProfile.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/Components/NPRelicOwnershipComponent.h"
#include "Net/UnrealNetwork.h"

const FName ANPBaseRelic::RelicComponentName(TEXT("RelicMesh"));

ANPBaseRelic::ANPBaseRelic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);

	RelicMesh = CreateDefaultSubobject<
		UPrimitiveComponent,
		UStaticMeshComponent>(RelicComponentName);
	SetRootComponent(RelicMesh);
	RelicMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	RelicMesh->SetSimulatePhysics(false);

	GrabbableComponent = CreateDefaultSubobject<UGrabbableComponent>(TEXT("GrabbableComponent"));
	OwnershipComponent = CreateDefaultSubobject<UNPRelicOwnershipComponent>(TEXT("OwnershipComponent"));
}

void ANPBaseRelic::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bStartWithPhysicsEnabled)
	{
		ReleaseFromDisplay();
	}
	else
	{
		OnRep_IsDisplayed();
	}
	OnRep_IsReturned();
	GrabbableComponent->OnGrabStarted.AddUObject(
		this,
		&ANPBaseRelic::HandleGrabStarted);
}

void ANPBaseRelic::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPBaseRelic, bIsDisplayed);
	DOREPLIFETIME(ANPBaseRelic, bIsUnlocked);
	DOREPLIFETIME(ANPBaseRelic, bIsReturned);
	DOREPLIFETIME(ANPBaseRelic, AccumulatedPhotoPenalty);
	DOREPLIFETIME(ANPBaseRelic, AccumulatedPriceBonus);
}

FVector ANPBaseRelic::GetRelicWorldLocation() const
{
	return IsValid(RelicMesh)
		? RelicMesh->GetComponentLocation()
		: GetActorLocation();
}

int32 ANPBaseRelic::GetBasePrice() const
{
	const FNPRelicTableRow* Data = GetRelicTableData();
	return Data ? FMath::Max(0, Data->Price) : 0;
}

int32 ANPBaseRelic::GetCurrentPrice() const
{
	const double CurrentPrice = FMath::Clamp(
		GetBasePrice() + AccumulatedPriceBonus - AccumulatedPhotoPenalty,
		0.0, static_cast<double>(MAX_int32));
	return static_cast<int32>(FMath::RoundToInt64(CurrentPrice));
}

const FNPRelicTableRow* ANPBaseRelic::GetRelicTableData() const
{
	return RelicTableData.GetRow<FNPRelicTableRow>(TEXT("GetRelicTableData"));
}

void ANPBaseRelic::SetRelicTableData(
	const FDataTableRowHandle& InRelicTableData)
{
	RelicTableData = InRelicTableData;
}

void ANPBaseRelic::SetUnlocked(bool bUnlocked)
{
	if (!HasAuthority() || bIsUnlocked == bUnlocked)
	{
		return;
	}

	bIsUnlocked = bUnlocked;
	if (bIsUnlocked && GrabbableComponent->IsGrabbed())
	{
		ReleaseFromDisplay();
	}
	ForceNetUpdate();
}

void ANPBaseRelic::OnRep_IsDisplayed()
{
	const ECollisionEnabled::Type CollisionEnabled = RelicMesh->GetCollisionEnabled();

	const bool bHasPhysicsCollision =
		CollisionEnabled == ECollisionEnabled::QueryAndPhysics
		|| CollisionEnabled == ECollisionEnabled::PhysicsOnly;

	const bool bCanSimulate = RelicMesh->CanEditSimulatePhysics()
		&& bHasPhysicsCollision;
	
	RelicMesh->SetSimulatePhysics(!bIsDisplayed && !bIsReturned && bCanSimulate);
}

bool ANPBaseRelic::AddPhotoPenaltyCapture(
	const float PenaltyRatePerCapture)
{
	const int32 BasePrice = GetBasePrice();
	if (!HasAuthority() || bIsReturned || BasePrice <= 0
		|| !FMath::IsFinite(PenaltyRatePerCapture)
		|| PenaltyRatePerCapture <= 0.0f
		|| AccumulatedPhotoPenalty >= BasePrice)
	{
		return false;
	}

	const int32 PreviousPenalty = AccumulatedPhotoPenalty;
	++SuccessfulEvidenceCaptureCount;

	// 매회 반올림한 값을 더하지 않고 누적 비율을 한 번 반올림하여
	// 소수점 오차가 쌓이거나 10회 전에 100%를 초과하지 않게 합니다.
	const double AccumulatedPenalty =
		static_cast<double>(BasePrice)
		* static_cast<double>(PenaltyRatePerCapture)
		* static_cast<double>(SuccessfulEvidenceCaptureCount);
	AccumulatedPhotoPenalty = FMath::Clamp(
		FMath::RoundToInt(AccumulatedPenalty),
		0,
		BasePrice);
	if (AccumulatedPhotoPenalty == PreviousPenalty)
	{
		return false;
	}

	// 서버의 Listen UI도 즉시 갱신하고, 이후 복제로 각 클라이언트 UI를 갱신합니다.
	OnRep_AccumulatedPhotoPenalty();
	ForceNetUpdate();
	return true;
}

bool ANPBaseRelic::AddPriceBonus(const double BonusRate)
{
	if (!HasAuthority() || bIsReturned || !FMath::IsFinite(BonusRate) || BonusRate <= 0.0)
	{
		return false;
	}

	const double NewBonus = FMath::Min(
		AccumulatedPriceBonus + GetBasePrice() * BonusRate,
		static_cast<double>(MAX_int32));
	if (NewBonus == AccumulatedPriceBonus)
	{
		return false;
	}

	FlushNetDormancy();
	AccumulatedPriceBonus = NewBonus;
	OnRep_AccumulatedPriceBonus();
	ForceNetUpdate();
	return true;
}

bool ANPBaseRelic::TryMarkReturned()
{
	if (!HasAuthority() || bIsReturned)
	{
		return false;
	}

	FlushNetDormancy();
	bIsReturned = true;
	SetReplicateMovement(false);
	OnRep_IsReturned();
	ForceNetUpdate();
	SetNetDormancy(DORM_DormantAll);
	return true;
}

bool ANPBaseRelic::ReleaseWithVelocityImpulse(const FVector VelocityImpulse)
{
	if (!HasAuthority() || bIsReturned || !IsValid(RelicMesh))
	{
		return false;
	}

	ReleaseFromDisplay();
	if (!RelicMesh->IsSimulatingPhysics())
	{
		return false;
	}

	RelicMesh->WakeAllRigidBodies();
	RelicMesh->AddImpulse(VelocityImpulse, NAME_None, true);
	ForceNetUpdate();
	return true;
}

void ANPBaseRelic::OnRep_IsReturned()
{
	if (!bIsReturned)
	{
		return;
	}

	GrabbableComponent->SetGrabEnabled(false);
	RelicMesh->SetSimulatePhysics(false);
	RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RelicMesh->SetVisibility(false, true);
}

void ANPBaseRelic::OnRep_AccumulatedPhotoPenalty()
{
	OnRelicValueChanged.Broadcast(this);
}

void ANPBaseRelic::OnRep_AccumulatedPriceBonus()
{
	OnRelicValueChanged.Broadcast(this);
}

void ANPBaseRelic::ReleaseFromDisplay()
{
	if (!HasAuthority() || !bIsDisplayed)
	{
		return;
	}

	bIsDisplayed = false;
	OnReleasedFromDisplay.Broadcast(this);
	OnRep_IsDisplayed();
	ForceNetUpdate();
}

void ANPBaseRelic::HandleGrabStarted(UPrimitiveComponent*)
{
	if (bIsUnlocked)
	{
		ReleaseFromDisplay();
	}
}
