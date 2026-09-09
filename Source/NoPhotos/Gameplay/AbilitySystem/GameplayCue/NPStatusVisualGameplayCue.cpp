#include "Gameplay/AbilitySystem/GameplayCue/NPStatusVisualGameplayCue.h"

ANPStatusVisualGameplayCue::ANPStatusVisualGameplayCue()
{
	bAutoDestroyOnRemove = false;
	bAllowMultipleOnActiveEvents = true;
	bAllowMultipleWhileActiveEvents = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	FRichCurve* AppearRichCurve = AppearCurve.GetRichCurve();
	const FKeyHandle AppearStart = AppearRichCurve->AddKey(0.0f, 0.0f);
	const FKeyHandle AppearPeak = AppearRichCurve->AddKey(0.181888f, 1.3f);
	const FKeyHandle AppearEnd = AppearRichCurve->AddKey(0.35f, 1.0f);
	AppearRichCurve->SetKeyInterpMode(AppearStart, RCIM_Cubic);
	AppearRichCurve->SetKeyInterpMode(AppearPeak, RCIM_Cubic);
	AppearRichCurve->SetKeyInterpMode(AppearEnd, RCIM_Cubic);

	FRichCurve* DisappearRichCurve = DisappearCurve.GetRichCurve();
	const FKeyHandle DisappearStart = DisappearRichCurve->AddKey(0.0f, 1.0f);
	const FKeyHandle DisappearPeak = DisappearRichCurve->AddKey(0.122861f, 1.3f);
	const FKeyHandle DisappearEnd = DisappearRichCurve->AddKey(0.3f, 0.0f);
	DisappearRichCurve->SetKeyInterpMode(DisappearStart, RCIM_Cubic);
	DisappearRichCurve->SetKeyInterpMode(DisappearPeak, RCIM_Cubic);
	DisappearRichCurve->SetKeyInterpMode(DisappearEnd, RCIM_Cubic);
}

bool ANPStatusVisualGameplayCue::WhileActive_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	Super::WhileActive_Implementation(Target, Parameters);
	VisualTarget = Target;
	bRemovalFinished = false;
	PrepareVisual();
	RequestAppear();
	return true;
}

bool ANPStatusVisualGameplayCue::OnRemove_Implementation(
	AActor* Target, const FGameplayCueParameters& Parameters)
{
	RequestDisappear();
	return true;
}

bool ANPStatusVisualGameplayCue::Recycle()
{
	SetActorTickEnabled(false);
	ScaleMultiplier = 0.0f;
	StartScale = 0.0f;
	Elapsed = 0.0f;
	bAppearing = false;
	bTransitioning = false;
	bRemovalFinished = false;
	VisualTarget.Reset();
	ResetVisual();
	ApplyVisualScale();
	return Super::Recycle();
}

void ANPStatusVisualGameplayCue::RequestAppear()
{
	if (bAppearing)
	{
		return;
	}
	bRemovalFinished = false;
	BeginTransition(true);
}

void ANPStatusVisualGameplayCue::RequestDisappear()
{
	if (!bAppearing || bRemovalFinished)
	{
		return;
	}
	BeginTransition(false);
}

void ANPStatusVisualGameplayCue::BeginTransition(bool bInAppearing)
{
	bAppearing = bInAppearing;
	StartScale = ScaleMultiplier;
	Elapsed = 0.0f;

	const FRichCurve* Curve = GetTransitionCurve().GetRichCurveConst();
	float MaxTime = 0.0f;
	Curve->GetTimeRange(MinTime, MaxTime);
	Duration = FMath::Max(0.0f, MaxTime - MinTime);
	CurveStartValue = Curve->Eval(MinTime);
	CurveEndValue = Curve->Eval(MaxTime);
	bTransitioning = Curve->GetNumKeys() > 0 && Duration > 0.0f;
	SetActorTickEnabled(true);
	ApplyVisualScale();

	if (!bTransitioning)
	{
		ScaleMultiplier = Curve->GetNumKeys() > 0
			? CurveEndValue
			: (bAppearing ? 1.0f : 0.0f);
		ApplyVisualScale();
		if (!bAppearing)
		{
			FinishDisappear();
		}
	}
}

void ANPStatusVisualGameplayCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bTransitioning)
	{
		return;
	}

	Elapsed += FMath::Max(0.0f, DeltaSeconds);
	const float NormalizedTime = FMath::Min(Elapsed / Duration, 1.0f);
	const float CurveTime = MinTime + FMath::Min(Elapsed, Duration);
	const float StartOffset = StartScale - CurveStartValue;
	ScaleMultiplier = GetTransitionCurve().GetRichCurveConst()->Eval(CurveTime)
		+ StartOffset * (1.0f - NormalizedTime);
	ApplyVisualScale();

	if (Elapsed < Duration)
	{
		return;
	}

	bTransitioning = false;
	ScaleMultiplier = CurveEndValue;
	ApplyVisualScale();
	if (!bAppearing)
	{
		FinishDisappear();
	}
}

void ANPStatusVisualGameplayCue::FinishDisappear()
{
	if (bRemovalFinished)
	{
		return;
	}
	bRemovalFinished = true;
	SetActorTickEnabled(false);
	GameplayCueFinishedCallback();
}

const FRuntimeFloatCurve& ANPStatusVisualGameplayCue::GetTransitionCurve() const
{
	return bAppearing ? AppearCurve : DisappearCurve;
}
