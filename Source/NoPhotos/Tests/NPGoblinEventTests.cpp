#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/MapEvents/Goblin/NPGoblinMapEvent.h"
#include "Gameplay/Goblin/NPGoblinPatrolRoute.h"
#include "Gameplay/Relic/NPPulleyPictureRelic.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPGoblinEventLifecycleTest,
	"NoPhotos.MapEvents.Goblin.SingleGoblinLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPGoblinEventLifecycleTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldValues = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &WorldValues);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };

	ANPGoblinMapEvent* Event = World->SpawnActor<ANPGoblinMapEvent>();
	ANPGoblinPatrolRoute* RouteA = World->SpawnActor<ANPGoblinPatrolRoute>();
	ANPGoblinPatrolRoute* RouteB = World->SpawnActor<ANPGoblinPatrolRoute>();
	ANPGoblinPatrolRoute* InvalidRoute = World->SpawnActor<ANPGoblinPatrolRoute>();
	APlayerState* Photographer = World->SpawnActor<APlayerState>();
	if (!Event || !RouteA || !RouteB || !InvalidRoute || !Photographer)
	{
		AddError(TEXT("Could not create lifecycle test actors"));
		return false;
	}
	InvalidRoute->GetSpline()->SetClosedLoop(false);
	TestTrue(TEXT("Closed routes are usable"), RouteA->IsUsableRoute() && RouteB->IsUsableRoute());
	for (int32 Index = 0; Index < 32; ++Index)
	{
		ANPGoblinPatrolRoute* Selected = Event->FindPatrolRoute();
		TestTrue(TEXT("Only a usable loaded route is selected"), Selected == RouteA || Selected == RouteB);
	}

	Event->GoblinClass = ANPGoblinCharacter::StaticClass();
	Event->GoblinCount = 10; // A saved legacy BP count must not increase the active population.
	ANPGoblinCharacter* First = Event->SpawnGoblinAt(FTransform(FVector(0, 0, 500)), RouteA);
	if (!TestNotNull(TEXT("First goblin"), First))
	{
		return false;
	}
	FBoolProperty* UseDoor = FindFProperty<FBoolProperty>(ANPGoblinCharacter::StaticClass(), TEXT("bUseDoorPresentation"));
	if (!TestNotNull(TEXT("Door presentation setting"), UseDoor))
	{
		return false;
	}
	// Use the timed presentation path so the test does not require authored meshes or a NavMesh.
	UseDoor->SetPropertyValue_InContainer(First, false);
	TestTrue(TEXT("Event starts with the tracked goblin"), Event->StartEvent());
	First->FinishSpawnPresentation();
	Event->TrySpawnGoblin();
	TestEqual(TEXT("Repeated spawn request keeps one goblin even with legacy count 10"), Event->GetSpawnedGoblinCount(), 1);
	TestTrue(TEXT("Assigned route stays fixed"), First->GetPatrolRoute() == RouteA);

	AddExpectedError(TEXT("촬영 보상 유물 클래스가 설정되지 않았습니다."), EAutomationExpectedErrorFlags::Contains, 3);
	for (int32 Capture = 0; Capture < 3; ++Capture)
	{
		First->OnPhotographed_Implementation(Photographer, 1.0f, Capture);
	}
	TestEqual(TEXT("Lethal photo starts exit"), First->GetLifecycleState(), ENPGoblinLifecycleState::Despawning);
	TestEqual(TEXT("Exiting goblin still occupies the slot"), Event->GetSpawnedGoblinCount(), 1);
	TestFalse(TEXT("No respawn before exit finishes"), World->GetTimerManager().IsTimerActive(Event->SpawnTimer));
	First->FinishDespawnPresentation();
	TestEqual(TEXT("Completed exit frees the slot"), Event->GetSpawnedGoblinCount(), 0);
	TestTrue(TEXT("Active event schedules a replacement"), World->GetTimerManager().IsTimerActive(Event->SpawnTimer));

	Event->FinishEvent();
	TestFalse(TEXT("Ending during respawn delay cancels replacement"), World->GetTimerManager().IsTimerActive(Event->SpawnTimer));
	Event->TrySpawnGoblin();
	TestEqual(TEXT("A stale callback cannot spawn after event end"), Event->GetSpawnedGoblinCount(), 0);

	ANPGoblinCharacter* Second = Event->SpawnGoblinAt(FTransform(FVector(500, 0, 500)), RouteB);
	if (!TestNotNull(TEXT("Second cycle goblin"), Second))
	{
		return false;
	}
	UseDoor->SetPropertyValue_InContainer(Second, false);
	TestTrue(TEXT("Event can start a new cycle"), Event->StartEvent());
	Second->FinishSpawnPresentation();
	TestTrue(TEXT("New goblin can use a different route"), Second->GetPatrolRoute() == RouteB);
	Event->FinishEvent();
	TestEqual(TEXT("Event timeout starts exit"), Second->GetLifecycleState(), ENPGoblinLifecycleState::Despawning);
	Second->FinishDespawnPresentation();
	TestEqual(TEXT("Timeout exit clears the slot"), Event->GetSpawnedGoblinCount(), 0);
	TestFalse(TEXT("Timeout exit never schedules another goblin"), World->GetTimerManager().IsTimerActive(Event->SpawnTimer));
	Event->ScheduleSpawn(0.1f);
	TestFalse(TEXT("Inactive event rejects new timers"), World->GetTimerManager().IsTimerActive(Event->SpawnTimer));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPGoblinRelicDropCountsTest,
	"NoPhotos.MapEvents.Goblin.RelicDropCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNPGoblinRelicDropCountsTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldValues = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &WorldValues);
	if (!TestNotNull(TEXT("Reward test world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	APlayerState* Photographer = World->SpawnActor<APlayerState>();
	if (!TestNotNull(TEXT("Photographer"), Photographer))
	{
		return false;
	}
	// Use reflected BP defaults and a native relic instead of the project's reward table.
	const auto SpawnGoblin = [World](int32 PhotoCount, int32 DefeatCount)
	{
		ANPGoblinCharacter* Goblin = World->SpawnActor<ANPGoblinCharacter>();
		if (Goblin)
		{
			UClass* Class = Goblin->GetClass();
			FindFProperty<FBoolProperty>(Class, TEXT("bUseDoorPresentation"))->SetPropertyValue_InContainer(Goblin, false);
			FindFProperty<FObjectProperty>(Class, TEXT("RelicDropTable"))->SetObjectPropertyValue_InContainer(Goblin, nullptr);
			FindFProperty<FClassProperty>(Class, TEXT("PhotographedRelicClass"))
				->SetObjectPropertyValue_InContainer(Goblin, ANPPulleyPictureRelic::StaticClass());
			FindFProperty<FIntProperty>(Class, TEXT("PhotoRelicDropCount"))->SetPropertyValue_InContainer(Goblin, PhotoCount);
			FindFProperty<FIntProperty>(Class, TEXT("DefeatRelicDropCount"))->SetPropertyValue_InContainer(Goblin, DefeatCount);
		}
		return Goblin;
	};
	const auto CountRelics = [World](const ANPGoblinCharacter* Goblin)
	{
		int32 Count = 0;
		for (TActorIterator<ANPBaseRelic> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetOwner() == Goblin)
			{
				++Count;
			}
		}
		return Count;
	};

	ANPGoblinCharacter* Goblin = SpawnGoblin(2, 4);
	if (!TestNotNull(TEXT("Configured goblin"), Goblin)) { return false; }
	Goblin->OnPhotographed_Implementation(Photographer, 1.0f, 1);
	TestEqual(TEXT("First photo drops configured two relics"), CountRelics(Goblin), 2);
	Goblin->OnPhotographed_Implementation(Photographer, 1.0f, 2);
	TestEqual(TEXT("Second photo drops two more"), CountRelics(Goblin), 4);
	Goblin->OnPhotographed_Implementation(Photographer, 1.0f, 3);
	TestEqual(TEXT("Lethal photo adds two photo and four defeat relics"), CountRelics(Goblin), 10);
	Goblin->OnPhotographed_Implementation(Photographer, 1.0f, 4);
	TestEqual(TEXT("Already defeated goblin cannot pay again"), CountRelics(Goblin), 10);
	TestNotNull(TEXT("Legacy getter still exposes the latest relic"), Goblin->GetSpawnedPhotoRelic());

	ANPGoblinCharacter* DefeatOnly = SpawnGoblin(0, 3);
	if (!TestNotNull(TEXT("Defeat-only goblin"), DefeatOnly)) { return false; }
	DefeatOnly->OnPhotographed_Implementation(Photographer, 1.0f, 5);
	DefeatOnly->OnPhotographed_Implementation(Photographer, 1.0f, 6);
	TestEqual(TEXT("Zero photo count disables ordinary drops"), CountRelics(DefeatOnly), 0);
	DefeatOnly->OnPhotographed_Implementation(Photographer, 1.0f, 7);
	TestEqual(TEXT("Defeat reward works independently"), CountRelics(DefeatOnly), 3);

	ANPGoblinCharacter* PhotoOnly = SpawnGoblin(1, 0);
	if (!TestNotNull(TEXT("Photo-only goblin"), PhotoOnly)) { return false; }
	for (int32 Capture = 0; Capture < 3; ++Capture)
	{
		PhotoOnly->OnPhotographed_Implementation(Photographer, 1.0f, Capture);
	}
	TestEqual(TEXT("Zero defeat count preserves photo rewards on the lethal hit"), CountRelics(PhotoOnly), 3);

	ANPGoblinCharacter* Timeout = SpawnGoblin(2, 4);
	if (!TestNotNull(TEXT("Timeout goblin"), Timeout)) { return false; }
	Timeout->BeginDespawnPresentation();
	TestEqual(TEXT("Event timeout/despawn is not a defeat reward"), CountRelics(Timeout), 0);

	ANPGoblinCharacter* Disabled = SpawnGoblin(-1, -2);
	if (!TestNotNull(TEXT("Disabled rewards goblin"), Disabled)) { return false; }
	for (int32 Capture = 0; Capture < 3; ++Capture)
	{
		Disabled->OnPhotographed_Implementation(Photographer, 1.0f, 8 + Capture);
	}
	TestEqual(TEXT("Negative settings clamp to zero"), CountRelics(Disabled), 0);
	return true;
}

#endif
