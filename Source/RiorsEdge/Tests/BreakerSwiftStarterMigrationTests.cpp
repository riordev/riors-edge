#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Slipcut.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerSaveGame.h"
#include "Weapons/BreakerWeaponComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSwiftStarterMigrationTest,
    "RiorsEdge.Save.Migration.SwiftStarterV9ToV10",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSwiftStarterMigrationTest::RunTest(const FString& Parameters)
{
    FString Note;
    const FName Skim(TEXT("Swift.Skim")), Slipcut(TEXT("Swift.Slipcut")), Lead(TEXT("Swift.Lead"));
    auto Legacy = [&]()
    {
        auto* Save = NewObject<UBreakerSaveGame>();
        Save->SaveVersion = 9;
        Save->CoreLayoutVersion = UBreakerSaveGame::ActiveCoreLayoutVersion;
        Save->Progression.PermanentClass = EBreakerClassId::Swift;
        Save->Progression.AbilityLoadout.ClassAbilityOne = Skim;
        Save->Progression.AbilityLoadout.Ultimate = TEXT("Swift.Overdrive");
        return Save;
    };
    auto* Unbought = Legacy();
    if (!TestTrue(TEXT("Unbought starter migrates"), UBreakerSaveGame::MigrateToCurrent(*Unbought, Note))) return false;
    TestEqual(TEXT("Version advances to ten"), Unbought->SaveVersion, 10);
    TestEqual(TEXT("Retired equipped starter becomes Slipcut"), Unbought->Progression.AbilityLoadout.ClassAbilityOne, Slipcut);
    TestTrue(TEXT("Intentionally empty second slot remains empty"), Unbought->Progression.AbilityLoadout.ClassAbilityTwo.IsNone());
    TestEqual(TEXT("Never-purchased Slipcut refunds no token"), Unbought->Progression.UnspentAbilityTokens, 0);

    auto* Paid = Legacy();
    auto& P = Paid->Progression;
    // A restored historical v9 receipt, not a present-day privileged unlock.
    P.UnlockedAbilityIds = {Skim, Slipcut, Lead};
    P.UnspentAbilityTokens = 2;
    P.AbilityTokensGranted = 5;
    P.UnspentDoctrinePoints = 0;
    P.CommittedBranch = TEXT("Swift.Kinetic");
    P.DoctrineNodeRanks = {{TEXT("Swift.Kinetic.ReadTheRoom"), 2}, {TEXT("Swift.Kinetic.Redirect"), 2}, {TEXT("Swift.Kinetic.SkimDiscipline"), 1}, {TEXT("Swift.Kinetic.SpendToLive"), 1}};
    P.AbilityLoadout.ClassAbilityTwo = Slipcut;
    if (!TestTrue(TEXT("Paid historical purchases migrate"), UBreakerSaveGame::MigrateToCurrent(*Paid, Note))) return false;
    TestTrue(TEXT("Migration does not duplicate equipped Slipcut"), P.AbilityLoadout.ClassAbilityOne.IsNone());
    TestEqual(TEXT("Existing Slipcut placement wins"), P.AbilityLoadout.ClassAbilityTwo, Slipcut);
    TestEqual(TEXT("Exactly one purchased token refunded"), P.UnspentAbilityTokens, 3);
    TestEqual(TEXT("Historical token entitlement stays stamped"), P.AbilityTokensGranted, 5);
    TestEqual(TEXT("Retired rank refunds its historical two-point cost"), P.UnspentDoctrinePoints, 2);
    TestEqual(TEXT("Other Doctrine ranks remain"), P.DoctrineNodeRanks.Num(), 3);
    TestEqual(TEXT("Commitment remains"), P.CommittedBranch, FName(TEXT("Swift.Kinetic")));
    TestEqual(TEXT("Only still-paid unlock remains"), P.UnlockedAbilityIds.Num(), 1);
    TestTrue(TEXT("Unrelated bought ability survives"), P.UnlockedAbilityIds.Contains(Lead));
    TestEqual(TEXT("Ultimate survives"), P.AbilityLoadout.Ultimate, FName(TEXT("Swift.Overdrive")));
    if (!TestTrue(TEXT("Pure step is itself idempotent"), UBreakerSaveGame::MigrateSwiftStarterV9ToV10(P, Note))) return false;
    if (!TestTrue(TEXT("Repeated full migration succeeds"), UBreakerSaveGame::MigrateToCurrent(*Paid, Note))) return false;
    TestEqual(TEXT("Repeated migration cannot mint a second token"), P.UnspentAbilityTokens, 3);
    TestEqual(TEXT("Repeated migration cannot mint Doctrine"), P.UnspentDoctrinePoints, 2);

    auto* Loaded = NewObject<UBreakerProgressionComponent>();
    Loaded->LoadProgressionState(P);
    TestEqual(TEXT("Normal progression load cannot refund the retired node again"), Loaded->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 2);
    TestEqual(TEXT("Normal progression load retains the one token refund"), Loaded->GetUnspentAbilityTokens(), 3);
    TestTrue(TEXT("Now-free starter remains available after normal load"), Loaded->IsAbilityUnlocked(Slipcut));

    auto* Moved = Legacy();
    Moved->Progression.AbilityLoadout.ClassAbilityOne = Lead;
    Moved->Progression.AbilityLoadout.ClassAbilityTwo = Skim;
    if (!TestTrue(TEXT("Moved retired starter migrates"), UBreakerSaveGame::MigrateToCurrent(*Moved, Note))) return false;
    TestEqual(TEXT("Other equipped ability retains its slot"), Moved->Progression.AbilityLoadout.ClassAbilityOne, Lead);
    TestEqual(TEXT("Retired second-slot starter becomes Slipcut in place"), Moved->Progression.AbilityLoadout.ClassAbilityTwo, Slipcut);

    auto* Empty = Legacy();
    Empty->Progression.AbilityLoadout.ClassAbilityOne = NAME_None;
    if (!TestTrue(TEXT("Empty loadout migrates"), UBreakerSaveGame::MigrateToCurrent(*Empty, Note))) return false;
    TestTrue(TEXT("Migration never refills intentionally cleared starter"), Empty->Progression.AbilityLoadout.ClassAbilityOne.IsNone());

    auto* Old = Legacy();
    Old->SaveVersion = 4;
    Old->Progression.CharacterLevel = 30;
    if (!TestTrue(TEXT("Frozen legacy grant chains through new retirement"), UBreakerSaveGame::MigrateToCurrent(*Old, Note))) return false;
    TestEqual(TEXT("V4 had never bought Slipcut, so receives no refund"), Old->Progression.UnspentAbilityTokens, 0);
    TestEqual(TEXT("Frozen v5 entitlement remains one"), Old->Progression.AbilityTokensGranted, 1);
    TestFalse(TEXT("Frozen Skim grant is retired by the later step"), Old->Progression.UnlockedAbilityIds.Contains(Skim));
    TestTrue(TEXT("Frozen Lead grant survives"), Old->Progression.UnlockedAbilityIds.Contains(Lead));

    auto* Current = Legacy();
    Current->SaveVersion = 10;
    Current->Progression.AbilityLoadout.ClassAbilityOne = Slipcut;
    if (!TestTrue(TEXT("Fresh v10 payload is already current"), UBreakerSaveGame::MigrateToCurrent(*Current, Note))) return false;
    TestEqual(TEXT("Fresh current starter never receives a token refund"), Current->Progression.UnspentAbilityTokens, 0);
    TestEqual(TEXT("Fresh current payload remains version ten"), Current->SaveVersion, 10);

    auto* Foreign = Legacy();
    Foreign->Progression.PermanentClass = EBreakerClassId::Caster;
    Foreign->Progression.UnlockedAbilityIds = {Slipcut};
    if (!TestTrue(TEXT("Foreign class advances without Swift rewriting"), UBreakerSaveGame::MigrateToCurrent(*Foreign, Note))) return false;
    TestEqual(TEXT("Foreign unknown ids remain verbatim"), Foreign->Progression.UnlockedAbilityIds.Num(), 1);
    TestEqual(TEXT("Foreign character gets no Swift refund"), Foreign->Progression.UnspentAbilityTokens, 0);

    auto* Invalid = Legacy();
    Invalid->Progression.UnlockedAbilityIds = {Slipcut};
    Invalid->Progression.DoctrineNodeRanks = {{TEXT("Swift.Kinetic.SkimDiscipline"), 2}};
    TestFalse(TEXT("Invalid retired rank is refused before partial refund"), UBreakerSaveGame::MigrateToCurrent(*Invalid, Note));
    TestEqual(TEXT("Refused migration retains version"), Invalid->SaveVersion, 9);
    TestEqual(TEXT("Refused migration retains paid unlock receipt"), Invalid->Progression.UnlockedAbilityIds.Num(), 1);
    TestEqual(TEXT("Refused migration retains original loadout"), Invalid->Progression.AbilityLoadout.ClassAbilityOne, Skim);
    TestEqual(TEXT("Refused migration pays nothing"), Invalid->Progression.UnspentAbilityTokens, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSwiftStarterRuntimeTest,
    "RiorsEdge.Abilities.SwiftStarterPaidRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSwiftStarterRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    auto* Movement = Player->GetBreakerMovement();
    Movement->SetComponentTickEnabled(false);
    auto* Attributes = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("Fresh character chooses actual Swift kit"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift))) return false;
    const auto Slot = EBreakerAbilitySlot::ClassAbilityOne;
    TestEqual(TEXT("Starter is Slipcut"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityOne, FName(TEXT("Swift.Slipcut")));
    TestTrue(TEXT("Second slot intentionally starts empty"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityTwo.IsNone());
    TestEqual(TEXT("Fresh starter needs no token purchase"), Progression->GetUnspentAbilityTokens(), 0);
    TestTrue(TEXT("Slipcut is unlocked through actual starter definition"), Progression->IsAbilityUnlocked(TEXT("Swift.Slipcut")));
    TestNull(TEXT("Retired Skim cannot resolve in the catalogue"), UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Skim")));
    auto* Abilities = Player->GetAbilities();
    Abilities->RefreshGrants();
    if (!TestTrue(TEXT("Registered starter grants through equipment"), Abilities->IsSlotGranted(Slot))) return false;
    auto* Momentum = Player->GetMomentum();
    Momentum->BindAttributes(Attributes);
    Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    const float Quoted = Abilities->GetCost(Slot);
    for (int32 Step = 0; Step < 40 && Momentum->GetMomentum() < Quoted; ++Step)
    {
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
        Momentum->AdvanceLoop(1);
    }
    Movement->StopMovementImmediately();
    const float BaseRate = Player->GetWeapon()->GetFireRateMultiplier();
    const float Before = Momentum->GetMomentum();
    if (!TestTrue(TEXT("Native movement earns starter resource cost"), Before >= Quoted)) return false;
    if (!TestTrue(TEXT("Fresh equipped Slipcut activates through actual slot"), Abilities->TryActivateSlot(Slot))) return false;
    TestEqual(TEXT("Starter still pays its actual resource cost"), Before - Momentum->GetMomentum(), Quoted, .001f);
    TestEqual(TEXT("Starter pays its actual weapon cadence lane"), Player->GetWeapon()->GetFireRateMultiplier(), BaseRate * 2.0f, .001f);
    TestTrue(TEXT("Native Slipcut window is active"), UBreakerAbilityStateComponent::FindOrAdd(Player)->IsWindowActive(UBreakerAbility_Slipcut::WindowKey()));
    return true;
}
#endif
