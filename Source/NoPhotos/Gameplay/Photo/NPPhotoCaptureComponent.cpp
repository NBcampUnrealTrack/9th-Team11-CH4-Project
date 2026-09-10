#include "Gameplay/Photo/NPPhotoCaptureComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Core/GameplayTag/NPGameplayTags.h"
#include "Gameplay/AbilitySystem/NPAbilitySystemComponent.h"
#include "Gameplay/Character/NPReplicatedStablePhysicsPawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Character/NPStablePhysicsPawn.h"
#include "Gameplay/Character/Component/NPStablePhysicsGrabComponent.h"
#include "Gameplay/Photo/NPPhotoLog.h"
#include "Gameplay/Photo/NPPhotoImageCodec.h"
#include "Core/Main/NPMainGameMode.h"
#include "Core/Main/NPMainPlayerController.h"

UNPPhotoCaptureComponent::UNPPhotoCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPPhotoCaptureComponent::BeginPlay()
{
	Super::BeginPlay();
	ImageCodec = NewObject<UNPPhotoImageCodec>(this, TEXT("PhotoCaptureImageCodec"));

	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (PlayerController && PlayerController->IsLocalController())
	{
		InitializeLocalCapture();
	}
}

void UNPPhotoCaptureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ExitPhotoMode();
	Super::EndPlay(EndPlayReason);
}

void UNPPhotoCaptureComponent::TogglePhotoMode()
{
	if (bPhotoModeActive)
	{
		ExitPhotoMode();
	}
	else
	{
		EnterPhotoMode();
	}
}

bool UNPPhotoCaptureComponent::EnterPhotoMode()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	ANPStablePhysicsPawn* StablePawn = PlayerController
		? Cast<ANPStablePhysicsPawn>(PlayerController->GetPawn())
		: nullptr;
	if (!PlayerController || !PlayerController->IsLocalController() || !StablePawn)
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[PhotoMode] Enter rejected: invalid local Pawn."));
		return false;
	}
	if (IsPhotographerGrabbing())
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[PhotoMode] Enter rejected: photographer is grabbing."));
		return false;
	}

	PhotoModePawn = StablePawn;
	bPhotoModeActive = true;
	StablePawn->SetPhotoViewActive(true);
	UE_LOG(LogNPPhoto, Log, TEXT("[PhotoMode] Entered. Pawn=%s"), *GetNameSafe(StablePawn));
	return true;
}

void UNPPhotoCaptureComponent::ExitPhotoMode()
{
	if (PhotoModePawn.IsValid())
	{
		PhotoModePawn->SetPhotoViewActive(false);
	}
	PhotoModePawn.Reset();
	if (bPhotoModeActive)
	{
		bPhotoModeActive = false;
		UE_LOG(LogNPPhoto, Log, TEXT("[PhotoMode] Exited."));
	}
}

bool UNPPhotoCaptureComponent::CanTakePhotoLocally() const
{
	const APlayerController* PlayerController =
		Cast<APlayerController>(GetOwner());
	const ANPStablePhysicsPawn* Pawn = PlayerController
		? Cast<ANPStablePhysicsPawn>(PlayerController->GetPawn())
		: nullptr;
	return PlayerController
		&& PlayerController->IsLocalController()
		&& GetWorld()
		&& bPhotoModeActive
		&& Pawn
		&& Pawn->IsPhotoViewReady()
		&& !IsPhotographerGrabbing();
}

