#include "Gameplay/Map/Square/NPSquareEntranceGimmick.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Interaction/Components/GrabbableComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "Net/UnrealNetwork.h"

ANPSquareEntranceGimmick::ANPSquareEntranceGimmick()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	PushOutDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("PushOutDirection"));
	PushOutDirection->SetupAttachment(RootComponent);
	PushOutDirection->SetHiddenInGame(true);
	EntranceBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("EntranceBarrier"));
	FallingAreaBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("FallingAreaBarrier"));
	for (UBoxComponent* Barrier : { EntranceBarrier.Get(), FallingAreaBarrier.Get() })
	{
		Barrier->SetupAttachment(RootComponent);
		Barrier->SetBoxExtent(FVector(100.0f));
		Barrier->SetCollisionObjectType(ECC_WorldStatic);
		Barrier->SetCollisionResponseToAllChannels(ECR_Block);
		Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Barrier->SetGenerateOverlapEvents(false);
		Barrier->SetHiddenInGame(true);
	}
}

void ANPSquareEntranceGimmick::BeginPlay()
{
	Super::BeginPlay();
	TSet<AStaticMeshActor*> UniqueBlocks;
	Blocks.RemoveAll([&UniqueBlocks](AStaticMeshActor* Block)
	{
		if (!IsValid(Block) || UniqueBlocks.Contains(Block))
		{
			return true;
		}
		UniqueBlocks.Add(Block);
		return false;
	});
	for (AStaticMeshActor* Block : Blocks)
	{
		FinalTransforms.Add(Block->GetActorTransform());
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Block->GetStaticMeshComponent()->SetSimulatePhysics(false);
		UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(this);
		Visual->SetStaticMesh(Block->GetStaticMeshComponent()->GetStaticMesh());
		for (int32 MaterialIndex = 0; MaterialIndex < Block->GetStaticMeshComponent()->GetNumMaterials(); ++MaterialIndex)
		{
			Visual->SetMaterial(MaterialIndex, Block->GetStaticMeshComponent()->GetMaterial(MaterialIndex));
		}
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetGenerateOverlapEvents(false);
		Visual->SetMobility(EComponentMobility::Movable);
		Visual->SetHiddenInGame(true);
		Visual->RegisterComponent();
		DisappearVisuals.Add(Visual);
	}
	bInitialized = true;
	ApplyState();
	if (HasAuthority() && IsValid(GoldenApple) && !Blocks.IsEmpty())
	{
		Grabbable = GoldenApple->FindComponentByClass<UGrabbableComponent>();
		if (Grabbable.IsValid())
		{
			Grabbable->OnActiveGrabCountChanged.AddUObject(
				this, &ANPSquareEntranceGimmick::HandleGrabCountChanged);
			HandleGrabCountChanged(Grabbable->GetActiveGrabCount());
		}
	}
}

void ANPSquareEntranceGimmick::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grabbable.IsValid())
	{
		Grabbable->OnActiveGrabCountChanged.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ANPSquareEntranceGimmick::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPSquareEntranceGimmick, State);
}

void ANPSquareEntranceGimmick::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		if (Grabbable.IsValid() && Grabbable->GetActiveGrabCount() > 0
			&& (State.Phase == ENPSquareEntrancePhase::Idle || State.Phase == ENPSquareEntrancePhase::Disappearing))
		{
			HandleGrabCountChanged(Grabbable->GetActiveGrabCount());
		}
		if (!Grabbable.IsValid() && State.Phase != ENPSquareEntrancePhase::Idle)
		{
			HandleGrabCountChanged(0);
		}
		const float Elapsed = GetServerTime() - State.StartTime;
		if (State.Phase == ENPSquareEntrancePhase::Falling && Elapsed >= GetSequenceDuration(true))
		{
			SetPhase(ENPSquareEntrancePhase::Stacked);
		}
		else if (State.Phase == ENPSquareEntrancePhase::Disappearing && Elapsed >= GetSequenceDuration(false))
		{
			SetPhase(ENPSquareEntrancePhase::Idle);
		}
	}
	if (State.Phase == ENPSquareEntrancePhase::Falling
		|| State.Phase == ENPSquareEntrancePhase::Disappearing)
	{
		ApplyState();
	}
}

void ANPSquareEntranceGimmick::HandleGrabCountChanged(int32 Count)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Count > 0)
	{
		if (State.Phase == ENPSquareEntrancePhase::Idle || State.Phase == ENPSquareEntrancePhase::Disappearing)
		{
			SetPhase(ENPSquareEntrancePhase::Falling);
		}
	}
	else if (State.Phase == ENPSquareEntrancePhase::Stacked
		|| (State.Phase == ENPSquareEntrancePhase::Falling
			&& GetServerTime() - State.StartTime >= GetSequenceDuration(true)))
	{
		SetPhase(ENPSquareEntrancePhase::Disappearing);
	}
	else if (State.Phase == ENPSquareEntrancePhase::Falling)
	{
		SetPhase(ENPSquareEntrancePhase::Idle);
	}
}

void ANPSquareEntranceGimmick::SetPhase(ENPSquareEntrancePhase Phase)
{
	if (Phase == ENPSquareEntrancePhase::Falling && !PushPlayersOutsideBarriers())
	{
		return;
	}
	State.Phase = Phase;
	State.StartTime = GetServerTime();
	ApplyState();
	ForceNetUpdate();
}

void ANPSquareEntranceGimmick::OnRep_State()
{
	ApplyState();
}

