#include "UI/GameScreen/Relic/NPRelicInfoUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Relic/NPBaseRelic.h"
#include "UI/GameScreen/Relic/NPRelicInfoWidget.h"
#include "TimerManager.h"

UNPRelicInfoUIComponent::UNPRelicInfoUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

//유물 정보 위젯 준비, Pawn의 잡기 이벤트 연결
void UNPRelicInfoUIComponent::BeginPlay()
{
	Super::BeginPlay();
	CreateRelicInfoWidget();

	if (APlayerController* PlayerController = GetLocalPlayerController())
	{
		PlayerController->OnPossessedPawnChanged.AddDynamic(
			this,
			&UNPRelicInfoUIComponent::HandlePossessedPawnChanged);
		BindToGrabComponent(PlayerController->GetPawn());
	}
}

//게임 종료시 타이머와 델리게이트를 해제, 생성한 위젯을 화면에서 제거
void UNPRelicInfoUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPopTimer();

	if (APlayerController* PlayerController = GetLocalPlayerController())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(
			this,
			&UNPRelicInfoUIComponent::HandlePossessedPawnChanged);
	}
	UnbindFromGrabComponent();

	if (IsValid(RelicInfoWidget))
	{
		RelicInfoWidget->RemoveFromParent();
		RelicInfoWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

//유물을 잡으면 진행 중인 퇴장을 취소, 데이터를 갱신한 위젯을 화면에 표시
void UNPRelicInfoUIComponent::ShowRelicInfo(ANPBaseRelic* Relic)
{
	ClearPopTimer();

	if (!IsValid(Relic) || !CreateRelicInfoWidget())
	{
		return;
	}

	RelicInfoWidget->SetPopRequested(false);
	RelicInfoWidget->SetRelicInfo(Relic);
	RelicInfoWidget->SetVisibility(ESlateVisibility::Visible);
	RelicInfoWidget->PlayShowAnimation();
}

//위젯 제거 요청. 입장 애니메이션을 반대로 재생, 애니메이션 재생 후 위젯 제거 예약
void UNPRelicInfoUIComponent::HideRelicInfo()
{
	if (!IsValid(RelicInfoWidget))
	{
		return;
	}

	if (RelicInfoWidget->IsPopRequested())
	{
		return;
	}

	RelicInfoWidget->SetPopRequested(true);
	RelicInfoWidget->OnPopRequested();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PopTimerHandle,
			this,
			&UNPRelicInfoUIComponent::CompleteRelicInfoPop,
			FMath::Max(0.0f, PopAnimationDuration),
			false);
	}
	else
	{
		CompleteRelicInfoPop();
	}
}

// 위젯이 아직 없다면 로컬 PlayerController를 Owning Player로 사용해 생성하고 숨겨 둡니다.
bool UNPRelicInfoUIComponent::CreateRelicInfoWidget()
{
	if (IsValid(RelicInfoWidget))
	{
		return true;
	}

	APlayerController* PlayerController = GetLocalPlayerController();
	if (!IsValid(PlayerController) || !RelicInfoWidgetClass)
	{
		return false;
	}

	RelicInfoWidget = CreateWidget<UNPRelicInfoWidget>(PlayerController, RelicInfoWidgetClass);
	if (!IsValid(RelicInfoWidget))
	{
		return false;
	}

	RelicInfoWidget->AddToViewport();
	RelicInfoWidget->ResetRelicInfo();
	RelicInfoWidget->SetVisibility(ESlateVisibility::Hidden);
	return true;
}

// 퇴장 애니메이션이 끝나면 OnPopped를 호출, 위젯을 Viewport에서 완전히 제거
void UNPRelicInfoUIComponent::CompleteRelicInfoPop()
{
	ClearPopTimer();

	if (!IsValid(RelicInfoWidget))
	{
		return;
	}

	RelicInfoWidget->OnPopped();
	RelicInfoWidget->RemoveFromParent();
	RelicInfoWidget = nullptr;
}

// 이전에 예약된 위젯 제거 타이머를 취소하여 중복 제거 또는 재표시 직후 제거되는 상황을 막습니다.
void UNPRelicInfoUIComponent::ClearPopTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PopTimerHandle);
	}
}

// 컴포넌트가 PlayerController나 Pawn 어느 쪽에 붙어 있어도 해당 로컬 PlayerController를 찾아 반환
APlayerController* UNPRelicInfoUIComponent::GetLocalPlayerController() const
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetOwner()))
	{
		return PlayerController->IsLocalController() ? PlayerController : nullptr;
	}

	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(PawnOwner->GetController()))
		{
			return PlayerController->IsLocalController() ? PlayerController : nullptr;
		}
	}

	return nullptr;
}

// PlayerController가 새로운 Pawn을 소유하면 기존 잡기 이벤트 연결을 새 Pawn으로 교체
void UNPRelicInfoUIComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToGrabComponent(NewPawn);
}

// Pawn에서 잡기 컴포넌트를 찾아 잡은 물체 변경 이벤트를 구독하고, 현재 잡은 물체 상태도 즉시 반영
void UNPRelicInfoUIComponent::BindToGrabComponent(APawn* Pawn)
{
	UnbindFromGrabComponent();

	if (!IsValid(Pawn))
	{
		return;
	}

	GrabComponent = Pawn->FindComponentByClass<UNPStablePhysicsGrabComponent>();
	if (!IsValid(GrabComponent))
	{
		return;
	}

	GrabComponent->OnGrabbedComponentChanged.AddUObject(
		this,
		&UNPRelicInfoUIComponent::HandleGrabbedComponentChanged);
	HandleGrabbedComponentChanged(GrabComponent->GetGrabbedComponent());
}

//기존 Pawn의 잡기 컴포넌트에서 등록했던 델리게이트를 제거합니다.
void UNPRelicInfoUIComponent::UnbindFromGrabComponent()
{
	if (IsValid(GrabComponent))
	{
		GrabComponent->OnGrabbedComponentChanged.RemoveAll(this);
		GrabComponent = nullptr;
	}
}

// 새로 잡은 물체가 유물이면 정보를 표시하고, 유물이 아니거나 놓았다면 위젯 퇴장
void UNPRelicInfoUIComponent::HandleGrabbedComponentChanged(UPrimitiveComponent* GrabbedComponent)
{
	ANPBaseRelic* Relic = GrabbedComponent ? Cast<ANPBaseRelic>(GrabbedComponent->GetOwner()) : nullptr;
	if (IsValid(Relic))
	{
		ShowRelicInfo(Relic);
	}
	else if (IsValid(RelicInfoWidget)
		&& RelicInfoWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		HideRelicInfo();
	}
}
