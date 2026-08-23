// SHIPPED-CONFIGURATION TESTS FOR THE BOOT FLOW (O40c), in the
// RiorsEdge.Movement.JumpGrantMatrix mold: every assertion below is about the
// configuration the game actually boots with, not about a rule in the
// abstract. This suite exists because the same failure shape shipped three
// times — JumpGrant, PowerBand, the keystone budget — and then a fourth: the
// three-map flow landed with maps the shipped boot path could not produce a
// pawn in (no PlayerStart), and with a title menu that re-opened, paused, on
// every single map arrival (D1). All of it behind a green suite, because
// nothing asserted the boot path AS CONFIGURED.
//
// What a headless test genuinely cannot cover — actually loading the three
// maps and walking front end → Anchor → gym — is covered by the capture
// harness (-game -BreakerAutoPlay and a plain -game boot); this suite pins
// every decision on that path that is expressible without a world.

#include "Misc/AutomationTest.h"

#include "Characters/BreakerCharacter.h"
#include "Game/BreakerGameInstance.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerCharacterRoster.h"
#include "Save/BreakerSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBootFlowConfigTest,
    "RiorsEdge.Game.BootFlow.ShippedConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBootFlowConfigTest::RunTest(const FString& Parameters)
{
    // ---- The boot map is the front end, and it exists -----------------------
    // GameDefaultMap governs a standalone/packaged launch. If it names a map
    // that is not the front end, the whole front door is bypassed; if it names
    // a map that does not exist, the game boots into nothing.
    FString GameDefaultMap;
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"),
        TEXT("GameDefaultMap"), GameDefaultMap, GEngineIni);
    TestTrue(TEXT("GameDefaultMap names the front end"),
        GameDefaultMap.Contains(UBreakerGameInstance::FrontEndMapName()));

    // ---- The three maps the code compares names against are real assets ----
    // Map identity is a string match against the .umap short name
    // (BreakerGameInstance.h): renaming an asset without editing those
    // functions is a silent no-op where the map just becomes "the gym". This
    // pins the other direction — the names the code holds must resolve to
    // packages on disk.
    for (const TCHAR* MapName : { UBreakerGameInstance::FrontEndMapName(),
        UBreakerGameInstance::AnchorMapName(), UBreakerGameInstance::GymMapName() })
    {
        const FString PackagePath = FString::Printf(TEXT("/Game/Breaker/Maps/%s"), MapName);
        TestTrue(*FString::Printf(TEXT("Map package '%s' exists"), *PackagePath),
            FPackageName::DoesPackageExist(PackagePath));
    }

    // ---- The D1 guard matrix ------------------------------------------------
    // The title menu belongs to sessions that have not entered the world yet.
    // The third row is the shipped bug: arriving in the Anchor mid-session
    // re-opened the title screen, paused, on every level transition.
    TestTrue(TEXT("Front end, character chosen (RETURN TO TITLE): menu shows"),
        ABreakerCharacter::ShouldShowInitialMenu(/*bIsFrontEndMap=*/true, /*bSessionHasActiveCharacter=*/true));
    TestTrue(TEXT("Front end, fresh session: menu shows"),
        ABreakerCharacter::ShouldShowInitialMenu(true, false));
    TestFalse(TEXT("Mid-session map arrival: MENU MUST NOT SHOW (D1)"),
        ABreakerCharacter::ShouldShowInitialMenu(false, true));
    TestTrue(TEXT("PIE drop-in with no character: menu shows (the daily workflow)"),
        ABreakerCharacter::ShouldShowInitialMenu(false, false));

    // ---- Travel closes the loop both ways ----------------------------------
    // HandleHubTravelSelected dispatches on exactly these two ids; a registry
    // that drops either one strands the player on one side of the gate.
    FBreakerTravelDestination Destination;
    TestTrue(TEXT("The gym destination is registered"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::GymDestinationId, Destination));
    TestTrue(TEXT("The gym destination is enabled"), Destination.bEnabled);
    TestTrue(TEXT("The hub destination is registered (the way back)"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::HubDestinationId, Destination));
    TestTrue(TEXT("The hub destination is enabled"), Destination.bEnabled);

    // ---- The EnterWorld dispatch matrix ------------------------------------
    // Entering a character travels into the Anchor (landing at the hub gate)
    // from the front end AND from any mid-session switch. The third row is the
    // shipped bug: switching characters loaded the new save onto the old
    // character's pawn, at the old character's location ("when i select a new
    // character i just immediately go to where my first character was").
    TestTrue(TEXT("PLAY from the front end travels"),
        ABreakerCharacter::ShouldTravelOnEnterWorld(/*bIsFrontEndMap=*/true, /*bHadMidSessionCharacter=*/false));
    TestTrue(TEXT("RETURN TO TITLE on the front end, then PLAY: travels"),
        ABreakerCharacter::ShouldTravelOnEnterWorld(true, true));
    TestTrue(TEXT("Mid-session character switch: MUST TRAVEL, never load in place"),
        ABreakerCharacter::ShouldTravelOnEnterWorld(false, true));
    TestFalse(TEXT("PIE drop-in first pick on a template map stays in place (the daily workflow)"),
        ABreakerCharacter::ShouldTravelOnEnterWorld(false, false));

    // ---- O100: the quartermaster is an ANCHOR interaction -------------------
    // Two halves, and both matter. REACHABLE: the shipped hub spawns the NPC
    // and its dialogue carries the one choice that opens the screen — a screen
    // with no door is the reachability rule's own example. NOT REACHABLE
    // ANYWHERE ELSE: no tab-strip entry and no pause-menu path, which is what
    // "Anchor interaction" means in this codebase and what the Forge tab
    // currently violates.
    {
        bool bFoundDoor = false;
        for (const FBreakerDialogueNode& Node : ABreakerNPC::MakeQuartermasterDialogue())
        {
            for (const FBreakerDialogueChoice& Choice : Node.Choices)
            {
                if (Choice.Action == EBreakerDialogueAction::OpenQuartermaster) bFoundDoor = true;
            }
        }
        TestTrue(TEXT("The quartermaster's dialogue opens the unlock screen"), bFoundDoor);

        // O42/content-and-modes: the Forge is the same kind of interaction and
        // was the inconsistency beside it — in the tab strip the pause menu
        // opens. Kess is its only door now, on every entry state she has.
        bool bFoundForgeDoor = false;
        for (const FBreakerDialogueNode& Node : ABreakerNPC::MakeForgeKeeperDialogue())
        {
            for (const FBreakerDialogueChoice& Choice : Node.Choices)
            {
                if (Choice.Action == EBreakerDialogueAction::OpenForge) bFoundForgeDoor = true;
            }
        }
        TestTrue(TEXT("Kess's dialogue opens the Forge"), bFoundForgeDoor);

        // The absence half is a source scan, because "no button anywhere reaches
        // this" is a statement about the whole menu file rather than about any
        // object a test can hold. The tab strip is built by AddTab and the pause
        // menu by BuildPauseScreen; a Quartermaster entry in either is the
        // defect.
        const FString MenuPath = FPaths::Combine(FPaths::ProjectDir(),
            TEXT("Source"), TEXT("RiorsEdge"), TEXT("UI"), TEXT("BreakerMenu.cpp"));
        FString Menu;
        if (FFileHelper::LoadFileToString(Menu, *MenuPath))
        {
            TestFalse(TEXT("The quartermaster has no tab-strip entry"),
                Menu.Contains(TEXT("AddTab(TEXT(\"QUARTERMASTER\")")));
            TestFalse(TEXT("The Forge has no tab-strip entry either"),
                Menu.Contains(TEXT("AddTab(TEXT(\"FORGE\")")));
            const int32 PauseBegin = Menu.Find(TEXT("SBreakerMenu::BuildPauseScreen"));
            if (PauseBegin != INDEX_NONE)
            {
                // The pause screen's own body only — a later screen mentioning
                // the value is not a pause-menu path.
                const int32 PauseEnd = Menu.Find(TEXT("TSharedRef<SWidget> SBreakerMenu::"), ESearchCase::CaseSensitive,
                    ESearchDir::FromStart, PauseBegin + 40);
                const FString PauseBody = Menu.Mid(PauseBegin, (PauseEnd == INDEX_NONE ? Menu.Len() : PauseEnd) - PauseBegin);
                TestFalse(TEXT("The pause menu has no path to the quartermaster"),
                    PauseBody.Contains(TEXT("Quartermaster")));
                TestFalse(TEXT("The pause menu has no path to the Forge"),
                    PauseBody.Contains(TEXT("Forge")));
            }
        }
        else
        {
            AddInfo(TEXT("Menu source not present (packaged build?); the absence scan was skipped."));
        }
    }

    // A travel point never offers the place the player already is.
    ABreakerTravelPoint* GymGate = NewObject<ABreakerTravelPoint>();
    GymGate->ExcludedDestinationId = ABreakerTravelPoint::GymDestinationId;
    for (const FBreakerTravelDestination& Offered : GymGate->GetAvailableDestinations())
    {
        TestNotEqual(TEXT("The gym's gate does not offer the gym"),
            Offered.Id, ABreakerTravelPoint::GymDestinationId);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLevelPointEntitlementTest,
    "RiorsEdge.Progression.LevelPointEntitlement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLevelPointEntitlementTest::RunTest(const FString& Parameters)
{
    // O111: ONE PER-LEVEL CURRENCY. 1 Core Point per level to 50, with the
    // slice lump (12) as an ADVANCE on that entitlement rather than a bonus
    // beside it. The Class Point ladder is gone and the pool it filled is
    // asserted at ZERO below -- that assertion is what stops it coming back,
    // and it is why these rows were rewritten rather than deleted. The doctrine
    // pool has no ladder at all: its eight arrive whole at commitment, which
    // Progression.SubclassCommitment covers. Run at the shipped grants, never
    // at an inflated test budget -- granting 30 points in a rig is exactly how
    // the keystone budget contradiction stayed invisible.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    Progression->ApplySliceDefaultsIfFresh();

    TestEqual(TEXT("A fresh character holds exactly the slice Core budget"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints),
        UBreakerProgressionLibrary::SliceCorePointGrant);
    TestEqual(TEXT("The retired class pool is empty on a fresh character"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints_Retired), 0);
    TestEqual(TEXT("An uncommitted character holds no doctrine points"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);

    // Levels 2..10 are pre-paid by the lump: no extra points may arrive.
    const FBreakerExperienceCurve Curve;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10, Curve));
    TestEqual(TEXT("Level 10 still holds the slice budget (lump = advance, not bonus)"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints),
        UBreakerProgressionLibrary::SliceCorePointGrant);

    // Level 13 is the first level past the lump: the entitlement is
    // min(Level, 50) and 12 are already advanced, so the 13th level pays the
    // 13th point. This used to be the level-11 keystone beat, which was
    // arithmetic on a Class Point budget that no longer exists -- the keystone
    // question moved to the doctrine pool, where eight arrive at once.
    Progression->AwardExperience(
        UBreakerExperienceLibrary::TotalXpToReachLevel(13, Curve) - Progression->GetTotalExperience());
    TestEqual(TEXT("Level 13 holds 13 Core Points"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 13);

    // The one exhaustion beat left: Core Points run to 50 and stop.
    Progression->AwardExperience(
        UBreakerExperienceLibrary::TotalXpToReachLevel(50, Curve) - Progression->GetTotalExperience());
    TestEqual(TEXT("Level 50 holds 50 Core Points"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 50);
    // THE RULING, ASSERTED. No level, no lump and no settle-up may put a point
    // back in the retired pool, and levelling alone never pays a doctrine point.
    TestEqual(TEXT("The retired class pool is still empty at the cap"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints_Retired), 0);
    TestEqual(TEXT("Levelling alone never pays a doctrine point"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCreatedCharacterKeepsItsClassTest,
    "RiorsEdge.Game.BootFlow.CreatedCharacterKeepsItsClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCreatedCharacterKeepsItsClassTest::RunTest(const FString& Parameters)
{
    // THE OWNER'S BUG, AS SHIPPED ORDERING: "i made a caster and had swifts
    // skill tree and abilities". Create writes a Caster save through the
    // roster; the arriving pawn's progression component runs BeginPlay FIRST
    // (which used to auto-lock Swift and take Swift's class definition) and
    // loads the save SECOND. This test replays exactly that ordering at the
    // component level: the worst case — the dev auto-lock has already fired —
    // and then the roster-shaped Caster state loads over it. Every reader the
    // front end trusts must answer CASTER afterwards.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    // A test rig has no game instance, so the dev default still auto-locks
    // Swift — deliberately: that is the stale definition the load must evict.
    Progression->ApplySliceDefaultsIfFresh();
    TestEqual(TEXT("Precondition: the fresh rig auto-locked Swift"),
        Progression->GetProgressionState().PermanentClass, EBreakerClassId::Swift);

    // The state UBreakerCharacterRoster::CreateCharacter writes: class and
    // level only, loadout all-None, empty economy.
    FBreakerProgressionState Created;
    Created.PermanentClass = EBreakerClassId::Caster;
    Created.CharacterLevel = 1;
    Progression->LoadProgressionState(Created);

    const UBreakerClassDefinition* Caster =
        UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Caster);
    const UBreakerClassDefinition* Swift =
        UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Swift);
    if (!Caster || !Swift || Caster->StarterAbilityIds.Num() == 0 || Swift->StarterAbilityIds.Num() == 0)
    {
        AddError(TEXT("Fallback class definitions missing — cannot exercise the create path."));
        return false;
    }

    const FBreakerProgressionState& State = Progression->GetProgressionState();
    TestEqual(TEXT("A created Caster IS a Caster after the load"),
        State.PermanentClass, EBreakerClassId::Caster);

    // The two readers the D-fix comment names answer from ClassDefinition, so
    // the loadout seeding and the unlock answer are the honest probes of it.
    TestEqual(TEXT("Ability slot one seeds from CASTER's starters, not the stale Swift definition"),
        State.AbilityLoadout.ClassAbilityOne, Caster->StarterAbilityIds[0]);
    TestEqual(TEXT("The ultimate seeds from CASTER's kit"),
        State.AbilityLoadout.Ultimate, Caster->BaseUltimateId);
    TestTrue(TEXT("A Caster starter ability is unlocked"),
        Progression->IsAbilityUnlocked(Caster->StarterAbilityIds[0]));
    TestFalse(TEXT("A Swift starter ability is NOT unlocked on a created Caster"),
        Progression->IsAbilityUnlocked(Swift->StarterAbilityIds[0]));

    // The skill screen enumerates trees here; none of them may belong to Swift.
    for (const UBreakerProgressionTree* Tree : Progression->GetAvailableTrees())
    {
        if (!Tree) continue;
        TestNotEqual(*FString::Printf(TEXT("Tree '%s' offered to a created Caster is not a Swift tree"),
            *Tree->TreeId.ToString()), Tree->RequiredClass, EBreakerClassId::Swift);
    }

    // And the economy still seeds: a created character must arrive spendable.
    TestEqual(TEXT("A created Caster arrives with the slice Core Point budget"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints),
        UBreakerProgressionLibrary::SliceCorePointGrant);
    return true;
}
