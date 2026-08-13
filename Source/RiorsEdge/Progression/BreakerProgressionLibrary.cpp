#include "Progression/BreakerProgressionLibrary.h"

#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#define LOCTEXT_NAMESPACE "BreakerProgressionContent"

namespace BreakerNodeTags
{
    UE_DEFINE_GAMEPLAY_TAG(Node_Fixate, "Progression.Node.Core.Fixate");
    UE_DEFINE_GAMEPLAY_TAG(Node_TunnelVision, "Progression.Node.Core.TunnelVision");
    UE_DEFINE_GAMEPLAY_TAG(Node_TriggerDiscipline, "Progression.Node.Core.TriggerDiscipline");
    UE_DEFINE_GAMEPLAY_TAG(Node_Cyclic, "Progression.Node.Core.Cyclic");
    UE_DEFINE_GAMEPLAY_TAG(Node_LastRound, "Progression.Node.Core.LastRound");
    UE_DEFINE_GAMEPLAY_TAG(Node_OpenWound, "Progression.Node.Core.OpenWound");
    UE_DEFINE_GAMEPLAY_TAG(Node_SetStance, "Progression.Node.Core.SetStance");
    UE_DEFINE_GAMEPLAY_TAG(Node_Read, "Progression.Node.Core.Read");
    UE_DEFINE_GAMEPLAY_TAG(Node_Loft, "Progression.Node.Core.Loft");
    UE_DEFINE_GAMEPLAY_TAG(Node_PhantomStep, "Progression.Node.Core.PhantomStep");
    UE_DEFINE_GAMEPLAY_TAG(Verb_Parry, "Progression.Verb.Parry");
    UE_DEFINE_GAMEPLAY_TAG(Verb_AirJump, "Progression.Verb.AirJump");

    UE_DEFINE_GAMEPLAY_TAG(Node_ReadTheRoom, "Progression.Node.Swift.Kinetic.ReadTheRoom");
    UE_DEFINE_GAMEPLAY_TAG(Node_Contact, "Progression.Node.Swift.Kinetic.Contact");
    UE_DEFINE_GAMEPLAY_TAG(Node_Carry, "Progression.Node.Swift.Kinetic.Carry");
    UE_DEFINE_GAMEPLAY_TAG(Node_Redirect, "Progression.Node.Swift.Kinetic.Redirect");
    UE_DEFINE_GAMEPLAY_TAG(Node_EvadeConversion, "Progression.Node.Swift.Kinetic.EvadeConversion");
    UE_DEFINE_GAMEPLAY_TAG(Node_Landing, "Progression.Node.Swift.Kinetic.Landing");
    UE_DEFINE_GAMEPLAY_TAG(Node_SkimDiscipline, "Progression.Node.Swift.Kinetic.SkimDiscipline");
    UE_DEFINE_GAMEPLAY_TAG(Node_AirWork, "Progression.Node.Swift.Kinetic.AirWork");

    UE_DEFINE_GAMEPLAY_TAG(Node_LongLens, "Progression.Node.Swift.Marksman.LongLens");
    UE_DEFINE_GAMEPLAY_TAG(Node_Steady, "Progression.Node.Swift.Marksman.Steady");
    UE_DEFINE_GAMEPLAY_TAG(Node_Ledger, "Progression.Node.Swift.Marksman.Ledger");
    UE_DEFINE_GAMEPLAY_TAG(Node_Angle, "Progression.Node.Swift.Marksman.Angle");
    UE_DEFINE_GAMEPLAY_TAG(Node_MarkEconomy, "Progression.Node.Swift.Marksman.MarkEconomy");
    UE_DEFINE_GAMEPLAY_TAG(Node_PierceDiscipline, "Progression.Node.Swift.Marksman.PierceDiscipline");
    UE_DEFINE_GAMEPLAY_TAG(Node_Sightline, "Progression.Node.Swift.Marksman.Sightline");
    UE_DEFINE_GAMEPLAY_TAG(Node_Lead, "Progression.Node.Swift.Marksman.Lead");
}

namespace
{
    // Investment gate per tier. O2 PLACEHOLDER — the shipping gates come from
    // the constellation Data Asset and are expected to change.
    int32 GateForTier(int32 Tier) { return FMath::Max(0, (Tier - 1) * 2); }

