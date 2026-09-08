#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerGearManaSuspensionRuntimeTest,
    "RiorsEdge.Items.GearManaSuspensionRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerGearManaSuspensionRuntimeTest::RunTest(const FString& Parameters)
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
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attributes = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attributes);
    auto* Combat = Player->GetCombat();
    Combat->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression();
    Progression->BindAttributes(Attributes);
    if (!TestTrue(TEXT("Actual Caster class"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    auto* Mana = Player->GetMana();
    Mana->BindAttributes(Attributes);
    Mana->SetComponentTickEnabled(false);
    auto* Equipment = Player->GetEquipment();
    Equipment->BindAttributes(Attributes);
    Equipment->BindCombatEvents();
    Equipment->SetComponentTickEnabled(false);
    const auto Overrun = UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Overrun"), 1, 11);
    if (!TestTrue(TEXT("Actual rolled Overrun equips"), Equipment->EquipItem(Overrun))) return false;
    FBreakerItemInstance Sustain;
    bool Found = false;
    for (int32 Seed = 1; Seed <= 8192; ++Seed)
    {
        Sustain = UBreakerLootLibrary::RollItem(TEXT("Suspension.Sustain"), EBreakerEquipSlot::Gloves,
            EBreakerItemRarity::Exceptional, 1, Seed);
        const bool Life = Sustain.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& Line)
        { return Line.AffixId == TEXT("Core.LifeOnKill"); });
        const bool Resource = Sustain.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& Line)
        { return Line.AffixId == TEXT("Core.ResourceOnKill"); });
        if (Life && Resource) { Found = true; break; }
    }
    if (!TestTrue(TEXT("Actual ordinary sustain drop discovered"), Found)) return false;
    if (!TestTrue(TEXT("Unmodified sustain drop equips"), Equipment->EquipItem(Sustain))) return false;
    Player->GetBreakerMovement()->SetMovementMode(MOVE_Falling);
    Equipment->TickComponent(0, LEVELTICK_All, nullptr);
    const float GearRegen = Attributes->GetClassResourceRegen();
    if (!TestTrue(TEXT("Traversal activates real Overrun gear regeneration"), GearRegen > 0)) return false;
    const auto Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    if (!TestTrue(TEXT("Paid starter cast makes room in normal Mana bank"), ASC->TryActivateAbility(Fracture))) return false;
    const float BeforeRegen = Mana->GetMana();
    Equipment->TickComponent(.5f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Gear regeneration pays normally before Unmake"), Mana->GetMana() - BeforeRegen, GearRegen * .5f, .001f);
    Mana->AdvanceLoop(30);
    const auto Unmake = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Unmake::StaticClass(), 1));
    const float BeforeUltimate = Mana->GetMana();
    if (!TestTrue(TEXT("Actual paid Unmake activates"), ASC->TryActivateAbility(Unmake))) return false;
    TestEqual(TEXT("Ultimate paid ordinary eighty Mana"), BeforeUltimate - Mana->GetMana(), 80.0f, .001f);
    TestTrue(TEXT("Actual ultimate owns Mana suspension"), Mana->IsGenerationSuspended());
    const float SuspendedMana = Mana->GetMana();
    Equipment->TickComponent(.5f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Overrun cannot regenerate through Unmake"), Mana->GetMana(), SuspendedMana, .0001f);

    auto Kill = [&]()
    {
        auto* Target = World->SpawnActor<AActor>();
        auto* Victim = NewObject<UBreakerCombatComponent>(Target);
        Target->AddInstanceComponent(Victim);
        Victim->RegisterComponent();
        auto* Health = NewObject<UBreakerAttributeSet>(Target);
        Health->ApplyMaxHealth(10); Health->ApplyHealth(10);
        Victim->BindAttributes(Health);
        FBreakerDamageRequest Hit;
        Hit.BaseDamage = 20;
        Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Hit.bCanCritical = false; Hit.bCanBeAvoided = false;
        Hit.SetInstigator(Player);
        return Victim->ReceiveDamage(Hit).bKilled;
    };
    FBreakerDamageRequest Injury;
    Injury.BaseDamage = 30; Injury.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Injury.bCanCritical = false; Injury.bCanBeAvoided = false; Injury.bBypassShield = true;
    Combat->ReceiveDamage(Injury);
    const float BeforeHealing = Attributes->GetHealth();
    if (!TestTrue(TEXT("Real lethal damage dispatches equipment kill listener"), Kill())) return false;
    TestTrue(TEXT("Life on Kill remains active during Mana suspension"), Attributes->GetHealth() > BeforeHealing);
    TestEqual(TEXT("Resource on Kill cannot bypass suspension"), Mana->GetMana(), SuspendedMana, .0001f);
    ASC->CancelAbilityHandle(Unmake);
    TestFalse(TEXT("Cancel releases the actual suspension"), Mana->IsGenerationSuspended());
    const float BeforeResume = Mana->GetMana();
    Equipment->TickComponent(.5f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Gear regeneration resumes"), Mana->GetMana() - BeforeResume, GearRegen * .5f, .001f);
    const float BeforeKill = Mana->GetMana();
    if (!TestTrue(TEXT("Another actual kill after cancellation"), Kill())) return false;
    TestEqual(TEXT("Gear resource on kill resumes"), Mana->GetMana() - BeforeKill,
        Equipment->GetStats().ResourceOnKill, .001f);
    return true;
}
#endif
