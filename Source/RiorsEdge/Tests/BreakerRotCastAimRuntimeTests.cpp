#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Tests/BreakerRotAimRuntimeObserver.h"

// ---------------------------------------------------------------------------
// O266, the half of the rule that is a PROMISE: the wind-up delays the
// resolution, not the press. Owner: "rot has a tendency to ... go off at
// another target location; the cast should appear where you originally cast
// it at the start of your cast time", and "theres a ding everytime you press
// it but it shouldnt do that if you dont cast it". Rot read its aim at the
// landing and the HUD heard the press. These are a real world, a real
// controller turned a quarter turn during a real wind-up, a real hit landing
// inside one, and the puddle and the cue asked when and where they went.
// ---------------------------------------------------------------------------
namespace
{
    // The interrupt fixture's rig: a Caster with Rot in its starter slot,
    // equipped through the same API the picker uses — no token, no level,
    // nothing the game does not hand a fresh Caster.
    struct FBreakerRotAimRig
    {
        ABreakerCharacter* Caster = nullptr;
        APlayerController* Controller = nullptr;
        UBreakerAbilityComponent* Abilities = nullptr;
        UBreakerAbilityStateComponent* State = nullptr;
        UBreakerManaComponent* Mana = nullptr;
        float WindUp = 0.0f;
    };