    UBreakerProgressionNode* MakeNode(
        FName NodeId,
        const TCHAR* DisplayName,
        const TCHAR* Description,
        EBreakerPointCurrency Currency,
        EBreakerClassId RequiredClass,
        int32 Tier,
        int32 MaxRank,
        int32 CostPerRank)
    {
        UBreakerProgressionNode* Node = NewObject<UBreakerProgressionNode>(GetTransientPackage(), UBreakerProgressionNode::StaticClass(), NAME_None, RF_Standalone);
        Node->AddToRoot();
        Node->NodeId = NodeId;
        Node->DisplayName = FText::FromString(DisplayName);
        Node->Description = FText::FromString(Description);
        Node->Currency = Currency;
        Node->RequiredClass = RequiredClass;
        Node->Tier = Tier;
        Node->MaxRank = MaxRank;
        Node->CostPerRank = CostPerRank;
        Node->RequiredTreeInvestment = GateForTier(Tier);
        return Node;
    }

    void AddPrerequisite(UBreakerProgressionNode* Node, FName RequiredNodeId, int32 RequiredRank = 1)
    {
        FBreakerNodePrerequisite Prerequisite;
        Prerequisite.NodeId = RequiredNodeId;
        Prerequisite.RequiredRank = RequiredRank;
        Node->Prerequisites.Add(Prerequisite);
    }

    void AddEffect(UBreakerProgressionNode* Node, EBreakerNodeStatTarget Target, EBreakerNodeStatBucket Bucket, float ValuePerRank)
    {
        FBreakerNodeEffect Effect;
        Effect.StatTarget = Target;
        Effect.StatBucket = Bucket;
        Effect.ValuePerRank = ValuePerRank;
        Node->Effects.Add(Effect);
    }

    UBreakerProgressionTree* MakeTree(FName TreeId, const TCHAR* DisplayName, EBreakerPointCurrency Currency, EBreakerClassId RequiredClass)
    {
        UBreakerProgressionTree* Tree = NewObject<UBreakerProgressionTree>(GetTransientPackage(), UBreakerProgressionTree::StaticClass(), NAME_None, RF_Standalone);
        Tree->AddToRoot();
        Tree->TreeId = TreeId;
        Tree->DisplayName = FText::FromString(DisplayName);
        Tree->Currency = Currency;
        Tree->RequiredClass = RequiredClass;
        return Tree;
    }
}