bool UNPPhotoCaptureComponent::TakePhoto()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (!PlayerController || !PlayerController->IsLocalController() || !World)
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[Capture] Invalid local capture context. Controller=%s Local=%s World=%s"),
			*GetNameSafe(PlayerController),
			PlayerController && PlayerController->IsLocalController() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(World));
		return false;
	}
	ANPStablePhysicsPawn* Pawn = Cast<ANPStablePhysicsPawn>(PlayerController->GetPawn());
	if (!bPhotoModeActive || !Pawn || !Pawn->IsPhotoViewReady())
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[Capture] Rejected locally: photo mode is inactive or camera is blending. Active=%s Pawn=%s Ready=%s"),
			bPhotoModeActive ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Pawn),
			Pawn && Pawn->IsPhotoViewReady() ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (IsPhotographerGrabbing())
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Capture] Rejected locally: photographer is grabbing an object."));
		return false;
	}
	if (!SceneCapture || !PhotoRenderTarget)
	{
		UE_LOG(LogNPPhoto, Log, TEXT("[Capture] Initializing SceneCapture and RenderTarget."));
		InitializeLocalCapture();
	}
	if (!SceneCapture || !PhotoRenderTarget)
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[Capture] Initialization failed. SceneCapture=%s RenderTarget=%s"),
			*GetNameSafe(SceneCapture),
			*GetNameSafe(PhotoRenderTarget));
		return false;
	}

	bPhotoAttemptInProgress = true;
	if (!Pawn || !Pawn->PlayPhotoShotMontage())
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[Montage] Photo attempt canceled. Pawn=%s"),
			*GetNameSafe(PlayerController->GetPawn()));
		CancelPhotoAttempt();
		return false;
	}
	UE_LOG(LogNPPhoto, Log, TEXT("[Montage] PhotoShotMontage started. Pawn=%s"), *GetNameSafe(Pawn));

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	SceneCapture->SetWorldLocationAndRotation(CameraLocation, CameraRotation);
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[Capture] Capturing scene. Location=%s Rotation=%s Target=%s"),
		*CameraLocation.ToCompactString(),
		*CameraRotation.ToCompactString(),
		*GetNameSafe(PhotoRenderTarget));
	SceneCapture->CaptureScene();
	UE_LOG(LogNPPhoto, Log, TEXT("[Capture] CaptureScene requested successfully."));
	OnPhotoCaptured.Broadcast(PhotoRenderTarget);
	if (ANPMainPlayerController* MainPlayerController = Cast<ANPMainPlayerController>(PlayerController))
	{
		MainPlayerController->PlayPhotoFlash();
	}
	else
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[PhotoUI] Flash skipped: Controller is not ANPMainPlayerController. Controller=%s"),
			*GetNameSafe(PlayerController));
	}

	bPhotoAttemptInProgress = false;
	const uint16 CaptureSequence = ++NextCaptureSequence;
	if (!ImageCodec)
	{
		return false;
	}

	TArray<uint8> JpegData;
	if (!ImageCodec->EncodeRenderTargetToJpeg(PhotoRenderTarget, JpegQuality, JpegData))
	{
		return false;
	}
	PendingJpegPhotos.Add(CaptureSequence, MoveTemp(JpegData));
	ServerRequestTakePhoto(CameraLocation, CameraRotation.Vector(), CaptureSequence);
	return true;
}

void UNPPhotoCaptureComponent::CancelPhotoAttempt()
{
	bPhotoAttemptInProgress = false;
}

bool UNPPhotoCaptureComponent::IsPhotographerGrabbing() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const UNPStablePhysicsGrabComponent* GrabComponent = Pawn
		? Pawn->FindComponentByClass<UNPStablePhysicsGrabComponent>()
		: nullptr;
	return GrabComponent && GrabComponent->IsHoldingObject();
}

void UNPPhotoCaptureComponent::InitializeLocalCapture()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || SceneCapture || PhotoRenderTarget)
	{
		UE_LOG(
			LogNPPhoto,
			Verbose,
			TEXT("[Capture] InitializeLocalCapture skipped. Owner=%s SceneCapture=%s RenderTarget=%s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(SceneCapture),
			*GetNameSafe(PhotoRenderTarget));
		return;
	}

	PhotoRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("PhotoRenderTarget"));
	PhotoRenderTarget->RenderTargetFormat = RTF_RGBA8;
	PhotoRenderTarget->TargetGamma = 2.2f;
	PhotoRenderTarget->ClearColor = FLinearColor::Black;
	PhotoRenderTarget->InitAutoFormat(CaptureWidth, CaptureHeight);
	PhotoRenderTarget->UpdateResourceImmediate(true);

	SceneCapture = NewObject<USceneCaptureComponent2D>(OwnerActor, TEXT("PhotoSceneCapture"));
	OwnerActor->AddInstanceComponent(SceneCapture);
	SceneCapture->TextureTarget = PhotoRenderTarget;
	SceneCapture->FOVAngle = CaptureFOV;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->bAlwaysPersistRenderingState = true;
	SceneCapture->RegisterComponent();
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[Capture] Local capture initialized. Size=%dx%d FOV=%.1f"),
		CaptureWidth,
		CaptureHeight,
		CaptureFOV);
}

