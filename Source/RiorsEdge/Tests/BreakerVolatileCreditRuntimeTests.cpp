#include "Tests/BreakerVolatileCreditRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"

void UBreakerVolatileCreditRuntimeObserver::OnKill(const FBreakerHitContext& Hit) { Kills.Add(Hit); }
void UBreakerVolatileCreditRuntimeObserver::OnHit(const FBreakerHitContext& Hit) { Hits.Add(Hit); }

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// O245. A Volatile blast is authored by the corpse and earned by whoever
// popped it. The two halves are separately pinned here because conflating them
// is exactly the defect: crediting the player by naming him the INSTIGATOR
// would have handed the blast his progression and rescaled it, which O217
// forbids. So the test asserts BOTH that the player is paid AND that the
// number did not move.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerVolatileCreditRuntimeTest,
    "RiorsEdge.Combat.VolatileCreditRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerVolatileCreditRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };

    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr = Player->GetAttributes();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr);

    auto* Observer = NewObject<UBreakerVolatileCreditRuntimeObserver>();
    Player->GetCombat()->OnKillDealt.AddDynamic(Observer, &UBreakerVolatileCreditRuntimeObserver::OnKill);
    Player->GetCombat()->OnHitDealt.AddDynamic(Observer, &UBreakerVolatileCreditRuntimeObserver::OnHit);

    auto Spawn = [&](FVector P) -> ABreakerEnemy*
    {
        auto* E = World->SpawnActor<ABreakerEnemy>(P, FRotator::ZeroRotator);
        if (!E) return nullptr;
        E->SetAreaLevel(1);
        E->ConfigureCrowdProbe();
        E->DispatchBeginPlay();
        E->SetActorTickEnabled(false);
        if (auto* M = E->FindComponentByClass<UBreakerEnemyMovementComponent>()) M->SetComponentTickEnabled(false);
        if (auto* S = E->FindComponentByClass<UBreakerStatusComponent>()) S->SetComponentTickEnabled(false);
        return E;
    };
    auto Clock = [&](float Seconds)
    { for (int32 I = 0; I < FMath::CeilToInt(Seconds * 100); ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };

    const FVector Origin = Player->GetActorLocation() + FVector(2000, 0, 0);

    // ---- The Volatile, and one victim standing inside its inner radius -----
    auto* Bomb = Spawn(Origin);
    auto* Victim = Spawn(Origin + FVector(60, 0, 0));
    if (!Bomb || !Victim) return false;
    auto* Mods = Bomb->GetModifierComponent();
    if (!Mods) return false;
    if (!TestTrue(TEXT("the fixture can legally dress a Volatile"),
        Mods->SetModifiers({ EBreakerEnemyModifier::Volatile }))) return false;

    // The blast the corpse authors, computed from the MONSTER's own attack
    // exactly as the shipped rule does. Nothing about the player enters this.
    const float Authored = UBreakerEnemyModifierLibrary::GetVolatileDetonationDamage(
        Bomb->GetEffectiveAttackDamage(), Mods->Params);
    if (!TestTrue(TEXT("the fixture's Volatile actually carries a blast"), Authored > 0.0f)) return false;

    auto* VictimCombat = Victim->FindComponentByClass<UBreakerCombatComponent>();
    const auto* VictimAttr = Victim->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();
    if (!VictimCombat || !VictimAttr) return false;

    // ---- The player pops it -----------------------------------------------
    auto* BombCombat = Bomb->FindComponentByClass<UBreakerCombatComponent>();
    if (!BombCombat) return false;
    FBreakerDamageRequest Lethal;
    Lethal.BaseDamage = BombCombat->GetMaxHealth() * 2.0f;
    Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Lethal.bCanCritical = false;
    Lethal.bCanBeAvoided = false;
    Lethal.SetInstigator(Player);
    BombCombat->ReceiveDamage(Lethal);
    TestTrue(TEXT("the Volatile died to the player's hit"), BombCombat->IsDead());
    TestEqual(TEXT("the corpse remembers who landed the killing blow"),
        BombCombat->GetLastDamageInstigator(), static_cast<AActor*>(Player));

    // CAPTURED HERE, NOT BEFORE THE KILL. An ordinary enemy chain-detonates on
    // death for 35% of its max health against every enemy within 420 cm
    // (ABreakerEnemy::HandleDeath), so the victim has already lost 77 to the
    // corpse by this line. Reading its health before the kill meant the blast
    // assertion below passed on the CHAIN alone and would have stayed green
    // with the Volatile blast entirely dead — the test measuring something it
    // was not testing. The baseline is taken after the chain has been paid, so
    // what it measures from here is the blast and only the blast.
    const float VictimHealthBefore = VictimAttr->GetHealth();

    const int32 KillsBeforeFuse = Observer->Kills.Num();
    // ABreakerEnemy::Tick drives AdvanceModifiers and the fixture disables the
    // actor tick, so the fuse is advanced explicitly here — the same way
    // BreakerBlackoutProtocolTests drives an aura. Ticking the world alone
    // would leave the fuse lit forever and prove nothing.
    Clock(0.1f);
    Mods->AdvanceModifiers(Mods->Params.VolatileFuseSeconds + 0.1f);
    Clock(0.1f);
    TestTrue(TEXT("the blast damaged the victim at all"), VictimAttr->GetHealth() < VictimHealthBefore);

    const FBreakerHitContext* BlastHit = Observer->Hits.FindByPredicate(
        [&](const FBreakerHitContext& H) { return H.Target == Victim; });
    if (!TestNotNull(TEXT("the player is credited with the blast's HIT on the victim"), BlastHit)) return false;

    // The whole point: paid to the player, authored by the corpse.
    TestEqual(TEXT("the hit is credited to the player who popped it"),
        BlastHit->CreditedTo.Get(), static_cast<AActor*>(Player));
    TestEqual(TEXT("the hit is still AUTHORED by the corpse (O217's one number)"),
        BlastHit->Instigator.Get(), static_cast<AActor*>(Bomb));

    // Unconditional on purpose. A default Volatile is 9x a chassis attack and
    // a trash body inside the inner radius does not survive it — that is the
    // shipped configuration, not a fixture indulgence. If content ever makes
    // the blast survivable this goes red, which is the correct outcome: the
    // whole question "does popping a Volatile pay the player" would need
    // re-asking.
    if (!TestTrue(TEXT("the blast killed the victim outright (shipped Volatile vs trash)"),
        VictimCombat->IsDead())) return false;
    TestTrue(TEXT("a blast KILL pays the player's kill hooks, not the corpse's"),
        Observer->Kills.Num() > KillsBeforeFuse);
    const FBreakerHitContext& Kill = Observer->Kills.Last();
    TestEqual(TEXT("the credited kill is the victim's"), Kill.Target.Get(), static_cast<AActor*>(Victim));

    // ---- The control: an unattributed death credits nobody new -------------
    auto* LoneBomb = Spawn(Origin + FVector(0, 4000, 0));
    auto* LoneVictim = Spawn(Origin + FVector(60, 4000, 0));
    if (!LoneBomb || !LoneVictim) return false;
    auto* LoneMods = LoneBomb->GetModifierComponent();
    if (!LoneMods || !LoneMods->SetModifiers({ EBreakerEnemyModifier::Volatile })) return false;
    auto* LoneCombat = LoneBomb->FindComponentByClass<UBreakerCombatComponent>();
    if (!LoneCombat) return false;

    const int32 HitsBeforeHazard = Observer->Hits.Num();
    FBreakerDamageRequest Hazard;                 // no instigator at all
    Hazard.BaseDamage = LoneCombat->GetMaxHealth() * 2.0f;
    Hazard.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Hazard.bCanCritical = false;
    Hazard.bCanBeAvoided = false;
    LoneCombat->ReceiveDamage(Hazard);
    TestNull(TEXT("a hazard death names no killer"), LoneCombat->GetLastDamageInstigator());
    LoneMods->AdvanceModifiers(LoneMods->Params.VolatileFuseSeconds + 0.1f);
    Clock(0.1f);
    TestEqual(TEXT("an unattributed blast credits the player with nothing"),
        Observer->Hits.Num(), HitsBeforeHazard);

    return true;
}

#endif