void ANPSquareEntranceGimmick::ApplyState()
{
	if (!bInitialized)
	{
		return;
	}
	const bool bIdle = State.Phase == ENPSquareEntrancePhase::Idle;
	const bool bFalling = State.Phase == ENPSquareEntrancePhase::Falling;
	const bool bStacked = State.Phase == ENPSquareEntrancePhase::Stacked;
	const bool bDisappearing = State.Phase == ENPSquareEntrancePhase::Disappearing;
	if (bFalling && FallingAreaBarrier->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !PushPlayersOutsideBarriers())
	{
		return;
	}
	// 낙하 보호벽은 제거 중 재활성화하지 않는다. 발판 위 플레이어를 가두지 않기 위함이다.
	if (!bIdle)
	{
		EntranceBarrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (bFalling)
	{
		FallingAreaBarrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	const float Elapsed = FMath::Max(0.0f, GetServerTime() - State.StartTime);
	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		AStaticMeshActor* Block = Blocks[Index];
		if (!IsValid(Block))
		{
			continue;
		}
		FTransform Transform = FinalTransforms[Index];
		bool bVisible = !bIdle;
		if (bFalling)
		{
			const float LocalTime = Elapsed - Index * FMath::Max(0.0f, FallInterval);
			const float Alpha = FMath::Clamp(LocalTime / FMath::Max(0.01f, FallDuration), 0.0f, 1.0f);
			Transform.AddToTranslation(FVector(0.0f, 0.0f, FMath::Max(0.0f, FallHeight) * (1.0f - Alpha * Alpha)));
			bVisible = LocalTime >= 0.0f;
		}
		else if (State.Phase == ENPSquareEntrancePhase::Disappearing)
		{
			const float LocalTime = Elapsed - (Blocks.Num() - 1 - Index) * FMath::Max(0.0f, DisappearInterval);
			const float Alpha = FMath::Clamp(LocalTime / FMath::Max(0.01f, DisappearDuration), 0.0f, 1.0f);
			Transform.SetScale3D(Transform.GetScale3D() * FMath::Max(0.001f, 1.0f - Alpha));
			bVisible = Alpha < 1.0f;
		}
		// 원본은 제거 완료까지 같은 크기의 발판으로 유지하고 별도 메시만 축소한다.
		DisappearVisuals[Index]->SetWorldTransform(Transform);
		DisappearVisuals[Index]->SetHiddenInGame(!bDisappearing || !bVisible);
		const FTransform& BlockTransform = bDisappearing ? FinalTransforms[Index] : Transform;
		if (!Block->GetActorTransform().Equals(BlockTransform))
		{
			Block->SetActorTransform(BlockTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Block->SetActorHiddenInGame(bDisappearing || !bVisible);
		Block->SetActorEnableCollision(bStacked || bDisappearing);
	}
	if (!bFalling)
	{
		FallingAreaBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (bIdle)
	{
		EntranceBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool ANPSquareEntranceGimmick::PushPlayersOutsideBarriers()
{
	const FVector Direction = PushOutDirection->GetForwardVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	// 두 벽의 합집합 바깥으로 이동시켜 다른 차단벽 안에 다시 들어가지 않게 한다.
	const FBox BarrierBounds = EntranceBarrier->Bounds.GetBox() + FallingAreaBarrier->Bounds.GetBox();
	bool bAllClear = true;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if ((!HasAuthority() && !Pawn->IsLocallyControlled()) || !Pawn->IsPlayerControlled())
		{
			continue;
		}
		UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
		if (!Body || !Body->IsCollisionEnabled())
		{
			continue;
		}
		const FBox PawnBounds = Body->Bounds.GetBox();
		if (!PawnBounds.Intersect(EntranceBarrier->Bounds.GetBox())
			&& !PawnBounds.Intersect(FallingAreaBarrier->Bounds.GetBox()))
		{
			continue;
		}
		const FVector Center = PawnBounds.GetCenter();
		const FVector Extent = PawnBounds.GetExtent();
		const FBox ExpandedBounds = BarrierBounds.ExpandBy(Extent + FVector(FMath::Max(1.0f, PushOutPadding)));
		const float Distance = FVector::DotProduct(ExpandedBounds.GetCenter() - Center, Direction)
			+ FVector::DotProduct(ExpandedBounds.GetExtent(), Direction.GetAbs());
		const FVector Candidate = Center + Direction * FMath::Max(0.0f, Distance);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Pawn);
		QueryParams.AddIgnoredActor(this);
		if (GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity,
			Body->GetCollisionObjectType(), FCollisionShape::MakeBox(Extent), QueryParams,
			FCollisionResponseParams(Body->GetCollisionResponseToChannels())))
		{
			bAllClear = false;
			continue;
		}
		const bool bMoved = Pawn->SetActorLocation(Pawn->GetActorLocation() + Candidate - Center,
			false, nullptr, ETeleportType::TeleportPhysics);
		if (bMoved)
		{
			Pawn->ForceNetUpdate();
		}
		bAllClear &= bMoved;
	}
	return bAllClear;
}

float ANPSquareEntranceGimmick::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

float ANPSquareEntranceGimmick::GetSequenceDuration(bool bFalling) const
{
	return FMath::Max(0.01f, bFalling ? FallDuration : DisappearDuration)
		+ FMath::Max(0, Blocks.Num() - 1) * FMath::Max(0.0f, bFalling ? FallInterval : DisappearInterval);
}
