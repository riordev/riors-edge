#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "GameFramework/PlayerController.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftBossEvidenceTest, "RiorsEdge.Missions.RiftBossEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRiftBossEvidenceTest::RunTest(const FString& Parameters)
{
    const FBreakerRiftDefinition Rift = UBreakerZoneBuilder::FernhallRiftFor(TEXT("substation"));
    const FBreakerMissionDefinition* Mission = UBreakerMissionLibrary::GetMissions().FindByPredicate(
        [](const FBreakerMissionDefinition& Entry) { return Entry.MissionId == TEXT("Act1.Fernhall"); });
    if (!TestNotNull(TEXT("authored mission"), Mission)) return false;
    FBreakerQuestFlagSet BeforeBoss;
    for (const FBreakerMissionBeat& Beat : Mission->Beats)
    {
        if (Beat.Kind == EBreakerMissionBeatKind::Boss) break;
        for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) BeforeBoss.Flags.AddUnique(Flag);
    }
    const TArray<FName> ExpectedFlags = UBreakerMissionLibrary::RiftCompletionFlagsFor(Rift, BeforeBoss);
    if (!TestTrue(TEXT("fixture reaches a real current boss objective"), !ExpectedFlags.IsEmpty())) return false;
    FObjectPropertyBase* ActiveBossProperty = FindFProperty<FObjectPropertyBase>(ABreakerGameMode::StaticClass(), TEXT("ActiveBoss"));
    if (!TestNotNull(TEXT("active boss fixture property"), ActiveBossProperty)) return false;
    // This isolates the evidence gate, not spawning, travel, or the boss AI.
    // ActiveBoss and the prior journal stage are explicit fixture setup. The
    // existing Rift runtime-loop test separately exercises real boss spawning.
    for (int32 Case = 0; Case < 5; ++Case)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("isolated evidence world"), World)) return false;
        FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
        Context.SetCurrentWorld(World);
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
        UBreakerGameInstance* Session = NewObject<UBreakerGameInstance>();
        World->SetGameInstance(Session); Context.OwningGameInstance = Session; Session->PendingRift = Rift;
        ABreakerGameMode* Mode = World->SpawnActor<ABreakerGameMode>();
        ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        if (!Mode || !Player || !Controller) return false;
        // No Character BeginPlay: no player save load or persistence.
        Controller->Possess(Player);
        // This isolated world skips actor initialization. Controller's normal
        // PostInitializeComponents registers it in the world's controller list;
        // reproduce that registration so the production death callback can
        // resolve the actual completing pawn through GetFirstPlayerController.
        World->AddController(Controller);
        if (!TestTrue(TEXT("death callback can resolve fixture player"),
            World->GetFirstPlayerController() == Controller && Controller->GetPawn() == Player)) return false;
        Player->GetQuestJournal()->RestoreFrom(BeforeBoss);
        TSubclassOf<ABreakerBossEnemy> BossClass = ABreakerBossEnemy::ClassForBossName(UBreakerMissionLibrary::BossForRift(Rift));
        if (Case == 3) BossClass = ABreakerBossEnemy::StaticClass();
        ABreakerBossEnemy* Boss = World->SpawnActor<ABreakerBossEnemy>(BossClass);
        if (!TestNotNull(TEXT("evidence subject"), Boss)) return false;
        ActiveBossProperty->SetObjectPropertyValue_InContainer(Mode, Case == 2 ? nullptr : Boss);
        Mode->MarkRiftTerminator(Boss);
        UBreakerCombatComponent* Combat = Boss->FindComponentByClass<UBreakerCombatComponent>();
        UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Boss);
        Combat->BindAttributes(Attributes);
        Attributes->ApplyHealth(100);
        if (Case >= 2)
        {
            FBreakerDamageRequest Damage;
            Damage.BaseDamage = 1000000; Damage.bCanCritical = false; Damage.bBypassShield = true;
            Combat->ReceiveDamage(Damage);
            if (!TestTrue(TEXT("actual lethal damage establishes death"), Combat->IsDead())) return false;
        }
        int32 Completions = 0;
        Mode->OnRiftCompleted.AddLambda([&](const FBreakerRiftDefinition&, APawn* CompletedBy)
        {
            ++Completions;
            TestTrue(TEXT("completion carries actual journal owner"), CompletedBy == Player);
        });
        if (Case == 0) Mode->CompleteRiftRun(Player);
        else Boss->OnRiftTerminatorDefeated.Broadcast(Boss);
        TestEqual(FString::Printf(TEXT("case %d retains generic completion broadcast"), Case), Completions, 1);
        for (FName Flag : ExpectedFlags)
            TestEqual(FString::Printf(TEXT("case %d story evidence %s"), Case, *Flag.ToString()),
                Player->GetQuestJournal()->HasFlag(Flag), Case == 4);
    }
    const FBreakerRiftDefinition Breach = UBreakerZoneBuilder::FernhallRiftFor(TEXT("breach"));
    TestEqual(TEXT("breach stable encounter"), Breach.EncounterId, FName(TEXT("breach.marshalling")));
    TestEqual(TEXT("breach campaign level"), Breach.AreaLevel, 20);
    TestEqual(TEXT("breach authored boss"), UBreakerMissionLibrary::BossForRift(Breach), FName(TEXT("FieldMarshal")));
    return true;
}
#endif
