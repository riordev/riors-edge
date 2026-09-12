#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPatienceRecoveryRuntimeTest,
    "RiorsEdge.Classes.Mana.PatienceAdditiveRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPatienceRecoveryRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
    // Restored entitlement fixture; only Patience's single rank is spent (O272).
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    auto* Mana = Player->GetMana(); Mana->BindAttributes(Attributes); Mana->SetComponentTickEnabled(false);
    auto* Weapon = Player->GetWeapon(); Weapon->BeginPlay(); Weapon->SetComponentTickEnabled(false);
    for (int32 Rank = 0; Rank <= 1; ++Rank)
    {
        FText Reason;
        if (Rank > 0 && !TestTrue(TEXT("actual Patience purchase"), Progression->PurchaseNode(
            UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), TEXT("Caster.VoidWhisperer.Patience"), Reason))) return false;
        Mana->AdvanceLoop(30);
        for (int32 Step = 0; Step < 20; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
        const int32 AmmoBefore = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        if (!TestEqual(TEXT("actual shot consumes a round"), Weapon->GetMagazineAmmo(), AmmoBefore - 1)) return false;
        if (!TestTrue(TEXT("real weapon shot resets the no-fire clock"), Weapon->GetLastShot().bFired)) return false;
        if (!TestTrue(TEXT("ordinary spend leaves recovery headroom"), Mana->TrySpendMana(90))) return false;
        const float Before = Mana->GetMana();
        // O272: the single rank's idle delay is two seconds. The unowned pass
        // reads the same window and pays only starter regeneration through it.
        const float Delay = 2.0f;
        Mana->AdvanceLoop(Delay - .25f);
        TestEqual(TEXT("before delay only starter regeneration pays"), Mana->GetMana() - Before, 11.0f * (Delay - .25f), .001f);
        const float AtBoundary = Mana->GetMana(); Mana->AdvanceLoop(.5f);
        TestEqual(TEXT("partial frame pays only eligible Patience time"), Mana->GetMana() - AtBoundary, 5.5f + (Rank > 0 ? .25f : 0), .001f);
        const float AfterBoundary = Mana->GetMana(); Mana->AdvanceLoop(1);
        TestEqual(TEXT("Patience improves rather than doubles starter recovery"), Mana->GetMana() - AfterBoundary, Rank > 0 ? 12.0f : 11.0f, .001f);
    }
    return true;
}
#endif
