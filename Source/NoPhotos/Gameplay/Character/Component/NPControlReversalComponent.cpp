#include "Gameplay/Character/Component/NPControlReversalComponent.h"

#include "AbilitySystemComponent.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Character/Component/NPStablePhysicsMovementComponent.h"

UNPControlReversalComponent::UNPControlReversalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPControlReversalComponent::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
	if (!IsValid(AbilitySystem))
	{
		return;
	}
	ControlTagHandle = AbilitySystem->RegisterGameplayTagEvent(
		NPGameplayTags::State_ControlsMirrored, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::HandleControlTagChanged);
	HandleControlTagChanged(NPGameplayTags::State_ControlsMirrored,
		AbilitySystem->GetTagCount(NPGameplayTags::State_ControlsMirrored));
}

FVector UNPControlReversalComponent::ResolveMovementInput(const FVector& WorldMoveInput,
	float InputViewYaw, bool bReverseHorizontal)
{
	if (WorldMoveInput.ContainsNaN() || !FMath::IsFinite(InputViewYaw))
	{
		return FVector::ZeroVector;
	}
	const FVector RawInput = WorldMoveInput.GetClampedToMaxSize(1.0f);
	if (!bReverseHorizontal)
	{
		return RawInput;
	}
	// 수평면 이동 전체를 반전하면 카메라 방향과 무관하게 W/S와 A/D가 함께 뒤집힙니다.
	// 수직 성분과 아날로그 입력 크기는 유지합니다. 기존 입력/RPC의 Yaw 인자는 호환성을 위해 유지합니다.
	return FVector(-RawInput.X, -RawInput.Y, RawInput.Z);
}

void UNPControlReversalComponent::ApplyRawMovementInput(const FVector& WorldMoveInput, float InputViewYaw)
{
	if (WorldMoveInput.ContainsNaN() || !FMath::IsFinite(InputViewYaw))
	{
		RawMovementInput = FVector::ZeroVector;
		RawInputViewYaw = 0.0f;
	}
	else
	{
		RawMovementInput = WorldMoveInput.GetClampedToMaxSize(1.0f);
		RawInputViewYaw = FRotator::NormalizeAxis(InputViewYaw);
	}
	bHasRawInput = true;
	ApplyResolvedInput();
}

void UNPControlReversalComponent::HandleControlTagChanged(FGameplayTag, int32 NewCount)
{
	const bool bNewReversed = NewCount > 0;
	if (bHorizontalReversed != bNewReversed)
	{
		bHorizontalReversed = bNewReversed;
		// 키를 누른 채 시작/종료해도 원본 입력에서 다시 계산합니다.
		ApplyResolvedInput();
	}
}

void UNPControlReversalComponent::ApplyResolvedInput()
{
	ANPStablePhysicsPawn* Pawn = Cast<ANPStablePhysicsPawn>(GetOwner());
	if (!bHasRawInput || !IsValid(Pawn) || (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled()))
	{
		return;
	}
	const FVector Resolved = ResolveMovementInput(RawMovementInput, RawInputViewYaw, bHorizontalReversed);
	if (UNPStablePhysicsMovementComponent* Movement = Pawn->GetStablePhysicsMovementComponent())
	{
		Movement->SetMoveInput(Resolved);
	}
	if (UNPStablePhysicsGrabComponent* Grab = Pawn->GetRightHandGrabComponent())
	{
		Grab->SetMovementIntent(Resolved);
	}
}

void UNPControlReversalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilitySystem) && ControlTagHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(NPGameplayTags::State_ControlsMirrored,
			EGameplayTagEventType::NewOrRemoved).Remove(ControlTagHandle);
	}
	Super::EndPlay(EndPlayReason);
}
