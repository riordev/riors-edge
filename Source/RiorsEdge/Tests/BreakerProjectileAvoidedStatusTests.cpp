#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerProjectileAvoidedStatusTest, "RiorsEdge.Combat.ProjectileAvoidedStatus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerProjectileAvoidedStatusTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated impact world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Source = World->SpawnActor<ABreakerCharacter>();
    auto* Target = World->SpawnActor<ABreakerCharacter>(FVector(500, 0, 0), FRotator::ZeroRotator);
    if (!Source || !Target) return false;
    // Only native consumers are initialized; Character BeginPlay would load saves.
    for (auto* Player : {Source, Target})
    {
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetCombat()->BeginPlay();
    }
    if (!Source->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    auto* Mana = Source->GetMana();
    Mana->BindAttributes(Source->GetAttributes());
    Mana->PassiveRegenPerSecond = 0; // Isolate earned status income.
    if (!Mana->TrySpendMana(50)) return false;
    auto* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Source);
    auto* Status = Target->FindComponentByClass<UBreakerStatusComponent>();
    if (!Cycle || !Status) return false;
    Status->BeginPlay();
    const auto Entry = Cycle->PeekNextEntry(0);
    FBreakerCarriedStatus Carried; Carried.Spec = Entry.Spec; Carried.DamageFamily = Entry.DamageFamily;
    if (!TestTrue(TEXT("uses shipped physical cycle payload"), Carried.Spec.StatusTag.IsValid() && Entry.Element == EBreakerElement::None)) return false;
    auto Impact = [&](float Damage)
    {
        FActorSpawnParameters Spawn; Spawn.Owner = Source; Spawn.Instigator = Source;
        auto* Projectile = World->SpawnActor<ABreakerProjectileBase>(ABreakerProjectileBase::StaticClass(), FVector(100, 0, 0), FRotator::ZeroRotator, Spawn);
        if (!Projectile) return false;
        FBreakerDamageRequest Request; Request.BaseDamage = Damage; Request.bCanCritical = false; Request.SetInstigator(Source);
        Projectile->InitializeProjectile(Request, FVector::ForwardVector, 4000);
        Projectile->AddImpactStatus(Carried); Projectile->SetCycleAdvanceOnHit(Cycle, 1);
        // Same authority seam as collision, at a frontal surface of the target.
        Projectile->Impact(Target, Target->GetActorLocation() + FVector(35, 0, 0));
        Mana->AdvanceLoop(1);
        return true;
    };
    const float InitialMana = Mana->GetMana();
    const int32 InitialCursor = Cycle->GetCursor();
    Target->GetCombat()->DodgeChance = 1;
    const float HealthBeforeDodge = Target->GetAttributes()->GetHealth();
    if (!Impact(10)) return false;
    TestEqual(TEXT("dodge negates impact damage"), Target->GetAttributes()->GetHealth(), HealthBeforeDodge);
    TestEqual(TEXT("dodge refuses carried physical status"), Status->GetDistinctStatusTypeCount(), 0);
    TestEqual(TEXT("avoided payload grants no status Mana"), Mana->GetMana(), InitialMana);
    TestEqual(TEXT("avoided impact cannot advance cycle"), Cycle->GetCursor(), InitialCursor);
    Target->GetCombat()->DodgeChance = 0;
    // Explicit XP entitlement fixture supplies the ordinary four Core points;
    // actual prerequisites and purchase gates still grant the parry verb.
    auto* Progression = Target->GetProgression();
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(4, Progression->ExperienceCurve));
    FText Failure;
    for (const TCHAR* Node : {TEXT("Core.Bulwark.Read"), TEXT("Core.Bulwark.Guard"), TEXT("Core.Bulwark.Parry")})
        if (!TestTrue(TEXT("purchases real parry prerequisite path"), Progression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(), Node, Failure))) return false;
    if (!TestTrue(TEXT("actual purchased parry starts"), Target->GetCombat()->TryParry())) return false;
    const float HealthBeforeParry = Target->GetAttributes()->GetHealth();
    if (!Impact(10)) return false;
    TestEqual(TEXT("parry negates impact damage"), Target->GetAttributes()->GetHealth(), HealthBeforeParry);
    TestFalse(TEXT("impact consumes the real parry window"), Target->GetCombat()->IsParryActive());
    TestEqual(TEXT("parry refuses carried status"), Status->GetDistinctStatusTypeCount(), 0);
    TestEqual(TEXT("parried payload grants no status Mana"), Mana->GetMana(), InitialMana);
    TestEqual(TEXT("parried impact cannot advance cycle"), Cycle->GetCursor(), InitialCursor);
    Target->GetCombat()->BlockChance = 0;
    if (!Impact(10)) return false;
    TestTrue(TEXT("accepted impact still applies shipped status"), Status->HasStatus(Carried.Spec.StatusTag));
    TestTrue(TEXT("accepted status earns normal Mana"), Mana->GetMana() > InitialMana);
    TestEqual(TEXT("accepted impact advances once"), Cycle->GetCursor(), (InitialCursor + 1) % Cycle->GetCycleLength());
    Status->ConsumeAllStatuses();
    Target->GetCombat()->DodgeChance = 1;
    if (!Impact(0)) return false;
    TestTrue(TEXT("status-only projectile preserves independent ailment application"), Status->HasStatus(Carried.Spec.StatusTag));
    return true;
}
#endif