// Node stat magnitudes below are gym-perceptibility tuning; wave-mode re-anchors.
UBreakerProgressionTree* UBreakerProgressionLibrary::GetCoreSliceTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Core.Slice"), TEXT("Core Constellations (Slice)"), EBreakerPointCurrency::CorePoints, EBreakerClassId::None);

    // --- Precision ---------------------------------------------------------
    UBreakerProgressionNode* Sightline = MakeNode(TEXT("Core.Precision.Sightline"), TEXT("Sightline"),
        TEXT("Precision gateway. Weak-point damage is easier to earn."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 1, 1);
    AddEffect(Sightline, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 7.0f); // O2 PLACEHOLDER
    Tree->Nodes.Add(Sightline);

    UBreakerProgressionNode* TunnelVision = MakeNode(TEXT("Core.Precision.TunnelVision"), TEXT("Tunnel Vision"),
        TEXT("Notable. Critical damage rises while a single target holds your attention."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 1, 2);
    AddPrerequisite(TunnelVision, TEXT("Core.Precision.Sightline"));
    AddEffect(TunnelVision, EBreakerNodeStatTarget::CriticalDamage, EBreakerNodeStatBucket::Flat, 22.0f); // O2 PLACEHOLDER
    TunnelVision->GrantedTags.AddTag(BreakerNodeTags::Node_TunnelVision.GetTag());
    Tree->Nodes.Add(TunnelVision);

    // Fixate is the slice's only More multiplier and is a Convergence node
    // after O21. The More bucket does not exist in code yet, so it ships as a
    // tag grant and the damage pipeline consumes it when the bucket lands.
    UBreakerProgressionNode* Fixate = MakeNode(TEXT("Core.Precision.Fixate"), TEXT("Fixate"),
        TEXT("Convergence. Repeated hits on one target build a More multiplier."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 3);
    AddPrerequisite(Fixate, TEXT("Core.Precision.TunnelVision"));
    Fixate->GrantedTags.AddTag(BreakerNodeTags::Node_Fixate.GetTag());
    Tree->Nodes.Add(Fixate);

    // --- Volley ------------------------------------------------------------
    UBreakerProgressionNode* TriggerDiscipline = MakeNode(TEXT("Core.Volley.TriggerDiscipline"), TEXT("Trigger Discipline"),
        TEXT("Volley gateway. Recoil settles faster between controlled bursts."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 1, 1);
    TriggerDiscipline->GrantedTags.AddTag(BreakerNodeTags::Node_TriggerDiscipline.GetTag());
    Tree->Nodes.Add(TriggerDiscipline);

    UBreakerProgressionNode* Cyclic = MakeNode(TEXT("Core.Volley.Cyclic"), TEXT("Cyclic"),
        TEXT("Sustained fire ramps rate of fire, then decays when you stop."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 3, 1);
    AddPrerequisite(Cyclic, TEXT("Core.Volley.TriggerDiscipline"));
    Cyclic->GrantedTags.AddTag(BreakerNodeTags::Node_Cyclic.GetTag());
    Tree->Nodes.Add(Cyclic);

    UBreakerProgressionNode* LastRound = MakeNode(TEXT("Core.Volley.LastRound"), TEXT("Last Round"),
        TEXT("The final round of a magazine fires extra projectiles. They apply no statuses (proc coefficient 0)."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 1, 2);
    AddPrerequisite(LastRound, TEXT("Core.Volley.TriggerDiscipline"));
    LastRound->GrantedTags.AddTag(BreakerNodeTags::Node_LastRound.GetTag());
    Tree->Nodes.Add(LastRound);

    // --- Affliction --------------------------------------------------------
    UBreakerProgressionNode* OpenWound = MakeNode(TEXT("Core.Affliction.OpenWound"), TEXT("Open Wound"),
        TEXT("Affliction gateway. Weak-point hits apply Bleed regardless of chance."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 1, 1);
    OpenWound->GrantedTags.AddTag(BreakerNodeTags::Node_OpenWound.GetTag());
    Tree->Nodes.Add(OpenWound);

    UBreakerProgressionNode* Deepen = MakeNode(TEXT("Core.Affliction.Deepen"), TEXT("Deepen"),
        TEXT("Damage over time hits harder and stacks deeper."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 3, 1);
    AddPrerequisite(Deepen, TEXT("Core.Affliction.OpenWound"));
    AddEffect(Deepen, EBreakerNodeStatTarget::DamageOverTime, EBreakerNodeStatBucket::IncreasedPercent, 18.0f); // O2 PLACEHOLDER
    Tree->Nodes.Add(Deepen);

    // --- Bulwark -----------------------------------------------------------
    UBreakerProgressionNode* SetStance = MakeNode(TEXT("Core.Bulwark.SetStance"), TEXT("Set Stance"),
        TEXT("Bulwark gateway. Block rolls more often and you carry more health."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 1, 1);
    AddEffect(SetStance, EBreakerNodeStatTarget::BlockChance, EBreakerNodeStatBucket::Flat, 6.0f);  // O2 PLACEHOLDER
    AddEffect(SetStance, EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 90.0f);      // O2 PLACEHOLDER
    SetStance->GrantedTags.AddTag(BreakerNodeTags::Node_SetStance.GetTag());
    Tree->Nodes.Add(SetStance);

    // Inert until Parry is owned. Buying it to rank 3 with no Parry must be a
    // no-op and must not error (§10.3 criterion 5); it therefore authors no
    // stat effect at all, only a tag Parry reads.
    UBreakerProgressionNode* Read = MakeNode(TEXT("Core.Bulwark.Read"), TEXT("Read"),
        TEXT("Parry's window widens. Inert until Parry is owned."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 3, 1);
    AddPrerequisite(Read, TEXT("Core.Bulwark.SetStance"));
    Read->GrantedTags.AddTag(BreakerNodeTags::Node_Read.GetTag());
    Tree->Nodes.Add(Read);

    UBreakerProgressionNode* Parry = MakeNode(TEXT("Core.Bulwark.Parry"), TEXT("Parry"),
        TEXT("VERB GRANT. Parry becomes available on its own short cooldown."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 2);
    AddPrerequisite(Parry, TEXT("Core.Bulwark.SetStance"));
    Parry->GrantedAbilityIds.Add(TEXT("Parry"));
    Parry->GrantedTags.AddTag(BreakerNodeTags::Verb_Parry.GetTag());
    Tree->Nodes.Add(Parry);

    // --- Kinesis -----------------------------------------------------------
    UBreakerProgressionNode* LightFooting = MakeNode(TEXT("Core.Kinesis.LightFooting"), TEXT("Light Footing"),
        TEXT("Kinesis gateway. Dodge rolls more often and you move a little quicker."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 1, 1);
    AddEffect(LightFooting, EBreakerNodeStatTarget::DodgeChance, EBreakerNodeStatBucket::Flat, 5.0f);             // O2 PLACEHOLDER
    AddEffect(LightFooting, EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 12.0f);  // O2 PLACEHOLDER
    Tree->Nodes.Add(LightFooting);

    // Inert until Air Jump is owned — the second inert-node test.
    UBreakerProgressionNode* Loft = MakeNode(TEXT("Core.Kinesis.Loft"), TEXT("Loft"),
        TEXT("Air Jump gains height and control. Inert until Air Jump is owned."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 3, 1);
    AddPrerequisite(Loft, TEXT("Core.Kinesis.LightFooting"));
    Loft->GrantedTags.AddTag(BreakerNodeTags::Node_Loft.GetTag());
    Tree->Nodes.Add(Loft);

    UBreakerProgressionNode* PhantomStep = MakeNode(TEXT("Core.Kinesis.PhantomStep"), TEXT("Phantom Step"),
        TEXT("A successful Dodge grants brief invulnerability on a 2.0s internal cooldown."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 2);
    AddPrerequisite(PhantomStep, TEXT("Core.Kinesis.LightFooting"));
    PhantomStep->GrantedTags.AddTag(BreakerNodeTags::Node_PhantomStep.GetTag());
    Tree->Nodes.Add(PhantomStep);

    UBreakerProgressionNode* AirJump = MakeNode(TEXT("Core.Kinesis.AirJump"), TEXT("Air Jump"),
        TEXT("VERB GRANT. One mid-air jump, refreshed on landing, wall contact, or a successful Dodge."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 2);
    AddPrerequisite(AirJump, TEXT("Core.Kinesis.LightFooting"));
    AddEffect(AirJump, EBreakerNodeStatTarget::AirControl, EBreakerNodeStatBucket::IncreasedPercent, 15.0f); // O2 PLACEHOLDER
    AirJump->GrantedAbilityIds.Add(TEXT("AirJump"));
    AirJump->GrantedTags.AddTag(BreakerNodeTags::Verb_AirJump.GetTag());
    Tree->Nodes.Add(AirJump);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetSwiftKineticTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Swift.Kinetic"), TEXT("Swift — Kinetic"), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift);

    UBreakerProgressionNode* Node = MakeNode(TEXT("Swift.Kinetic.ReadTheRoom"), TEXT("Read the Room"),
        TEXT("Airborne Momentum generation credit lasts longer."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_ReadTheRoom.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.Contact"), TEXT("Contact"),
        TEXT("Wall ride Momentum generation continues briefly after losing contact."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Contact.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.Carry"), TEXT("Carry"),
        TEXT("Slide chaining pays flat Momentum and carries more speed."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 1, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::SlideSpeed, EBreakerNodeStatBucket::IncreasedPercent, 12.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Carry.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.Redirect"), TEXT("Redirect"),
        TEXT("Sharp airborne direction changes reduce Skim's cooldown once per airtime."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.ReadTheRoom"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Redirect.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.EvadeConversion"), TEXT("Evade Conversion"),
        TEXT("The passive dodge proc yields more Momentum on a shorter internal cooldown."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::DodgeChance, EBreakerNodeStatBucket::Flat, 4.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_EvadeConversion.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.Landing"), TEXT("Landing"),
        TEXT("Long falls convert into Momentum on landing."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.ReadTheRoom"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Landing.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.SkimDiscipline"), TEXT("Skim Discipline"),
        TEXT("Grants Hard Stop. Skim may be used twice per airtime."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.Redirect"));
    Node->GrantedAbilityIds.Add(TEXT("HardStop"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SkimDiscipline.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.AirWork"), TEXT("Air Work"),
        TEXT("Airborne handling improves sharply while at Redline."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.Landing"));
    AddEffect(Node, EBreakerNodeStatTarget::AirControl, EBreakerNodeStatBucket::IncreasedPercent, 12.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AirWork.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetSwiftMarksmanTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Swift.Marksman"), TEXT("Swift — Marksman"), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift);

    UBreakerProgressionNode* Node = MakeNode(TEXT("Swift.Marksman.LongLens"), TEXT("Long Lens"),
        TEXT("Distant weak-point hits generate Momentum and land harder."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 1, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::CriticalDamage, EBreakerNodeStatBucket::Flat, 18.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_LongLens.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Marksman.Steady"), TEXT("Steady"),
        TEXT("Aiming while moving no longer widens spread."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Steady.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Marksman.Ledger"), TEXT("Ledger"),
        TEXT("Momentum spent on Marksman abilities is partly refunded when they connect."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Ledger.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Marksman.Angle"), TEXT("Angle"),
        TEXT("Ricochets seek the nearest target instead of reflecting geometrically."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.LongLens"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Angle.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Marksman.MarkEconomy"), TEXT("Mark Economy"),
        TEXT("Lead's mark survives its target's death and jumps to a nearby enemy."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.Ledger"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MarkEconomy.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Marksman.PierceDiscipline"), TEXT("Pierce Discipline"),
        TEXT("Each target pierced by a shot generates Momentum, up to three."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.Steady"));
    AddEffect(Node, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 6.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_PierceDiscipline.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Marksman.Sightline"), TEXT("Sightline"),
        TEXT("Grants Sightline. Its pierce ignores Armour after the first target."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Marksman.PierceDiscipline"));
    Node->GrantedAbilityIds.Add(TEXT("Sightline"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Sightline.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Marksman.Lead"), TEXT("Lead"),
        TEXT("Grants Lead. Lead may be held on two targets at once."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Marksman.MarkEconomy"));
    Node->GrantedAbilityIds.Add(TEXT("Lead"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Lead.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

const TArray<UBreakerProgressionTree*>& UBreakerProgressionLibrary::GetAllFallbackTrees()
{
    static TArray<UBreakerProgressionTree*> Trees;
    if (Trees.Num() == 0)
    {
        Trees.Add(GetCoreSliceTree());
        Trees.Add(GetSwiftKineticTree());
        Trees.Add(GetSwiftMarksmanTree());
    }
    return Trees;
}

TArray<UBreakerProgressionTree*> UBreakerProgressionLibrary::GetTreesForClass(EBreakerClassId ClassId)
{
    TArray<UBreakerProgressionTree*> Result;
    for (UBreakerProgressionTree* Tree : GetAllFallbackTrees())
    {
        if (Tree && (Tree->RequiredClass == EBreakerClassId::None || Tree->RequiredClass == ClassId))
        {
            Result.Add(Tree);
        }
    }
    return Result;
}

UBreakerClassDefinition* UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId ClassId)
{
    // Only Swift is authored in full for the slice (Class-Kits §1.7).
    if (ClassId != EBreakerClassId::Swift) return nullptr;

    static UBreakerClassDefinition* Swift = nullptr;
    if (Swift) return Swift;

    Swift = NewObject<UBreakerClassDefinition>(GetTransientPackage(), UBreakerClassDefinition::StaticClass(), NAME_None, RF_Standalone);
    Swift->AddToRoot();
    Swift->ClassAssetId = TEXT("Swift");
    Swift->ClassId = EBreakerClassId::Swift;
    Swift->DisplayName = LOCTEXT("SwiftName", "Swift");
    Swift->Description = LOCTEXT("SwiftDescription", "Momentum: a decaying state built by moving and spent on short-cooldown bursts.");
    Swift->StartingClassAbilityIds = {TEXT("Skim"), TEXT("Sever")};
    Swift->BaseUltimateId = TEXT("Overdrive");
    Swift->BranchTrees.Add(GetSwiftKineticTree());
    Swift->BranchTrees.Add(GetSwiftMarksmanTree());
    Swift->BranchTrees.Add(GetCoreSliceTree());
    return Swift;
}

const UBreakerProgressionNode* UBreakerProgressionLibrary::FindFallbackNode(FName NodeId)
{
    for (const UBreakerProgressionTree* Tree : GetAllFallbackTrees())
    {
        if (!Tree) continue;
        if (const UBreakerProgressionNode* Node = Tree->FindNode(NodeId)) return Node;
    }
    return nullptr;
}

#undef LOCTEXT_NAMESPACE
