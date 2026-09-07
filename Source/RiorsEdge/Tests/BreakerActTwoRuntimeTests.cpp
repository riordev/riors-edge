#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerActTwoRuntimeTest,
    "RiorsEdge.Missions.ActTwo.ContactAndBreachRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerActTwoRuntimeTest::RunTest(const FString& Parameters)
{
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    FBreakerQuestFlagSet Carried;
    // Prior chapter is fixture setup; this test earns the Act II combat flags.
    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
        if (Mission.MissionId == TEXT("Act1.Fernhall"))
            for (const FBreakerMissionBeat& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Carried.Add(Flag);
    for (int32 Stage = 0; Stage < 3; ++Stage)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/ActTwo_%s/Lvl_Fernhall"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Package->SetFlags(RF_Transient);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Lvl_Fernhall")), Package, true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("Isolated real map world"), World)) return false;
        FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
        Context.SetCurrentWorld(World);
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
        UBreakerGameInstance* Session = NewObject<UBreakerGameInstance>();
        World->SetGameInstance(Session);
        Context.OwningGameInstance = Session;
        if (Stage == 1) Session->PendingRift = UBreakerZoneBuilder::FernhallRiftFor(TEXT("breach"));
        World->GetWorldSettings()->DefaultGameMode = ABreakerGameMode::StaticClass();
        if (!TestTrue(TEXT("Authority mode installed"), World->SetGameMode(FURL()))) return false;
        World->InitializeActorsForPlay(FURL());
        ABreakerGameMode* Mode = World->GetAuthGameMode<ABreakerGameMode>();
        if (!TestNotNull(TEXT("Actual game mode"), Mode)) return false;
        Mode->DispatchBeginPlay();
        ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Controller"), Controller)) return false;
        Controller->Possess(Player);
        // Character BeginPlay loads disk saves: bind only the real combat path.
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        FScriptDelegate QuestKill;
        QuestKill.BindUFunction(Player, TEXT("HandleQuestKill"));
        Player->GetCombat()->OnKillDealt.Add(QuestKill);
        UBreakerQuestJournal* Journal = Player->GetQuestJournal();
        if (Stage != 2) Journal->RestoreFrom(Carried);
        auto Kill = [&](ABreakerEnemy* Enemy)
        {
            if (!Enemy->HasActorBegunPlay()) Enemy->DispatchBeginPlay();
            FBreakerDamageRequest Hit;
            Hit.BaseDamage = 100000000.0f;
            Hit.bCanCritical = false;
            Hit.bBypassShield = true;
            Hit.SetInstigator(Player);
            return Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit).bKilled;
        };
        int32 Completions = 0;
        Mode->OnRiftCompleted.AddLambda([&](const FBreakerRiftDefinition&, APawn*) { ++Completions; });
        Mode->HandleStartingNewPlayer_Implementation(Controller);
        if (Stage == 0)
        {
            auto Contacts = [&]()
            {
                TArray<ABreakerEnemy*> Found;
                for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                    if (It->Tags.Contains(TEXT("Fernhall.AlteredContact"))) Found.Add(*It);
                return Found;
            };
            auto Doors = [&]()
            {
                int32 Count = 0;
                for (TActorIterator<ABreakerRiftDoor> It(World); It; ++It)
                    if (It->Rift.EncounterId == TEXT("breach.marshalling")) ++Count;
                return Count;
            };
            TestEqual(TEXT("No Act II contact before accepted investigation"), Contacts().Num(), 0);
            TestEqual(TEXT("No Breach door before report and orders"), Doors(), 0);
            Journal->SetFlag(TEXT("Quest.AlteredContact.Accepted"));
            TestEqual(TEXT("Acceptance alone cannot prepay arrival"), Contacts().Num(), 0);
            for (FName Flag : UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Fernhall"), Journal->GetState())) Journal->SetFlag(Flag);
            TArray<ABreakerEnemy*> Found = Contacts();
            if (!TestEqual(TEXT("Live journal opens exactly one authored contact"), Found.Num(), 1)) return false;
            ABreakerEnemy* Contact = Found[0];
            TestTrue(TEXT("Contact is the ordinary Drudge class"), Contact->IsA<ABreakerAlteredEnemy>());
            TestFalse(TEXT("Contact does not respawn"), Contact->DoesRespawn());
            TestFalse(TEXT("Contact cannot finish a rift"), Contact->IsRiftTerminator());
            UAbilitySystemComponent* ContactASC = Contact->GetAbilitySystemComponent();
            UBreakerAttributeSet* ContactAttributes = Cast<UBreakerAttributeSet>(Contact->GetDefaultSubobjectByName(TEXT("Attributes")));
            if (!TestNotNull(TEXT("Contact attribute subobject"), ContactAttributes)) return false;
            ContactASC->AddAttributeSetSubobject(ContactAttributes);
            if (!Contact->HasActorBegunPlay()) Contact->DispatchBeginPlay();
            Mode->Tick(0.0f);
            TestEqual(TEXT("Late BeginPlay still receives the authored wound"),
                ContactASC->GetNumericAttribute(UBreakerAttributeSet::GetHealthAttribute()), Contact->GetMonsterMaxHealth() * 0.35f, 0.01f);
            FBreakerDamageRequest Injury;
            Injury.BaseDamage = 1.0f;
            Injury.bCanCritical = false;
            Injury.bBypassShield = true;
            Injury.SetInstigator(Player);
            Contact->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Injury);
            const float InjuredHealth = ContactASC->GetNumericAttribute(UBreakerAttributeSet::GetHealthAttribute());
            TestTrue(TEXT("Further actual damage lowers contact health"), InjuredHealth < Contact->GetMonsterMaxHealth() * 0.35f);
            Mode->Tick(0.0f);
            TestEqual(TEXT("Subsequent mode ticks never restore wounded health"),
                ContactASC->GetNumericAttribute(UBreakerAttributeSet::GetHealthAttribute()), InjuredHealth);
            for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                if (*It != Contact) { TestTrue(TEXT("Unrelated patrol dies through combat"), Kill(*It)); break; }
            TestFalse(TEXT("Unrelated actual death cannot finish contact"), Journal->HasFlag(TEXT("Quest.AlteredContact.ContactDown")));
            TestTrue(TEXT("Dedicated contact actually dies through combat"), Kill(Contact));
            TestTrue(TEXT("Bound dedicated death grants objective"), Journal->HasFlag(TEXT("Quest.AlteredContact.ContactDown")));
            TestEqual(TEXT("Contact death alone does not create Breach door"), Doors(), 0);
            Journal->SetFlag(TEXT("Quest.AlteredContact.TurnedIn"));
            TestEqual(TEXT("Report alone still requires Breach orders"), Doors(), 0);
            Journal->SetFlag(TEXT("Quest.Breach.Accepted"));
            TestEqual(TEXT("Report plus accepted orders creates one physical door"), Doors(), 1);
            Mode->HandleStartingNewPlayer_Implementation(Controller);
            TestEqual(TEXT("Repeated startup cannot duplicate contact"), Contacts().Num(), 1);
            TestEqual(TEXT("Repeated startup cannot duplicate Breach door"), Doors(), 1);
            Carried = Journal->GetState();
        }
        else if (Stage == 1)
        {
            TestTrue(TEXT("Breach is an actual rift instance"), Mode->IsRiftInstance());
            ABreakerBossEnemy* Boss = nullptr;
            int32 ClearedWaves = 0;
            for (int32 Wave = 0; Wave < 4; ++Wave)
            {
                TArray<ABreakerEnemy*> Enemies;
                for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                {
                    if (It->IsDeadEnemy()) continue;
                    if (ABreakerBossEnemy* Candidate = Cast<ABreakerBossEnemy>(*It)) { Boss = Candidate; break; }
                    Enemies.Add(*It);
                }
                if (Boss) break;
                if (!TestTrue(TEXT("Each authored pre-boss wave contains enemies"), Enemies.Num() > 0)) return false;
                for (ABreakerEnemy* Enemy : Enemies) TestTrue(TEXT("Real wave enemy killed"), Kill(Enemy));
                ++ClearedWaves;
                TestFalse(TEXT("Pre-boss deaths cannot finish Marshal objective"), Journal->HasFlag(TEXT("Quest.Breach.MarshalDown")));
                Mode->StartNextWave();
            }
            TestEqual(TEXT("Breach requires three combat waves before boss four"), ClearedWaves, 3);
            if (!TestNotNull(TEXT("Fourth wave spawns a real boss"), Boss)) return false;
            TestFalse(TEXT("Breach never substitutes Holdfast"), Boss->IsA<ABreakerHoldfastEnemy>());
            TestEqual(TEXT("Field Marshal uses its authored concrete class"), Boss->GetClass(), ABreakerBossEnemy::StaticClass());
            TestEqual(TEXT("No completion before boss death"), Completions, 0);
            TestTrue(TEXT("Actual Field Marshal dies"), Kill(Boss));
            TestEqual(TEXT("Boss death completes exactly once"), Completions, 1);
            TestTrue(TEXT("Actual matching boss pays Marshal objective"), Journal->HasFlag(TEXT("Quest.Breach.MarshalDown")));
            TestEqual(TEXT("Combat without return still grants only prior two doctrine points"), UBreakerMissionLibrary::DoctrinePointEntitlement(Journal->GetState()), 2);
            Mode->CompleteRiftRun(Player);
            Mode->StartNextWave();
            TestEqual(TEXT("Repeat completion cannot duplicate run"), Completions, 1);
            TestFalse(TEXT("Completed Breach cannot restart waves"), Mode->IsWaveActive());
        }
        else
        {
            auto BreachDoors = [&]()
            {
                int32 Count = 0;
                for (TActorIterator<ABreakerRiftDoor> It(World); It; ++It)
                    if (It->Rift.EncounterId == TEXT("breach.marshalling")) ++Count;
                return Count;
            };
            TestEqual(TEXT("Startup binds empty journal without a Breach door"), BreachDoors(), 0);
            TestTrue(TEXT("Carried progress contains actual contact credit"), Carried.Has(TEXT("Quest.AlteredContact.ContactDown")));
            // Character save restore mutates this same journal without OnFlagSet.
            // Do not SetFlag: the real mode must notice the loaded progress.
            Journal->RestoreFrom(Carried);
            TestEqual(TEXT("Silent restore itself emits no door update"), BreachDoors(), 0);
            Mode->Tick(0.0f);
            TestEqual(TEXT("Mode notices same-journal restored orders and creates door"), BreachDoors(), 1);
            Mode->Tick(0.0f);
            TestEqual(TEXT("Further ticks cannot duplicate restored door"), BreachDoors(), 1);
        }
    }
    AddInfo(TEXT("Actual dedicated death, live journal gates, physical door and four-wave boss lifecycle validated; dialogue choice and cross-map loading are covered separately."));
    return true;
}
#endif