    ABreakerCharacter* BreakerRotAimSpawnCharacter(UWorld* World, const FVector& At, const FRotator& Facing)
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ABreakerCharacter* Actor = World->SpawnActor<ABreakerCharacter>(At, Facing, Spawn);
        if (!Actor) return nullptr;
        Actor->SetActorTickEnabled(false);
        Actor->GetBreakerMovement()->SetComponentTickEnabled(false);
        UAbilitySystemComponent* ASC = Actor->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Actor, Actor);
        ASC->AddAttributeSetSubobject(Actor->GetAttributes());
        Actor->GetCombat()->BindAttributes(Actor->GetAttributes());
        Actor->GetProgression()->BindAttributes(Actor->GetAttributes());
        return Actor;
    }

    bool BreakerRotAimBuildRig(FAutomationTestBase& Test, UWorld* World, FBreakerRotAimRig& Out)
    {
        const FVector Origin(0, 0, 0);
        Out.Caster = BreakerRotAimSpawnCharacter(World, Origin, FRotator::ZeroRotator);
        Out.Controller = World->SpawnActor<APlayerController>();
        if (!Out.Caster || !Out.Controller) return false;
        Out.Controller->Possess(Out.Caster);
        Out.Controller->SetViewTarget(Out.Caster);
        Out.Controller->SetInitialLocationAndRotation(Origin, FRotator::ZeroRotator);
        if (!Test.TestTrue(TEXT("Caster locks its class"), Out.Caster->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
        Out.Mana = Out.Caster->GetMana();
        Out.Mana->BindAttributes(Out.Caster->GetAttributes());
        Out.State = UBreakerAbilityStateComponent::FindOrAdd(Out.Caster);
        Out.State->RegisterAllComponentTickFunctions(true);
        Out.State->SetComponentTickEnabled(true);
        if (!Out.State->HasBegunPlay()) Out.State->BeginPlay();
        Out.Abilities = Out.Caster->GetAbilities();
        FText Reason;
        if (!Test.TestTrue(TEXT("Rot equips"), Out.Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityTwo, TEXT("Caster.Rot"), Reason))) return false;
        Out.Abilities->RefreshGrants();
        Out.WindUp = BreakerAuthoredCastSeconds(TEXT("Caster.Rot"));
        return Test.TestTrue(TEXT("Rot authors a wind-up"), Out.WindUp > 0.05f);
    }

    // Every live Rot puddle this caster owns in this world. The zone list is
    // process-wide, so a stale pointer from another fixture's world is
    // filtered rather than counted.
    TArray<ABreakerZoneActor*> BreakerRotAimZonesOf(const UWorld* World, const AActor* Caster)
    {
        TArray<ABreakerZoneActor*> Found;
        for (const TWeakObjectPtr<ABreakerZoneActor>& Held : ABreakerZoneActor::GetLiveZones())
            if (ABreakerZoneActor* Zone = Held.Get())
                if (Zone->GetWorld() == World && Zone->GetZoneInstigator() == Caster && Zone->GetRemainingDuration() > 0.0f)
                    Found.Add(Zone);
        return Found;
    }

    // The live Rot instance on the rig's ASC — the object that holds the cast
    // queue (O271). Found the way BreakerAnyCastPending finds a caster, so a
    // test asks the ability itself whether it took the press.
    UBreakerAbility_Rot* BreakerRotAimInstance(AActor* Caster)
    {
        const UAbilitySystemComponent* ASC = Caster ? Caster->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
        if (!ASC) return nullptr;
        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
            if (UBreakerAbility_Rot* Rot = Cast<UBreakerAbility_Rot>(Spec.GetPrimaryInstance())) return Rot;
        return nullptr;
    }

    // Point the reticle. Rot reads the controller's view point. The camera
    // manager only serves that from its cache for a local player, and this
    // rig has none, so the controller answers with the pawn's own facing —
    // both are turned, and the solve reads the same yaw on either path. Yaw
    // only: the puddle's horizontal line is the thing under test.
    void BreakerRotAimPoint(APlayerController& Controller, ABreakerCharacter& Caster, float YawDegrees)
    {
        const FRotator Facing(0.0f, YawDegrees, 0.0f);
        Controller.SetControlRotation(Facing);
        Caster.SetActorRotation(Facing);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRotAimLockedAtPressTest,
    "RiorsEdge.Abilities.CastTime.RotAimLockedAtPress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRotAimLockedAtPressTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto Clock = [&](float Seconds) { for (float T = 0; T < Seconds; T += .01f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };

    FBreakerRotAimRig Rig;
    if (!BreakerRotAimBuildRig(*this, World, Rig)) return false;
    Clock(0.05f);

    // --- Pressed at +X, turned to +Y during the wind-up ---------------------
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 0.0f);
    if (!TestTrue(TEXT("Rot activates"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
    TestTrue(TEXT("the press opens a wind-up rather than a puddle"), BreakerAnyCastPending(Rig.Caster));
    TestEqual(TEXT("no puddle exists at the press"), BreakerRotAimZonesOf(World, Rig.Caster).Num(), 0);

    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 90.0f);
    BreakerResolvePendingCast(World, Rig.Caster, Rig.WindUp + 0.5f);
    TestFalse(TEXT("the wind-up has landed"), BreakerAnyCastPending(Rig.Caster));

    const TArray<ABreakerZoneActor*> FirstLanding = BreakerRotAimZonesOf(World, Rig.Caster);
    if (!TestEqual(TEXT("exactly one puddle after the landing"), FirstLanding.Num(), 1)) return false;
    const FVector Feet = Rig.Caster->GetActorLocation();
    const FVector First = FirstLanding[0]->GetActorLocation();
    // On the +X line the press pointed down: the empty world has nothing to
    // hit, so the puddle sits at the end of the reticle line, well out along
    // X and on the caster's own Y. A puddle that followed the turn would sit
    // out along Y instead.
    TestTrue(TEXT("the puddle went out along +X, where the press pointed"), First.X > Feet.X + 1000.0f);
    TestTrue(TEXT("and stayed on the caster's Y line, not the reticle's drift"), FMath::Abs(First.Y - Feet.Y) < 5.0f);

    // --- The control: an undisturbed +Y press lands on +Y --------------------
    // Without this the assertion above passes just as well against a rig
    // whose turn never reached the aim solve at all.
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 90.0f);
    if (!TestTrue(TEXT("Rot activates again"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
    BreakerResolvePendingCast(World, Rig.Caster, Rig.WindUp + 0.5f);
    const TArray<ABreakerZoneActor*> SecondLanding = BreakerRotAimZonesOf(World, Rig.Caster);
    if (!TestEqual(TEXT("a second, separate puddle"), SecondLanding.Num(), 2)) return false;
    const ABreakerZoneActor* Second = SecondLanding[0] == FirstLanding[0] ? SecondLanding[1] : SecondLanding[0];
    TestTrue(TEXT("the undisturbed press went out along +Y"), Second->GetActorLocation().Y > Feet.Y + 1000.0f);
    TestTrue(TEXT("and stayed on the caster's X line"), FMath::Abs(Second->GetActorLocation().X - Feet.X) < 50.0f);
    return true;
}

// ---------------------------------------------------------------------------
// O178: KIT owns WHEN the cue fires, and for a wind-up that is the landing.
// OnAbilityActivated is the broadcast the HUD plays the cast cue off; it used
// to fire the moment the press returned, 0.6 s before Rot existed and before
// any hit had its say.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRotCueOnLandingRuntimeTest,
    "RiorsEdge.Abilities.CastTime.CueOnLandingRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRotCueOnLandingRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto Clock = [&](float Seconds) { for (float T = 0; T < Seconds; T += .01f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };

    FBreakerRotAimRig Rig;
    if (!BreakerRotAimBuildRig(*this, World, Rig)) return false;
    // Something to land the interrupting hit from, as the interrupt fixture
    // does it: a second character, not the caster hitting itself.
    ABreakerCharacter* Target = BreakerRotAimSpawnCharacter(World, FVector(200, 0, 0), FRotator(0, 180, 0));
    if (!Target) return false;
    Clock(0.05f);

    UBreakerRotAimRuntimeObserver* Cue = NewObject<UBreakerRotAimRuntimeObserver>();
    Cue->AddToRoot();
    ON_SCOPE_EXIT { Cue->RemoveFromRoot(); };
    Rig.Abilities->OnAbilityActivated.AddDynamic(Cue, &UBreakerRotAimRuntimeObserver::OnActivated);

    // --- The press is silent; the landing is the cue --------------------------
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 0.0f);
    if (!TestTrue(TEXT("Rot activates"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
    if (!TestTrue(TEXT("the press opens a wind-up"), BreakerAnyCastPending(Rig.Caster))) return false;
    TestEqual(TEXT("no cue at the press"), Cue->Count, 0);
    BreakerResolvePendingCast(World, Rig.Caster, Rig.WindUp + 0.5f);
    TestFalse(TEXT("the wind-up has landed"), BreakerAnyCastPending(Rig.Caster));
    TestEqual(TEXT("one cue, at the landing"), Cue->Count, 1);
    TestEqual(TEXT("and the landing is real: a puddle exists"), BreakerRotAimZonesOf(World, Rig.Caster).Num(), 1);

    // --- An interrupted cast never cues -------------------------------------
    if (!TestTrue(TEXT("Rot activates again"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
    TestEqual(TEXT("still no cue at the second press"), Cue->Count, 1);
    // A real hit lands on the caster a third of the way into the wind-up.
    Clock(Rig.WindUp * .33f);
    FBreakerDamageRequest Hit;
    Hit.BaseDamage = 12.0f;
    Hit.bCanCritical = false;
    Hit.DamageFamily = EBreakerDamageFamily::Physical;
    Hit.SetInstigator(Target);
    const FBreakerDamageResult Landed = Rig.Caster->GetCombat()->ReceiveDamage(Hit);
    if (!TestTrue(TEXT("the interrupting hit actually lands"), Landed.HealthDamage + Landed.ShieldDamage > 0.0f)) return false;
    TestFalse(TEXT("the hit ended the cast"), BreakerAnyCastPending(Rig.Caster));
    // Past where the puddle would have landed.
    Clock(Rig.WindUp + .10f);
    TestEqual(TEXT("an interrupted cast is never announced"), Cue->Count, 1);
    TestEqual(TEXT("and never lands"), BreakerRotAimZonesOf(World, Rig.Caster).Num(), 1);
    return true;
}

// ---------------------------------------------------------------------------
// O271 (a): a Rot recast spawns a new puddle — nothing merges into a live one.
// VW4's anti-stack rule used to refresh a live own Rot within half a radius of
// the new aim instead of spawning, and the owner felt the second press as a
// press that did nothing. Two casts aimed at the SAME spot, each resolved,
// are two live puddles. RotAimLockedAtPress already lands two on different
// lines; this is the case the merge caught.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRotRecastSpawnsNewTest,
    "RiorsEdge.Abilities.Rot.RecastSpawnsNew",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRotRecastSpawnsNewTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto Clock = [&](float Seconds) { for (float T = 0; T < Seconds; T += .01f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };

    FBreakerRotAimRig Rig;
    if (!BreakerRotAimBuildRig(*this, World, Rig)) return false;
    Clock(0.05f);

    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 0.0f);
    if (!TestTrue(TEXT("the first Rot activates"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
    BreakerResolvePendingCast(World, Rig.Caster, Rig.WindUp + 0.5f);
    const TArray<ABreakerZoneActor*> FirstLanding = BreakerRotAimZonesOf(World, Rig.Caster);
    if (!TestEqual(TEXT("one puddle after the first landing"), FirstLanding.Num(), 1)) return false;
    ABreakerZoneActor* First = FirstLanding[0];
    const FVector FirstCenter = First->GetActorLocation();

    // The same aim, a moment later. Under the old rule this landed inside the
    // first puddle's refresh fraction and became a refresh of it.
    Clock(0.2f);
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 0.0f);
    if (!TestTrue(TEXT("the recast activates"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
    BreakerResolvePendingCast(World, Rig.Caster, Rig.WindUp + 0.5f);

    const TArray<ABreakerZoneActor*> SecondLanding = BreakerRotAimZonesOf(World, Rig.Caster);
    if (!TestEqual(TEXT("two live puddles: the recast spawned rather than merged"), SecondLanding.Num(), 2)) return false;
    ABreakerZoneActor* Second = SecondLanding[0] == First ? SecondLanding[1] : SecondLanding[0];
    TestTrue(TEXT("the second is a different actor"), Second != First);
    // Inside half the first's radius: the exact footprint the old rule folded
    // a recast into.
    TestTrue(TEXT("and it sits on the same spot, well inside the first's radius"),
        FVector::Dist2D(Second->GetActorLocation(), FirstCenter) < First->GetSpec().RadiusCm * 0.5f);
    return true;
}

// ---------------------------------------------------------------------------
// O271 (b): a press during a wind-up queues ONE cast, fired when the wind-up
// resolves; the aim is solved at the queued press. Aim +X and press; turn to
// +Y and press inside the wind-up; turn to -X and press again. The first
// lands on +X, a second wind-up is already running, and it lands on +Y — the
// third press went nowhere.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerQueuedPressLandsWhereItPointedTest,
    "RiorsEdge.Abilities.CastTime.QueuedPressLandsWhereItPointed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerQueuedPressLandsWhereItPointedTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto Clock = [&](float Seconds) { for (float T = 0; T < Seconds; T += .01f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };

    FBreakerRotAimRig Rig;
    if (!BreakerRotAimBuildRig(*this, World, Rig)) return false;
    Clock(0.05f);
    UBreakerAbility_Rot* Rot = BreakerRotAimInstance(Rig.Caster);
    if (!TestNotNull(TEXT("the rig has a live Rot instance"), Rot)) return false;
    const FVector Feet = Rig.Caster->GetActorLocation();
    const float Cost = Rig.Abilities->GetCost(EBreakerAbilitySlot::ClassAbilityTwo);
    TestTrue(TEXT("Rot has a price"), Cost > 0.0f);

    // --- Press on +X: the wind-up opens ---------------------------------------
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 0.0f);
    if (!TestTrue(TEXT("the first press activates"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo))) return false;
    if (!TestTrue(TEXT("and opens a wind-up"), BreakerAnyCastPending(Rig.Caster))) return false;
    TestFalse(TEXT("nothing is queued yet"), Rot->HasQueuedCast());

    // --- Press on +Y inside the wind-up: refused by GAS, taken by the queue --
    Clock(Rig.WindUp * 0.25f);
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 90.0f);
    TestFalse(TEXT("the second press activates nothing — the instance is still casting"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo));
    TestTrue(TEXT("but the queue accepted it"), Rot->HasQueuedCast());
    TestEqual(TEXT("and no puddle exists yet"), BreakerRotAimZonesOf(World, Rig.Caster).Num(), 0);

    // --- A third press on -X is dropped: ONE queued cast ----------------------
    Clock(Rig.WindUp * 0.25f);
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 180.0f);
    TestFalse(TEXT("the third press activates nothing either"), Rig.Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityTwo));
    TestTrue(TEXT("the queue still holds exactly the one press"), Rot->HasQueuedCast());

    // --- The first landing: +X, and the queued cast starts its own wind-up ---
    // Ticked by a fixed clock rather than BreakerResolvePendingCast, which
    // would run straight through the second wind-up as well.
    const float BankBeforeLanding = Rig.Mana->GetMana();
    Clock(Rig.WindUp * 0.5f + 0.1f);
    const TArray<ABreakerZoneActor*> FirstLanding = BreakerRotAimZonesOf(World, Rig.Caster);
    if (!TestEqual(TEXT("one puddle after the first landing"), FirstLanding.Num(), 1)) return false;
    const FVector First = FirstLanding[0]->GetActorLocation();
    TestTrue(TEXT("it went out along +X, where the first press pointed"), First.X > Feet.X + 1000.0f);
    TestTrue(TEXT("and not toward +Y or -X"), FMath::Abs(First.Y - Feet.Y) < 5.0f);
    TestTrue(TEXT("a second wind-up is already pending"), BreakerAnyCastPending(Rig.Caster));
    TestFalse(TEXT("and the queue is spent"), Rot->HasQueuedCast());
    // The queued cast is a paid cast, not a free one: the bank dropped by
    // the price across the landing (the small regen inside 0.1 s cannot
    // hide a whole Rot).
    TestTrue(TEXT("the queued cast paid its price"), BankBeforeLanding - Rig.Mana->GetMana() > Cost * 0.5f);

    // --- The second landing: +Y, where the QUEUED press pointed ---------------
    // The reticle sits on -X the whole way through, so a puddle that solved
    // at the second landing would go there instead.
    BreakerRotAimPoint(*Rig.Controller, *Rig.Caster, 180.0f);
    BreakerResolvePendingCast(World, Rig.Caster, Rig.WindUp + 0.5f);
    TestFalse(TEXT("the second wind-up has landed"), BreakerAnyCastPending(Rig.Caster));
    const TArray<ABreakerZoneActor*> SecondLanding = BreakerRotAimZonesOf(World, Rig.Caster);
    if (!TestEqual(TEXT("two puddles after the second landing"), SecondLanding.Num(), 2)) return false;
    const ABreakerZoneActor* Second = SecondLanding[0] == FirstLanding[0] ? SecondLanding[1] : SecondLanding[0];
    TestTrue(TEXT("the second went out along +Y, where the queued press pointed"), Second->GetActorLocation().Y > Feet.Y + 1000.0f);
    TestTrue(TEXT("and stayed on the caster's X line"), FMath::Abs(Second->GetActorLocation().X - Feet.X) < 50.0f);
    for (const ABreakerZoneActor* Zone : SecondLanding)
        TestTrue(TEXT("no puddle on -X: the third press was dropped"), Zone->GetActorLocation().X > Feet.X - 1000.0f);

    // --- And nothing else follows: the dropped press never became a cast -----
    Clock(Rig.WindUp + 0.2f);
    TestFalse(TEXT("no third wind-up"), BreakerAnyCastPending(Rig.Caster));
    TestEqual(TEXT("still two puddles"), BreakerRotAimZonesOf(World, Rig.Caster).Num(), 2);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