void UNPPhotoCaptureComponent::ServerRequestTakePhoto_Implementation(
	FVector_NetQuantize10 CameraLocation,
	FVector_NetQuantizeNormal CameraForward,
	uint16 CaptureSequence)
{
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[Server] Photo RPC received. Owner=%s Sequence=%u"),
		*GetNameSafe(GetOwner()),
		CaptureSequence);

	APlayerController* Photographer = Cast<APlayerController>(GetOwner());
	ANPMainGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ANPMainGameMode>()
		: nullptr;
	ANPReplicatedStablePhysicsPawn* PhotographerPawn = Photographer
		? Cast<ANPReplicatedStablePhysicsPawn>(Photographer->GetPawn())
		: nullptr;
	UNPAbilitySystemComponent* AbilitySystem = PhotographerPawn
		? Cast<UNPAbilitySystemComponent>(
			PhotographerPawn->GetAbilitySystemComponent())
		: nullptr;
	const bool bServerPhotoAiming = AbilitySystem
		&& AbilitySystem->HasMatchingGameplayTag(
			NPGameplayTags::State_Photo_Aiming);
	if (!Photographer || !GameMode || !bServerPhotoAiming)
	{
		UE_LOG(
			LogNPPhoto,
			Error,
			TEXT("[Server] Rejected: invalid photographer, GameMode, or GAS photo aim state. Photographer=%s GameMode=%s PhotoAiming=%s"),
			*GetNameSafe(Photographer),
			*GetNameSafe(GameMode),
			bServerPhotoAiming ? TEXT("true") : TEXT("false"));
		FNPPhotoEvidenceResult FailureResult;
		FailureResult.CaptureSequence = CaptureSequence;
		FailureResult.FailureReason = ENPPhotoEvidenceFailureReason::InvalidPhotographer;
		ClientReceivePhotoResult(FailureResult);
		return;
	}

	FNPPhotoEvidenceResult RejectedResult;
	RejectedResult.CaptureSequence = CaptureSequence;
	RejectedResult.Photographer = Photographer->PlayerState;
	if (IsPhotographerGrabbing())
	{
		UE_LOG(LogNPPhoto, Warning, TEXT("[Server] Rejected: photographer is grabbing an object."));
		RejectedResult.FailureReason = ENPPhotoEvidenceFailureReason::PhotographerIsGrabbing;
		ClientReceivePhotoResult(RejectedResult);
		return;
	}

	const double CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastServerCaptureTime < PhotoCooldown)
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[Server] Rejected: cooldown. Remaining=%.2f"),
			PhotoCooldown - (CurrentTime - LastServerCaptureTime));
		RejectedResult.FailureReason = ENPPhotoEvidenceFailureReason::CaptureOnCooldown;
		ClientReceivePhotoResult(RejectedResult);
		return;
	}
	LastServerCaptureTime = CurrentTime;

	if (AbilitySystem && PhotographerPawn)
	{
		FGameplayCueParameters ShutterCueParameters;
		ShutterCueParameters.Location = PhotographerPawn->GetActorLocation();
		ShutterCueParameters.Normal = PhotographerPawn->GetActorForwardVector();
		ShutterCueParameters.Instigator = PhotographerPawn;
		ShutterCueParameters.EffectCauser = PhotographerPawn;
		AbilitySystem->ExecuteGameplayCue(
			NPGameplayTags::GameplayCue_Photo_Shutter,
			ShutterCueParameters);
		UE_LOG(
			LogNPPhoto,
			Log,
			TEXT("[PhotoCue][Shutter] Requested. Pawn=%s ASC=%s Location=%s Tag=%s"),
			*GetNameSafe(PhotographerPawn),
			*GetNameSafe(AbilitySystem),
			*ShutterCueParameters.Location.ToCompactString(),
			*NPGameplayTags::GameplayCue_Photo_Shutter.GetTag().ToString());
	}
	else
	{
		UE_LOG(
			LogNPPhoto,
			Warning,
			TEXT("[PhotoCue][Shutter] Skipped: Pawn or ASC is invalid. Pawn=%s ASC=%s"),
			*GetNameSafe(PhotographerPawn),
			*GetNameSafe(AbilitySystem));
	}

	FNPPhotoCaptureRequest Request;
	Request.Photographer = Photographer;
	Request.CameraLocation = CameraLocation;
	Request.CameraForward = CameraForward;
	Request.CaptureSequence = CaptureSequence;
	ClientReceivePhotoResult(GameMode->HandlePhotoCaptureRequest(Request));
}

