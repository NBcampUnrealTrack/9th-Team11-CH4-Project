#include "Gameplay/Character/NPStatusVisualManager.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPControlReversalGameplayCue.h"
#include "Gameplay/AbilitySystem/GameplayCue/NPPhotoStunGameplayCue.h"
#include "Gameplay/Character/NPLeaderCrown.h"

ANPStatusVisualManager::ANPStatusVisualManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	AppearCurve.GetRichCurve()->AddKey(0.0f, 0.0f);
	AppearCurve.GetRichCurve()->AddKey(0.15f, 0.8f);
	AppearCurve.GetRichCurve()->AddKey(0.25f, 1.0f);
	DisappearCurve.GetRichCurve()->AddKey(0.0f, 1.0f);
	DisappearCurve.GetRichCurve()->AddKey(0.1f, 0.6f);
	DisappearCurve.GetRichCurve()->AddKey(0.2f, 0.0f);
}

void ANPStatusVisualManager::InitializeLeaderCrown(UChildActorComponent* InLeaderCrown)
{
	LeaderCrown = InLeaderCrown;
	ApplyVisualScale(ENPStatusVisualType::Leader, 0.0f);
	FinishDisappear(ENPStatusVisualType::Leader);
}

void ANPStatusVisualManager::RequestControlReversalVisual(
	ANPControlReversalGameplayCue* Visual, bool bActive)
{
	if (bActive)
	{
		if (ControlReversalVisual.IsValid() && ControlReversalVisual.Get() != Visual)
		{
			ControlReversalVisual->CompleteManagedRemoval();
		}
		ControlReversalVisual = Visual;
	}
	else if (ControlReversalVisual.Get() != Visual)
	{
		if (IsValid(Visual))
		{
			Visual->CompleteManagedRemoval();
		}
		return;
	}
	RequestVisual(ENPStatusVisualType::ControlReversal, bActive);
}

void ANPStatusVisualManager::RequestPhotoStunVisual(
	ANPPhotoStunGameplayCue* Visual, bool bActive)
{
	if (bActive)
	{
		if (PhotoStunVisual.IsValid() && PhotoStunVisual.Get() != Visual)
		{
			PhotoStunVisual->CompleteManagedRemoval();
		}
		PhotoStunVisual = Visual;
	}
	else if (PhotoStunVisual.Get() != Visual)
	{
		if (IsValid(Visual))
		{
			Visual->CompleteManagedRemoval();
		}
		return;
	}
	RequestVisual(ENPStatusVisualType::PhotoStun, bActive);
}

void ANPStatusVisualManager::RequestVisual(ENPStatusVisualType VisualType, bool bActive)
{
	FTransitionState& State = Transitions[GetVisualIndex(VisualType)];
	if (State.bRequestedActive == bActive && !State.bTransitioning)
	{
		ApplyVisualScale(VisualType, State.CurrentScale);
		if (!bActive)
		{
			FinishDisappear(VisualType);
		}
		return;
	}

	State.bRequestedActive = bActive;
	State.StartScale = State.CurrentScale;
	State.TargetScale = bActive ? 1.0f : 0.0f;
	State.Elapsed = 0.0f;
	const FRichCurve* Curve = GetTransitionCurve(bActive).GetRichCurveConst();
	float MaxTime = 0.0f;
	Curve->GetTimeRange(State.MinTime, MaxTime);
	State.Duration = FMath::Max(0.0f, MaxTime - State.MinTime);
	State.CurveStartValue = Curve->Eval(State.MinTime);
	State.CurveEndValue = Curve->Eval(MaxTime);
	State.bHasCurve = Curve->GetNumKeys() > 0;
	State.bTransitioning = State.bHasCurve && State.Duration > 0.0f;

	OnRequest(VisualType, bActive);
	if (bActive)
	{
		PrepareAppearance(VisualType);
	}
	ApplyVisualScale(VisualType, State.CurrentScale);
	if (State.bTransitioning)
	{
		SetActorTickEnabled(true);
		return;
	}

	State.CurrentScale = State.bHasCurve ? State.CurveEndValue : State.TargetScale;
	ApplyVisualScale(VisualType, State.CurrentScale);
	if (!bActive)
	{
		FinishDisappear(VisualType);
	}
}

void ANPStatusVisualManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Transitions); ++Index)
	{
		FTransitionState& State = Transitions[Index];
		if (!State.bTransitioning)
		{
			continue;
		}

		State.Elapsed += FMath::Max(0.0f, DeltaSeconds);
		const FRichCurve* Curve =
			GetTransitionCurve(State.bRequestedActive).GetRichCurveConst();
		const float CurveTime = State.MinTime + FMath::Min(State.Elapsed, State.Duration);
		const float NormalizedTime = FMath::Min(State.Elapsed / State.Duration, 1.0f);
		const float StartOffset = State.StartScale - State.CurveStartValue;
		State.CurrentScale = Curve->Eval(CurveTime) + StartOffset * (1.0f - NormalizedTime);
		const ENPStatusVisualType VisualType = static_cast<ENPStatusVisualType>(Index);
		ApplyVisualScale(VisualType, State.CurrentScale);

		if (State.Elapsed >= State.Duration)
		{
			State.bTransitioning = false;
			State.CurrentScale = State.CurveEndValue;
			ApplyVisualScale(VisualType, State.CurrentScale);
			if (!State.bRequestedActive)
			{
				FinishDisappear(VisualType);
			}
		}
	}

	SetActorTickEnabled(HasActiveTransition());
}

int32 ANPStatusVisualManager::GetVisualIndex(ENPStatusVisualType VisualType)
{
	return static_cast<int32>(VisualType);
}

const FRuntimeFloatCurve& ANPStatusVisualManager::GetTransitionCurve(bool bAppearing) const
{
	const ANPStatusVisualManager* ClassDefaults =
		GetClass()->GetDefaultObject<ANPStatusVisualManager>();
	return bAppearing ? ClassDefaults->AppearCurve : ClassDefaults->DisappearCurve;
}

void ANPStatusVisualManager::ApplyVisualScale(
	ENPStatusVisualType VisualType, float ScaleMultiplier)
{
	switch (VisualType)
	{
	case ENPStatusVisualType::Leader:
		if (ANPLeaderCrown* Crown = LeaderCrown
			? Cast<ANPLeaderCrown>(LeaderCrown->GetChildActor()) : nullptr)
		{
			Crown->SetVisualScaleMultiplier(ScaleMultiplier);
		}
		break;
	case ENPStatusVisualType::ControlReversal:
		if (ControlReversalVisual.IsValid())
		{
			ControlReversalVisual->SetManagedScaleMultiplier(ScaleMultiplier);
		}
		break;
	case ENPStatusVisualType::PhotoStun:
		if (PhotoStunVisual.IsValid())
		{
			PhotoStunVisual->SetManagedScaleMultiplier(ScaleMultiplier);
		}
		break;
	}
}

void ANPStatusVisualManager::PrepareAppearance(ENPStatusVisualType VisualType)
{
	if (VisualType != ENPStatusVisualType::Leader || !IsValid(LeaderCrown))
	{
		return;
	}
	LeaderCrown->SetVisibility(true, true);
	LeaderCrown->SetHiddenInGame(false, true);
	if (AActor* Crown = LeaderCrown->GetChildActor())
	{
		Crown->SetActorHiddenInGame(false);
		Crown->SetActorTickEnabled(true);
	}
}

void ANPStatusVisualManager::FinishDisappear(ENPStatusVisualType VisualType)
{
	switch (VisualType)
	{
	case ENPStatusVisualType::Leader:
		if (IsValid(LeaderCrown))
		{
			LeaderCrown->SetVisibility(false, true);
			LeaderCrown->SetHiddenInGame(true, true);
			if (AActor* Crown = LeaderCrown->GetChildActor())
			{
				Crown->SetActorHiddenInGame(true);
				Crown->SetActorTickEnabled(false);
			}
		}
		break;
	case ENPStatusVisualType::ControlReversal:
		if (ANPControlReversalGameplayCue* Visual = ControlReversalVisual.Get())
		{
			ControlReversalVisual.Reset();
			Visual->CompleteManagedRemoval();
		}
		break;
	case ENPStatusVisualType::PhotoStun:
		if (ANPPhotoStunGameplayCue* Visual = PhotoStunVisual.Get())
		{
			PhotoStunVisual.Reset();
			Visual->CompleteManagedRemoval();
		}
		break;
	}
}

bool ANPStatusVisualManager::HasActiveTransition() const
{
	for (const FTransitionState& State : Transitions)
	{
		if (State.bTransitioning)
		{
			return true;
		}
	}
	return false;
}
