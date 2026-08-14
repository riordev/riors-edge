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

    // Velocity — the Core constellation O27 asked for: damage that keys off the
    // movement state, which is where this game's build identity belongs and
    // which did not exist in the slice at all.
    UE_DEFINE_GAMEPLAY_TAG(Node_Freefall, "Progression.Node.Core.Freefall");
    UE_DEFINE_GAMEPLAY_TAG(Node_Slipstream, "Progression.Node.Core.Slipstream");
    UE_DEFINE_GAMEPLAY_TAG(Node_Traction, "Progression.Node.Core.Traction");
    UE_DEFINE_GAMEPLAY_TAG(Node_Afterburn, "Progression.Node.Core.Afterburn");
    UE_DEFINE_GAMEPLAY_TAG(Node_TerminalVelocity, "Progression.Node.Core.TerminalVelocity");
    UE_DEFINE_GAMEPLAY_TAG(Node_RedlineDoctrine, "Progression.Node.Core.RedlineDoctrine");
    UE_DEFINE_GAMEPLAY_TAG(Node_CalledShot, "Progression.Node.Core.CalledShot");
    UE_DEFINE_GAMEPLAY_TAG(Node_Salvo, "Progression.Node.Core.Salvo");
    UE_DEFINE_GAMEPLAY_TAG(Node_Barrage, "Progression.Node.Core.Barrage");

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

    UE_DEFINE_GAMEPLAY_TAG(Node_Downforce, "Progression.Node.Swift.Kinetic.Downforce");
    UE_DEFINE_GAMEPLAY_TAG(Node_Grind, "Progression.Node.Swift.Kinetic.Grind");
    UE_DEFINE_GAMEPLAY_TAG(Node_Overpressure, "Progression.Node.Swift.Kinetic.Overpressure");
    UE_DEFINE_GAMEPLAY_TAG(Node_Deadeye, "Progression.Node.Swift.Marksman.Deadeye");
    UE_DEFINE_GAMEPLAY_TAG(Node_Culling, "Progression.Node.Swift.Marksman.Culling");
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

    void AddEffect(UBreakerProgressionNode* Node, EBreakerNodeStatTarget Target, EBreakerNodeStatBucket Bucket, float ValuePerRank,
        EBreakerBuildCondition Condition = EBreakerBuildCondition::Always)
    {
        FBreakerNodeEffect Effect;
        Effect.StatTarget = Target;
        Effect.StatBucket = Bucket;
        Effect.ValuePerRank = ValuePerRank;
        Effect.Condition = Condition;
        Node->Effects.Add(Effect);
    }

    // A More multiplier on outgoing damage, authored as whole percent above 1.0
    // (25.0 == x1.25). O3 restricts these to branch keystones and constellation
    // Convergence/Keystone nodes, so every caller below is a single-rank node
    // costing 3 or more — which is also how SBreakerMenu classifies a node as a
    // Convergence, so the board reads them correctly with no UI change.
    void AddDamageMore(UBreakerProgressionNode* Node, float PercentAboveOne, EBreakerBuildCondition Condition = EBreakerBuildCondition::Always)
    {
        AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::MorePercent, PercentAboveOne, Condition);
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
        TEXT("Precision gateway. Weak-point damage is easier to earn, and everything you fire lands a little harder."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 1, 1);
    AddEffect(Sightline, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 7.0f); // O2 PLACEHOLDER
    AddEffect(Sightline, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 4.0f); // O2 PLACEHOLDER
    Tree->Nodes.Add(Sightline);

    UBreakerProgressionNode* TunnelVision = MakeNode(TEXT("Core.Precision.TunnelVision"), TEXT("Tunnel Vision"),
        TEXT("Notable. Critical damage rises while a single target holds your attention."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 1, 2);
    AddPrerequisite(TunnelVision, TEXT("Core.Precision.Sightline"));
    AddEffect(TunnelVision, EBreakerNodeStatTarget::CriticalDamage, EBreakerNodeStatBucket::Flat, 22.0f); // O2 PLACEHOLDER
    TunnelVision->GrantedTags.AddTag(BreakerNodeTags::Node_TunnelVision.GetTag());
    Tree->Nodes.Add(TunnelVision);

    // Called Shot is the crit-chance choice the Precision line was missing:
    // without a second chance source, Critical Damage had nothing to turn on
    // and crit could not be the third axis of the variance band (Power-Curve
    // §4). Two ranks, so it reads as a Minor on the board.
    UBreakerProgressionNode* CalledShot = MakeNode(TEXT("Core.Precision.CalledShot"), TEXT("Called Shot"),
        TEXT("Deliberate fire finds the seam. Critical chance rises sharply, and every shot lands a little harder."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 2, 1);
    AddPrerequisite(CalledShot, TEXT("Core.Precision.Sightline"));
    AddEffect(CalledShot, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 4.0f);          // O2 PLACEHOLDER
    AddEffect(CalledShot, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 3.0f);      // O2 PLACEHOLDER
    CalledShot->GrantedTags.AddTag(BreakerNodeTags::Node_CalledShot.GetTag());
    Tree->Nodes.Add(CalledShot);

    // Fixate is a Convergence node after O21 and now carries a REAL More
    // multiplier: EBreakerNodeStatBucket::MorePercent exists, and the aggregator
    // composes it under the O3 cap. Unconditional, which is what makes it the
    // generalist pick against Terminal Velocity's larger conditional one.
    UBreakerProgressionNode* Fixate = MakeNode(TEXT("Core.Precision.Fixate"), TEXT("Fixate"),
        TEXT("Convergence. Repeated hits on one target build a MORE multiplier to all damage dealt."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 3);
    AddPrerequisite(Fixate, TEXT("Core.Precision.TunnelVision"));
    AddDamageMore(Fixate, 22.0f); // O2 PLACEHOLDER: x1.22
    Fixate->GrantedTags.AddTag(BreakerNodeTags::Node_Fixate.GetTag());
    Tree->Nodes.Add(Fixate);

    // --- Volley ------------------------------------------------------------
    UBreakerProgressionNode* TriggerDiscipline = MakeNode(TEXT("Core.Volley.TriggerDiscipline"), TEXT("Trigger Discipline"),
        TEXT("Volley gateway. Recoil settles faster between controlled bursts."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 1, 1);
    TriggerDiscipline->GrantedTags.AddTag(BreakerNodeTags::Node_TriggerDiscipline.GetTag());
    Tree->Nodes.Add(TriggerDiscipline);

    UBreakerProgressionNode* Cyclic = MakeNode(TEXT("Core.Volley.Cyclic"), TEXT("Cyclic"),
        TEXT("Sustained fire ramps rate of fire, then decays when you stop. Every rank increases damage dealt."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 3, 1);
    AddPrerequisite(Cyclic, TEXT("Core.Volley.TriggerDiscipline"));
    // The rate-of-fire ramp is still only a tag nothing consumes, so until the
    // weapon layer reads it this node's three ranks were a purchase that did
    // literally nothing. The Increased Damage per rank is real output now and
    // stays when the ramp lands.
    AddEffect(Cyclic, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 3.0f); // O2 PLACEHOLDER
    Cyclic->GrantedTags.AddTag(BreakerNodeTags::Node_Cyclic.GetTag());
    Tree->Nodes.Add(Cyclic);

    UBreakerProgressionNode* LastRound = MakeNode(TEXT("Core.Volley.LastRound"), TEXT("Last Round"),
        TEXT("The final round of a magazine fires extra projectiles. They apply no statuses (proc coefficient 0)."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 1, 2);
    AddPrerequisite(LastRound, TEXT("Core.Volley.TriggerDiscipline"));
    LastRound->GrantedTags.AddTag(BreakerNodeTags::Node_LastRound.GetTag());
    Tree->Nodes.Add(LastRound);

    // The generalist damage ladder. Deliberately the LARGEST unconditional
    // per-rank damage in the tree and deliberately the most boring: it is the
    // control against which every conditional node is measured, and a build
    // that takes only this is exactly the "baseline" the variance band is
    // defined against.
    UBreakerProgressionNode* Salvo = MakeNode(TEXT("Core.Volley.Salvo"), TEXT("Salvo"),
        TEXT("Volume over placement. Every rank increases all damage dealt, with no condition attached."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 3, 1);
    AddPrerequisite(Salvo, TEXT("Core.Volley.TriggerDiscipline"));
    AddEffect(Salvo, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 6.0f); // O2 PLACEHOLDER
    Salvo->GrantedTags.AddTag(BreakerNodeTags::Node_Salvo.GetTag());
    Tree->Nodes.Add(Salvo);

    // Second unconditional More. Fixate and Barrage are both generalists, so a
    // build that wants three Mores and refuses to commit to a movement state
    // can find only two — which is the shape O27 asks for: the uncommitted
    // build is viable, the committed one is stronger.
    UBreakerProgressionNode* Barrage = MakeNode(TEXT("Core.Volley.Barrage"), TEXT("Barrage"),
        TEXT("Convergence. Sustained output becomes a MORE multiplier to all damage dealt."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 3);
    AddPrerequisite(Barrage, TEXT("Core.Volley.Cyclic"));
    AddDamageMore(Barrage, 22.0f); // O2 PLACEHOLDER: x1.22
    Barrage->GrantedTags.AddTag(BreakerNodeTags::Node_Barrage.GetTag());
    Tree->Nodes.Add(Barrage);

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

    // --- Velocity ----------------------------------------------------------
    // NEW under O27. "Movement is part of character building rather than a
    // fixed utility layer" was true of the movement STATS and false of
    // everything that mattered: no node anywhere converted a movement state
    // into damage, so the pillar had no offensive expression at all.
    //
    // Four laddered conditionals and two Convergences. Each pays roughly twice
    // what Salvo's unconditional rank pays, and each is worth nothing while you
    // stand still — the trade that makes a movement build a build.
    UBreakerProgressionNode* Freefall = MakeNode(TEXT("Core.Velocity.Freefall"), TEXT("Freefall"),
        TEXT("Velocity gateway. Increased damage while airborne. Nothing while your feet are down."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 3, 1);
    AddEffect(Freefall, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 9.0f, EBreakerBuildCondition::Airborne); // O2 PLACEHOLDER
    Freefall->GrantedTags.AddTag(BreakerNodeTags::Node_Freefall.GetTag());
    Tree->Nodes.Add(Freefall);

    UBreakerProgressionNode* Slipstream = MakeNode(TEXT("Core.Velocity.Slipstream"), TEXT("Slipstream"),
        TEXT("Velocity gateway. Increased damage while sliding, and slides carry further."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 1, 3, 1);
    AddEffect(Slipstream, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 9.0f, EBreakerBuildCondition::Sliding); // O2 PLACEHOLDER
    AddEffect(Slipstream, EBreakerNodeStatTarget::SlideSpeed, EBreakerNodeStatBucket::IncreasedPercent, 5.0f);                              // O2 PLACEHOLDER
    Slipstream->GrantedTags.AddTag(BreakerNodeTags::Node_Slipstream.GetTag());
    Tree->Nodes.Add(Slipstream);

    // The narrowest condition in the tree pays the most per rank. Wall riding
    // is the hardest state to hold, so the node is priced for the player who
    // actually builds their traversal around it.
    UBreakerProgressionNode* Traction = MakeNode(TEXT("Core.Velocity.Traction"), TEXT("Traction"),
        TEXT("Increased damage while wall riding. The narrowest window in the constellation, and the largest per rank."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 2, 1);
    AddPrerequisite(Traction, TEXT("Core.Velocity.Freefall"));
    AddEffect(Traction, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 14.0f, EBreakerBuildCondition::WallRiding); // O2 PLACEHOLDER
    Traction->GrantedTags.AddTag(BreakerNodeTags::Node_Traction.GetTag());
    Tree->Nodes.Add(Traction);

    UBreakerProgressionNode* Afterburn = MakeNode(TEXT("Core.Velocity.Afterburn"), TEXT("Afterburn"),
        TEXT("Increased damage for a few seconds after dashing. The one Velocity line you can trigger on demand."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 2, 3, 1);
    AddPrerequisite(Afterburn, TEXT("Core.Velocity.Slipstream"));
    AddEffect(Afterburn, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 8.0f, EBreakerBuildCondition::RecentlyDashed); // O2 PLACEHOLDER
    Afterburn->GrantedTags.AddTag(BreakerNodeTags::Node_Afterburn.GetTag());
    Tree->Nodes.Add(Afterburn);

    // The largest single multiplier in the game and the hardest to keep on.
    // Capped at the Damage-Pipeline §4 per-More ceiling of 1.30x by the
    // aggregator, so authoring it AT the ceiling is a statement that nothing
    // will ever be allowed past it.
    UBreakerProgressionNode* TerminalVelocity = MakeNode(TEXT("Core.Velocity.TerminalVelocity"), TEXT("Terminal Velocity"),
        TEXT("Convergence. A MORE multiplier to all damage dealt while airborne. Land and it is gone."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 3);
    AddPrerequisite(TerminalVelocity, TEXT("Core.Velocity.Traction"));
    AddDamageMore(TerminalVelocity, 30.0f, EBreakerBuildCondition::Airborne); // O2 PLACEHOLDER: x1.30
    TerminalVelocity->GrantedTags.AddTag(BreakerNodeTags::Node_TerminalVelocity.GetTag());
    Tree->Nodes.Add(TerminalVelocity);

    // Class-coupled by construction rather than by a RequiredClass field: the
    // Momentum loop is inert for everyone but Swift, so Redline never fires for
    // another class. It stays a Core node because O15 forbids mutually
    // exclusive tiers and a Tank can see, and decline, the trade honestly.
    UBreakerProgressionNode* RedlineDoctrine = MakeNode(TEXT("Core.Velocity.RedlineDoctrine"), TEXT("Redline Doctrine"),
        TEXT("Convergence. A MORE multiplier to all damage dealt while at Redline Momentum. Inert for a class with no Momentum."), EBreakerPointCurrency::CorePoints, EBreakerClassId::None, 3, 1, 3);
    AddPrerequisite(RedlineDoctrine, TEXT("Core.Velocity.Afterburn"));
    AddDamageMore(RedlineDoctrine, 20.0f, EBreakerBuildCondition::Redline); // O2 PLACEHOLDER: x1.20
    RedlineDoctrine->GrantedTags.AddTag(BreakerNodeTags::Node_RedlineDoctrine.GetTag());
    Tree->Nodes.Add(RedlineDoctrine);

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

    // Kinetic's offensive half. The branch was eight nodes of Momentum-loop
    // knobs, every one of which made movement better at generating Momentum and
    // none of which made movement worth anything offensively — so a Kinetic
    // player's damage came entirely from Core and gear.
    Node = MakeNode(TEXT("Swift.Kinetic.Downforce"), TEXT("Downforce"),
        TEXT("Shots fired while airborne land significantly harder."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.ReadTheRoom"));
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 11.0f, EBreakerBuildCondition::Airborne); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Downforce.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Swift.Kinetic.Grind"), TEXT("Grind"),
        TEXT("Shots fired off a wall ride land significantly harder."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.Contact"));
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 13.0f, EBreakerBuildCondition::WallRiding); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Grind.GetTag());
    Tree->Nodes.Add(Node);

    // The branch keystone O3 permits. Sliding is the state Kinetic can hold
    // most reliably, so its More is the smallest of the four conditional ones.
    Node = MakeNode(TEXT("Swift.Kinetic.Overpressure"), TEXT("Overpressure"),
        TEXT("Branch keystone. A MORE multiplier to all damage dealt while sliding."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 3, 1, 3);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.Carry"));
    AddDamageMore(Node, 20.0f, EBreakerBuildCondition::Sliding); // O2 PLACEHOLDER: x1.20
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Overpressure.GetTag());
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
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 3.0f); // O2 PLACEHOLDER
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
        TEXT("Each target pierced by a shot generates Momentum, up to three. Shots also land harder."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.Steady"));
    AddEffect(Node, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 6.0f); // O2 PLACEHOLDER
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 3.0f); // O2 PLACEHOLDER
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

    // Marksman's crit-chance ladder. Long Lens gave the branch crit DAMAGE with
    // almost no chance to apply it to, which is a stat that reads well on a card
    // and does close to nothing.
    Node = MakeNode(TEXT("Swift.Marksman.Deadeye"), TEXT("Deadeye"),
        TEXT("Held aim finds the weak point. A large flat critical chance per rank."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.LongLens"));
    AddEffect(Node, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 4.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Deadeye.GetTag());
    Tree->Nodes.Add(Node);

    // Marksman's branch keystone: the only unconditional More outside Core, and
    // the pick for a build that refuses to organise itself around a movement
    // state. It is the smallest unconditional More for exactly that reason.
    Node = MakeNode(TEXT("Swift.Marksman.Culling"), TEXT("Culling"),
        TEXT("Branch keystone. A MORE multiplier to all damage dealt, with no condition attached."), EBreakerPointCurrency::ClassPoints, EBreakerClassId::Swift, 3, 1, 3);
    AddPrerequisite(Node, TEXT("Swift.Marksman.PierceDiscipline"));
    AddDamageMore(Node, 18.0f); // O2 PLACEHOLDER: x1.18
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Culling.GetTag());
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
    // Ids must match the ability fallback registry exactly, or a loadout
    // seeded from them resolves to nothing.
    Swift->StartingClassAbilityIds = {TEXT("Swift.Skim"), TEXT("Swift.Lead")};
    Swift->BaseUltimateId = TEXT("Swift.Overdrive");
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
