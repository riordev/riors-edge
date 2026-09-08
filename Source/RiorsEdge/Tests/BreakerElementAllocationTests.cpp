#include "Misc/AutomationTest.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerElementAllocationTest, "RiorsEdge.Combat.ElementScopeAllocation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerElementAllocationTest::RunTest(const FString& Parameters)
{
    auto* Unbound = NewObject<UBreakerAttributeSet>();
    FBreakerDamageRequest NativeDefault; NativeDefault.BaseDamage = 100;
    NativeDefault.ElementShares = {{EBreakerElement::Entropy, 1}}; NativeDefault.bCanCritical = false;
    UBreakerDamageLibrary::FillSourcePools(Unbound, EBreakerDamageDelivery::Ability, NativeDefault);
    FBreakerDefenseState DefaultDefense; DefaultDefense.Health = 10000;
    TestEqual(TEXT("Unbound native attributes preserve their valid source default"),
        UBreakerDamageLibrary::ResolveDamage(NativeDefault, DefaultDefense).RawDamage, 100.f, .001f);
    FBreakerDamageRequest Hit;
    Hit.BaseDamage = 100; Hit.SourceDamageMultiplier = 2;
    Hit.SourceIncreasedPercent = 100; Hit.bHasSourceSplit = true;
    Hit.CriticalChance = 1; Hit.CriticalMultiplier = 2;
    Hit.ElementShares = {{EBreakerElement::Entropy, .25f}, {EBreakerElement::Void, .5f}};
    Hit.ElementSource.ElementalDamageIncreasedPercent = 50;
    Hit.ElementSource.ElementalMoreProduct = 1.22f; Hit.ElementSource.VoidMoreProduct = 1.18f;
    FBreakerDefenseState Defense; Defense.Health = 10000;
    Defense.PhysicalDamageReductionPercent = 60; Defense.SharedDamageReductionPercent = 10;
    const auto Result = UBreakerDamageLibrary::ResolveDamage(Hit, Defense);
    TestTrue(TEXT("One critical applies to all portions"), Result.bCritical);
    TestEqual(TEXT("Physical portion receives no elemental bonus"), Result.UnconvertedRawDamage, 100.f, .001f);
    if (TestEqual(TEXT("Both element portions remain explicit"), Result.ElementRawDamage.Num(), 2))
    {
        TestEqual(TEXT("Entropy joins additive bucket"), Result.ElementRawDamage[0].RawDamage, 152.5f, .001f);
        TestEqual(TEXT("Void receives matching scopes once"), Result.ElementRawDamage[1].RawDamage, 359.9f, .001f);
    }
    TestEqual(TEXT("One combined damage payment"), Result.RawDamage, 612.4f, .001f);
    TestEqual(TEXT("Physical DR weights actual physical raw amount, shared DR remains additive"), Result.HealthDamage, 491.16f, .002f);
    Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
    TestEqual(TEXT("True damage ignores both reductions"), UBreakerDamageLibrary::ResolveDamage(Hit, Defense).HealthDamage, 612.4f, .001f);
    Hit.ElementShares = {{EBreakerElement::Void, 1}}; Hit.CriticalChance = 0;
    Hit.SourceMoreProduct = FBreakerAttributeAggregator::ComposedMoreCeiling();
    Hit.SourceIncreasedPercent = 0; Hit.ElementSource.ElementalDamageIncreasedPercent = 0;
    Hit.SourceDamageMultiplier = Hit.SourceMoreProduct;
    TestEqual(TEXT("Scoped allocation cannot exceed ceiling already spent by a window"),
        UBreakerDamageLibrary::ResolveDamage(Hit, Defense).RawDamage, 100 * Hit.SourceMoreProduct, .002f);
    Hit.SourceMoreProduct = 1; Hit.SourceIncreasedPercent = -100; Hit.SourceDamageMultiplier = 0;
    Hit.ElementSource.ElementalDamageIncreasedPercent = 50;
    Hit.ElementSource.ElementalMoreProduct = Hit.ElementSource.VoidMoreProduct = 1;
    TestEqual(TEXT("Elemental Increased joins a zero delivery bucket rather than multiplying zero"),
        UBreakerDamageLibrary::ResolveDamage(Hit, Defense).RawDamage, 50.f, .001f);
    return true;
}
#endif