void UNPPhotoCaptureComponent::ClientReceivePhotoResult_Implementation(
	const FNPPhotoEvidenceResult& Result)
{
	UE_LOG(
		LogNPPhoto,
		Log,
		TEXT("[Result] PlayerCaptured=%s CapturedPlayer=%s RelicSuccess=%s ReactiveSuccess=%s Reason=%d Thief=%s Relic=%s ReactiveTarget=%s"),
		Result.bPlayerCaptured ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Result.CapturedPlayer.Get()),
		Result.bSuccess ? TEXT("true") : TEXT("false"),
		Result.bReactiveTargetSuccess ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Result.FailureReason),
		*GetNameSafe(Result.Thief.Get()),
		*GetNameSafe(Result.Relic.Get()),
		*GetNameSafe(Result.ReactiveTarget.Get()));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Result.bSuccess && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Green,
			FString::Printf(
				TEXT("[사진 판정 성공]\n촬영자: %s\n도둑: %s\n유물: %s"),
				*GetNameSafe(Result.Photographer.Get()),
				*GetNameSafe(Result.Thief.Get()),
				*GetNameSafe(Result.Relic.Get())));
	}
	if (Result.bReactiveTargetSuccess && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Yellow,
			FString::Printf(
				TEXT("[고블린 촬영 성공]\n촬영자: %s\n대상: %s\n가시율: %.0f%%"),
				*GetNameSafe(Result.Photographer.Get()),
				*GetNameSafe(Result.ReactiveTarget.Get()),
				Result.ReactiveTargetVisibility * 100.0f));
	}
#endif

	if (TArray<uint8>* JpegData = PendingJpegPhotos.Find(Result.CaptureSequence))
	{
		if (Result.bSuccess && Result.PhotoId.IsValid())
		{
			StoreLocalCorrectPhoto(
				Result.PhotoId,
				Result.CaptureSequence,
				MoveTemp(*JpegData));
		}
		PendingJpegPhotos.Remove(Result.CaptureSequence);
	}
	OnPhotoResultReceived.Broadcast(Result);
}

void UNPPhotoCaptureComponent::StoreLocalCorrectPhoto(
	const FGuid& PhotoId,
	const uint16 CaptureSequence,
	TArray<uint8>&& JpegData)
{
	if (!PhotoId.IsValid() || JpegData.IsEmpty())
	{
		return;
	}

	FLocalCorrectPhoto& Photo = LocalCorrectPhotos.FindOrAdd(PhotoId);
	Photo.CaptureSequence = CaptureSequence;
	Photo.JpegData = MoveTemp(JpegData);
	Photo.Width = CaptureWidth;
	Photo.Height = CaptureHeight;
	LocalPhotoOrder.AddUnique(PhotoId);

	while (LocalPhotoOrder.Num() > MaximumLocalCorrectPhotos)
	{
		const FGuid OldestPhotoId = LocalPhotoOrder[0];
		LocalPhotoOrder.RemoveAt(0);
		LocalCorrectPhotos.Remove(OldestPhotoId);
		LocalPhotoTextures.Remove(OldestPhotoId);
	}
}

UTexture2D* UNPPhotoCaptureComponent::FindLocalPhotoTexture(const FGuid& PhotoId)
{
	if (const TObjectPtr<UTexture2D>* CachedTexture = LocalPhotoTextures.Find(PhotoId))
	{
		return CachedTexture->Get();
	}

	const FLocalCorrectPhoto* Photo = LocalCorrectPhotos.Find(PhotoId);
	if (!Photo || !ImageCodec)
	{
		return nullptr;
	}

	UTexture2D* Texture = ImageCodec->DecodeJpegToTexture(Photo->JpegData);
	if (IsValid(Texture))
	{
		LocalPhotoTextures.Add(PhotoId, Texture);
	}
	return Texture;
}

bool UNPPhotoCaptureComponent::GetLocalPhotoData(
	const FGuid& PhotoId,
	uint16& OutCaptureSequence,
	const TArray<uint8>*& OutJpegData,
	int32& OutWidth,
	int32& OutHeight) const
{
	const FLocalCorrectPhoto* Photo = LocalCorrectPhotos.Find(PhotoId);
	if (!Photo)
	{
		return false;
	}

	OutCaptureSequence = Photo->CaptureSequence;
	OutJpegData = &Photo->JpegData;
	OutWidth = Photo->Width;
	OutHeight = Photo->Height;
	return true;
}

void UNPPhotoCaptureComponent::ResetLocalPhotos()
{
	PendingJpegPhotos.Reset();
	LocalCorrectPhotos.Reset();
	LocalPhotoOrder.Reset();
	LocalPhotoTextures.Reset();
	NextCaptureSequence = 0;
}
