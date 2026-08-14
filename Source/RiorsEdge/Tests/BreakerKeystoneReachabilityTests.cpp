#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameplayTagContainer.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// KEYSTONE REACHABILITY — the O40(c) instrument for the ability-rewrite layer
// ---------------------------------------------------------------------------
// O40(c) makes reachability part of definition-of-done: a feature merges
// together with its in-game path and a shipped-configuration test in the
// `RiorsEdge.Movement.JumpGrantMatrix` mold. This file is that test for
// keystone ability rewrites, and it exists because the layer had already
// failed exactly the way the third jump did.
//
// Overdrive shipped with three keystone variant rows. Exactly ONE of the three
// tags, `Keystone.Swift.Bloodrhythm`, was granted by any node, so the Kinetic
// and Marksman rewrites were unreachable BY CONSTRUCTION — resolvable code
// behind a tag no character in the game could ever hold. Nothing caught it:
// `RiorsEdge.Abilities.*` proved every row resolves when the tag is present,
// which was true and beside the point, and the suite was green for the whole
// time the content was dead. Class-Kits §6.1.1 recorded it as prose, which is
// not an instrument.
//
// The two tests below close both directions, and both are written against the
// SHIPPED registries rather than fixtures, so they fail on the day content
// drifts rather than on the day someone re-reads the doc.
// ---------------------------------------------------------------------------

namespace
{
    // Every distinct keystone tag any shipped ability variant row keys off,
    // paired with the class that owns the ability carrying it.
    TMap<FGameplayTag, EBreakerClassId> CollectKeystoneTagsFromAbilities()
    {
        TMap<FGameplayTag, EBreakerClassId> Tags;
        for (const UBreakerAbilityDefinition* Ability : UBreakerAbilityDefinition::GetFallbackRegistry())
        {
            if (!Ability) continue;
            for (const FBreakerAbilityVariant& Variant : Ability->Variants)
            {
                if (Variant.KeystoneTag.IsValid())
                {
                    Tags.Add(Variant.KeystoneTag, Ability->ClassId);
                }
            }
        }
        return Tags;
    }

    // Every tag any node in any shipped fallback tree grants, mapped back to
    // the node that grants it so a failure can name the culprit.
    TMap<FGameplayTag, const UBreakerProgressionNode*> CollectGrantedTagsFromTrees()
    {
        TMap<FGameplayTag, const UBreakerProgressionNode*> Granted;
        for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
        {
            if (!Tree) continue;
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (!Node) continue;
                for (const FGameplayTag& Tag : Node->GrantedTags)
                {
                    Granted.Add(Tag, Node);
                }
            }
        }
        return Granted;
    }

    // A class has an authored branch layer when at least one fallback tree is
    // REQUIRED by it. The shared Core tree carries RequiredClass == None and
    // must not count, or every class would look authored.
    bool ClassHasAuthoredBranchTrees(EBreakerClassId ClassId)
    {
        for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
        {
            if (Tree && Tree->RequiredClass == ClassId)
            {
                return true;
            }
        }
        return false;
    }
}

// ---------------------------------------------------------------------------
// Forward direction: every rewrite a player can resolve, a player can reach.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeystoneRewritesAreReachableTest,
    "RiorsEdge.Abilities.KeystoneReachability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeystoneRewritesAreReachableTest::RunTest(const FString& Parameters)
{
    const TMap<FGameplayTag, EBreakerClassId> KeystoneTags = CollectKeystoneTagsFromAbilities();
    const TMap<FGameplayTag, const UBreakerProgressionNode*> Granted = CollectGrantedTagsFromTrees();

    TestTrue(TEXT("The shipped ability registry authors at least one keystone variant row"),
        KeystoneTags.Num() > 0);

    for (const TPair<FGameplayTag, EBreakerClassId>& Entry : KeystoneTags)
    {
        const FString TagName = Entry.Key.ToString();
        const UBreakerProgressionNode* const* GrantingNode = Granted.Find(Entry.Key);

        if (!ClassHasAuthoredBranchTrees(Entry.Value))
        {
            // HONEST EMPTINESS, not an exemption anyone chose. Caster's three
            // rewrite rows exist while no Caster branch tree does (O39 ships
            // the class through abilities + Core only), so there is no node
            // anywhere that COULD carry the tag. This arm is deliberately
            // narrow and self-closing: the day a Caster branch tree is
            // authored, ClassHasAuthoredBranchTrees flips, this arm stops
            // applying, and the test goes red until the keystones are sited on
            // real nodes. That is the whole point — the gap cannot be
            // forgotten, because closing half of it turns the suite red.
            TestNull(*FString::Printf(
                TEXT("%s belongs to a class with no branch trees, so no node should be granting it yet"), *TagName),
                GrantingNode ? *GrantingNode : nullptr);
            continue;
        }

        if (!GrantingNode)
        {
            AddError(FString::Printf(
                TEXT("UNREACHABLE KEYSTONE: %s is a variant row on a shipped ability whose class HAS branch trees, ")
                TEXT("but no node in any fallback tree grants it. The rewrite resolves in code and no character can ever ")
                TEXT("hold the tag. Site it on that branch's keystone node, or delete the variant row."), *TagName));
            continue;
        }

        // O37: the keystone tier is what branch commitment unlocks, so a node
        // carrying a keystone tag must be the flagged cornerstone. A keystone
        // rewrite hanging off an ordinary node would be reachable but would
        // bypass the commitment gate entirely, which is a quieter version of
        // the same defect.
        TestTrue(*FString::Printf(TEXT("%s is granted by a cornerstone node (O37 commitment gate)"), *TagName),
            (*GrantingNode)->bCornerstone);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Reverse direction: no node promises a rewrite that does not exist.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeystoneGrantsAreReadTest,
    "RiorsEdge.Abilities.KeystoneGrantsAreRead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeystoneGrantsAreReadTest::RunTest(const FString& Parameters)
{
    // The twin failure, and the more dangerous one to a player: a keystone node
    // that reads as a build-defining pick on the board and grants a tag nothing
    // consumes. It costs three points and silently does nothing. The forward
    // test cannot see it, because it walks the ability side.
    const TMap<FGameplayTag, EBreakerClassId> KeystoneTags = CollectKeystoneTagsFromAbilities();

    // Matched on the tag STRING rather than against a requested `Keystone`
    // root, because nothing declares that bare root — every declaration is a
    // full three-segment leaf. RequestGameplayTag on the parent would depend on
    // implicit parent registration, which is a detail of the tag manager rather
    // than of this project's content.
    for (const TPair<FGameplayTag, const UBreakerProgressionNode*>& Entry : CollectGrantedTagsFromTrees())
    {
        if (!Entry.Key.ToString().StartsWith(TEXT("Keystone.")))
        {
            continue;  // ordinary Node.* identity tags are read elsewhere
        }
        if (!KeystoneTags.Contains(Entry.Key))
        {
            AddError(FString::Printf(
                TEXT("KEYSTONE GRANTS NOTHING: node '%s' grants %s and no shipped ability variant row keys off it. ")
                TEXT("The node presents as a keystone and is inert."),
                *Entry.Value->NodeId.ToString(), *Entry.Key.ToString()));
        }
    }

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
