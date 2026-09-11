#include "Progression/BreakerProgressionLibrary.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Classes/BreakerManaComponent.h"
#include "Data/BreakerDataFile.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#define LOCTEXT_NAMESPACE "BreakerProgressionContent"

namespace BreakerNodeTags
{
    UE_DEFINE_GAMEPLAY_TAG(Node_Fixate, "Progression.Node.Core.Fixate");
    UE_DEFINE_GAMEPLAY_TAG(Node_TunnelVision, "Progression.Node.Core.TunnelVision");
    UE_DEFINE_GAMEPLAY_TAG(Node_Core_Deadeye, "Progression.Node.Core.Precision.Deadeye");
    UE_DEFINE_GAMEPLAY_TAG(Node_Core_Interposition, "Progression.Node.Core.Bulwark.Interposition");
    UE_DEFINE_GAMEPLAY_TAG(Node_Core_Execute, "Progression.Node.Core.Ruin.Execute");
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

    // Kinetic TIER 4 — Class-Kits §1.4 K9-K11.
    UE_DEFINE_GAMEPLAY_TAG(Node_MomentumShield, "Progression.Node.Swift.Kinetic.MomentumShield");
    UE_DEFINE_GAMEPLAY_TAG(Node_SpendToLive, "Progression.Node.Swift.Kinetic.SpendToLive");
    UE_DEFINE_GAMEPLAY_TAG(Node_NoGround, "Progression.Node.Swift.Kinetic.NoGround");

    UE_DEFINE_GAMEPLAY_TAG(Node_LongLens, "Progression.Node.Swift.Marksman.LongLens");
    UE_DEFINE_GAMEPLAY_TAG(Node_Steady, "Progression.Node.Swift.Marksman.Steady");
    UE_DEFINE_GAMEPLAY_TAG(Node_Ledger, "Progression.Node.Swift.Marksman.Ledger");
    UE_DEFINE_GAMEPLAY_TAG(Node_Angle, "Progression.Node.Swift.Marksman.Angle");
    UE_DEFINE_GAMEPLAY_TAG(Node_MarkEconomy, "Progression.Node.Swift.Marksman.MarkEconomy");
    UE_DEFINE_GAMEPLAY_TAG(Node_PierceDiscipline, "Progression.Node.Swift.Marksman.PierceDiscipline");
    UE_DEFINE_GAMEPLAY_TAG(Node_Sightline, "Progression.Node.Swift.Marksman.Sightline");
    UE_DEFINE_GAMEPLAY_TAG(Node_Lead, "Progression.Node.Swift.Marksman.Lead");

    // Marksman TIER 4 — Class-Kits §1.5 M9-M11. M11's DISPLAY name, "Called
    // Shot", collides with the Core Precision node of the same name and nothing
    // else: separate node id, separate tag, separate currency. Same resolution
    // as Frenzy's Trigger Discipline below — both names are transcribed from
    // their design documents and renaming either to dodge the collision would
    // put the code and the authority document out of step.
    UE_DEFINE_GAMEPLAY_TAG(Node_Reserve, "Progression.Node.Swift.Marksman.Reserve");
    UE_DEFINE_GAMEPLAY_TAG(Node_Overpenetration, "Progression.Node.Swift.Marksman.Overpenetration");
    UE_DEFINE_GAMEPLAY_TAG(Node_MarksmanCalledShot, "Progression.Node.Swift.Marksman.CalledShot");

    UE_DEFINE_GAMEPLAY_TAG(Node_Downforce, "Progression.Node.Swift.Kinetic.Downforce");
    UE_DEFINE_GAMEPLAY_TAG(Node_Overpressure, "Progression.Node.Swift.Kinetic.Overpressure");
    UE_DEFINE_GAMEPLAY_TAG(Node_Deadeye, "Progression.Node.Swift.Marksman.Deadeye");
    UE_DEFINE_GAMEPLAY_TAG(Node_Culling, "Progression.Node.Swift.Marksman.Culling");

    // FRENZY. Its Trigger Discipline shares a DISPLAY name with Volley's
    // gateway and nothing else: separate node id, separate tag, separate
    // currency. Both names are transcribed from their design documents and
    // renaming either to avoid the collision would put the code and the
    // authority document out of step, which is the worse failure.
    UE_DEFINE_GAMEPLAY_TAG(Node_FrenzyTrigger, "Progression.Node.Swift.Frenzy.TriggerDiscipline");
    UE_DEFINE_GAMEPLAY_TAG(Node_Loaded, "Progression.Node.Swift.Frenzy.Loaded");
    UE_DEFINE_GAMEPLAY_TAG(Node_ShortLeash, "Progression.Node.Swift.Frenzy.ShortLeash");
    UE_DEFINE_GAMEPLAY_TAG(Node_Rhythm, "Progression.Node.Swift.Frenzy.Rhythm");
    UE_DEFINE_GAMEPLAY_TAG(Node_DryFire, "Progression.Node.Swift.Frenzy.DryFire");
    UE_DEFINE_GAMEPLAY_TAG(Node_Feed, "Progression.Node.Swift.Frenzy.Feed");
    UE_DEFINE_GAMEPLAY_TAG(Node_Overrev, "Progression.Node.Swift.Frenzy.Overrev");
    UE_DEFINE_GAMEPLAY_TAG(Node_SlipcutMastery, "Progression.Node.Swift.Frenzy.SlipcutMastery");
    UE_DEFINE_GAMEPLAY_TAG(Node_AmmunitionEconomy, "Progression.Node.Swift.Frenzy.AmmunitionEconomy");
    UE_DEFINE_GAMEPLAY_TAG(Node_Bloodrhythm, "Progression.Node.Swift.Frenzy.Bloodrhythm");

    // Frenzy TIER 4 — Class-Kits §1.3 F9-F11.
    UE_DEFINE_GAMEPLAY_TAG(Node_SecondWind, "Progression.Node.Swift.Frenzy.SecondWind");
    UE_DEFINE_GAMEPLAY_TAG(Node_RedlineTrigger, "Progression.Node.Swift.Frenzy.RedlineTrigger");
    UE_DEFINE_GAMEPLAY_TAG(Node_NoSafety, "Progression.Node.Swift.Frenzy.NoSafety");

    UE_DEFINE_GAMEPLAY_TAG(Node_Conductive, "Progression.Node.Core.Conductive");
    UE_DEFINE_GAMEPLAY_TAG(Node_ChargeUp, "Progression.Node.Core.ChargeUp");
    UE_DEFINE_GAMEPLAY_TAG(Node_Threshold, "Progression.Node.Core.Threshold");
    UE_DEFINE_GAMEPLAY_TAG(Node_Catalyst, "Progression.Node.Core.Catalyst");
    UE_DEFINE_GAMEPLAY_TAG(Node_Penetrance, "Progression.Node.Core.Penetrance");
    UE_DEFINE_GAMEPLAY_TAG(Node_ReactionChain, "Progression.Node.Core.ReactionChain");

    // Caster / SPELLBLADE (Class-Kits §2.3).
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_ContactCharge, "Progression.Node.Caster.Spellblade.ContactCharge");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_FollowThrough, "Progression.Node.Caster.Spellblade.FollowThrough");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Close, "Progression.Node.Caster.Spellblade.Close");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Debt, "Progression.Node.Caster.Spellblade.Debt");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_MomentumTransfer, "Progression.Node.Caster.Spellblade.MomentumTransfer");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Bloodprice, "Progression.Node.Caster.Spellblade.Bloodprice");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Blink, "Progression.Node.Caster.Spellblade.Blink");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Edge, "Progression.Node.Caster.Spellblade.Edge");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Edgework, "Progression.Node.Caster.Spellblade.Edgework");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_NoDistance, "Progression.Node.Caster.Spellblade.NoDistance");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Reprisal, "Progression.Node.Caster.Spellblade.Reprisal");
    UE_DEFINE_GAMEPLAY_TAG(Node_SB_Overreach, "Progression.Node.Caster.Spellblade.Overreach");

    // Caster / VOID WHISPERER (Class-Kits §2.4).
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_Seep, "Progression.Node.Caster.VoidWhisperer.Seep");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_StandingWater, "Progression.Node.Caster.VoidWhisperer.StandingWater");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_Patience, "Progression.Node.Caster.VoidWhisperer.Patience");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_Lingering, "Progression.Node.Caster.VoidWhisperer.Lingering");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_Attrition, "Progression.Node.Caster.VoidWhisperer.Attrition");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_Drain, "Progression.Node.Caster.VoidWhisperer.Drain");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_Zonework, "Progression.Node.Caster.VoidWhisperer.Zonework");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_Wellspring, "Progression.Node.Caster.VoidWhisperer.Wellspring");
    UE_DEFINE_GAMEPLAY_TAG(Node_VW_LongDark, "Progression.Node.Caster.VoidWhisperer.LongDark");

    // Caster / MULTISPELL (Class-Kits §2.5).
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Variance, "Progression.Node.Caster.Multispell.Variance");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Cycle, "Progression.Node.Caster.Multispell.Cycle");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Reservoir, "Progression.Node.Caster.Multispell.Reservoir");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Chain, "Progression.Node.Caster.Multispell.Chain");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Payment, "Progression.Node.Caster.Multispell.Payment");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Sequence, "Progression.Node.Caster.Multispell.Sequence");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Fracture, "Progression.Node.Caster.Multispell.Fracture");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Resonance, "Progression.Node.Caster.Multispell.Resonance");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Cascade, "Progression.Node.Caster.Multispell.Cascade");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Interference, "Progression.Node.Caster.Multispell.Interference");
    UE_DEFINE_GAMEPLAY_TAG(Node_MS_Prepared, "Progression.Node.Caster.Multispell.Prepared");

    // Gunsmith / ARMORY (Class-Kits-Gunsmith §4.1). AR5's DISPLAY name, "Last
    // Round", collides with Core.Volley.LastRound and nothing else, and AR10's
    // "Overpressure" collides with Swift.Kinetic.Overpressure (and Tank D6
    // below) the same way — separate node ids, separate tags, separate
    // currencies. All are transcribed from their design documents; renaming any
    // to dodge a display collision would put code and authority out of step,
    // the standing resolution since Frenzy's Trigger Discipline.
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_FieldStripping, "Progression.Node.Gunsmith.Armory.FieldStripping");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_WorkingStock, "Progression.Node.Gunsmith.Armory.WorkingStock");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_Chambered, "Progression.Node.Gunsmith.Armory.Chambered");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_DeepPockets, "Progression.Node.Gunsmith.Armory.DeepPockets");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_LastRound, "Progression.Node.Gunsmith.Armory.LastRound");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_ColdBarrel, "Progression.Node.Gunsmith.Armory.ColdBarrel");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_BenchWork, "Progression.Node.Gunsmith.Armory.BenchWork");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_RigDiscipline, "Progression.Node.Gunsmith.Armory.RigDiscipline");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_Reciprocal, "Progression.Node.Gunsmith.Armory.Reciprocal");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_Overpressure, "Progression.Node.Gunsmith.Armory.Overpressure");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_NoReserve, "Progression.Node.Gunsmith.Armory.NoReserve");
    UE_DEFINE_GAMEPLAY_TAG(Node_AR_Machinist, "Progression.Node.Gunsmith.Armory.Machinist");

    // Gunsmith / FIELD TECH (Class-Kits-Gunsmith §4.2). FT7's "Emplacement"
    // shares a display name with Tank B7 below — same resolution as above.
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Salvage, "Progression.Node.Gunsmith.FieldTech.Salvage");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Overwatch, "Progression.Node.Gunsmith.FieldTech.Overwatch");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_SecondShift, "Progression.Node.Gunsmith.FieldTech.SecondShift");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Tithe, "Progression.Node.Gunsmith.FieldTech.Tithe");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Requisition, "Progression.Node.Gunsmith.FieldTech.Requisition");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Foreman, "Progression.Node.Gunsmith.FieldTech.Foreman");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Emplacement, "Progression.Node.Gunsmith.FieldTech.Emplacement");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Logistics, "Progression.Node.Gunsmith.FieldTech.Logistics");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Redundancy, "Progression.Node.Gunsmith.FieldTech.Redundancy");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Automation, "Progression.Node.Gunsmith.FieldTech.Automation");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Deadman, "Progression.Node.Gunsmith.FieldTech.Deadman");
    UE_DEFINE_GAMEPLAY_TAG(Node_FT_Foundry, "Progression.Node.Gunsmith.FieldTech.Foundry");

    // Gunsmith / TINKERER (Class-Kits-Gunsmith §4.3). TK9's "Patience" shares
    // a display name with Caster VW3 — same resolution as above.
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_CheapWork, "Progression.Node.Gunsmith.Tinkerer.CheapWork");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_QuickSet, "Progression.Node.Gunsmith.Tinkerer.QuickSet");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_Tripwire, "Progression.Node.Gunsmith.Tinkerer.Tripwire");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_Rearm, "Progression.Node.Gunsmith.Tinkerer.Rearm");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_AttritionField, "Progression.Node.Gunsmith.Tinkerer.AttritionField");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_Overlap, "Progression.Node.Gunsmith.Tinkerer.Overlap");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_Ordnance, "Progression.Node.Gunsmith.Tinkerer.Ordnance");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_Interdiction, "Progression.Node.Gunsmith.Tinkerer.Interdiction");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_Patience, "Progression.Node.Gunsmith.Tinkerer.Patience");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_DeadGround, "Progression.Node.Gunsmith.Tinkerer.DeadGround");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_CommandDetonation, "Progression.Node.Gunsmith.Tinkerer.CommandDetonation");
    UE_DEFINE_GAMEPLAY_TAG(Node_TK_Minefield, "Progression.Node.Gunsmith.Tinkerer.Minefield");

    // Tank / LEECH (Class-Kits-Tank §3). L3's "Open Wound" shares a display
    // name with Core.Affliction.OpenWound — same resolution as above.
    UE_DEFINE_GAMEPLAY_TAG(Node_L_Clot, "Progression.Node.Tank.Leech.Clot");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_SlowBleed, "Progression.Node.Tank.Leech.SlowBleed");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_OpenWound, "Progression.Node.Tank.Leech.OpenWound");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_FeedTheWound, "Progression.Node.Tank.Leech.FeedTheWound");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_Bloodlet, "Progression.Node.Tank.Leech.Bloodlet");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_Transfusion, "Progression.Node.Tank.Leech.Transfusion");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_RendMastery, "Progression.Node.Tank.Leech.RendMastery");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_SecondHeart, "Progression.Node.Tank.Leech.SecondHeart");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_NothingWasted, "Progression.Node.Tank.Leech.NothingWasted");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_Reciprocity, "Progression.Node.Tank.Leech.Reciprocity");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_Exsanguinate, "Progression.Node.Tank.Leech.Exsanguinate");
    UE_DEFINE_GAMEPLAY_TAG(Node_L_Vein, "Progression.Node.Tank.Leech.Vein");

    // Tank / BASTION (Class-Kits-Tank §4).
    UE_DEFINE_GAMEPLAY_TAG(Node_B_LineOfSight, "Progression.Node.Tank.Bastion.LineOfSight");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_Footing, "Progression.Node.Tank.Bastion.Footing");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_Loud, "Progression.Node.Tank.Bastion.Loud");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_HeldGround, "Progression.Node.Tank.Bastion.HeldGround");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_AnsweringFire, "Progression.Node.Tank.Bastion.AnsweringFire");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_Bulk, "Progression.Node.Tank.Bastion.Bulk");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_Emplacement, "Progression.Node.Tank.Bastion.Emplacement");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_Interposition, "Progression.Node.Tank.Bastion.Interposition");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_Conversion, "Progression.Node.Tank.Bastion.Conversion");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_StandingOrder, "Progression.Node.Tank.Bastion.StandingOrder");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_ImmovableObject, "Progression.Node.Tank.Bastion.ImmovableObject");
    UE_DEFINE_GAMEPLAY_TAG(Node_B_Wall, "Progression.Node.Tank.Bastion.Wall");

    // Tank / DEMOLITIONIST (Class-Kits-Tank §5).
    UE_DEFINE_GAMEPLAY_TAG(Node_D_ShapedCharge, "Progression.Node.Tank.Demolitionist.ShapedCharge");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_Bootstraps, "Progression.Node.Tank.Demolitionist.Bootstraps");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_BracedForImpact, "Progression.Node.Tank.Demolitionist.BracedForImpact");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_Fragmentation, "Progression.Node.Tank.Demolitionist.Fragmentation");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_Concussion, "Progression.Node.Tank.Demolitionist.Concussion");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_Overpressure, "Progression.Node.Tank.Demolitionist.Overpressure");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_Demolition, "Progression.Node.Tank.Demolitionist.Demolition");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_TerminalDescent, "Progression.Node.Tank.Demolitionist.TerminalDescent");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_BlastRadius, "Progression.Node.Tank.Demolitionist.BlastRadius");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_KineticRecovery, "Progression.Node.Tank.Demolitionist.KineticRecovery");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_ChainReaction, "Progression.Node.Tank.Demolitionist.ChainReaction");
    UE_DEFINE_GAMEPLAY_TAG(Node_D_Detonation, "Progression.Node.Tank.Demolitionist.Detonation");

    // Support / MEDIC (Class-Kits-Support §4.1).
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_FieldDressing, "Progression.Node.Support.Medic.FieldDressing");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_TriagePriority, "Progression.Node.Support.Medic.TriagePriority");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_CleanHands, "Progression.Node.Support.Medic.CleanHands");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_SteadyHands, "Progression.Node.Support.Medic.SteadyHands");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_SecondOpinion, "Progression.Node.Support.Medic.SecondOpinion");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_Attending, "Progression.Node.Support.Medic.Attending");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_FieldKit, "Progression.Node.Support.Medic.FieldKit");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_SustainedCare, "Progression.Node.Support.Medic.SustainedCare");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_Overflow, "Progression.Node.Support.Medic.Overflow");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_BloodDebt, "Progression.Node.Support.Medic.BloodDebt");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_NoTriage, "Progression.Node.Support.Medic.NoTriage");
    UE_DEFINE_GAMEPLAY_TAG(Node_MD_Triage, "Progression.Node.Support.Medic.Triage");

    // Support / CONDUCTOR (Class-Kits-Support §4.2).
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_DownbeatDiscipline, "Progression.Node.Support.Conductor.DownbeatDiscipline");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Section, "Progression.Node.Support.Conductor.Section");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Sustain, "Progression.Node.Support.Conductor.Sustain");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Rehearsal, "Progression.Node.Support.Conductor.Rehearsal");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Tempo, "Progression.Node.Support.Conductor.Tempo");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Attunement, "Progression.Node.Support.Conductor.Attunement");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Conducting, "Progression.Node.Support.Conductor.Conducting");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Counterpoint, "Progression.Node.Support.Conductor.Counterpoint");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_StandingOvation, "Progression.Node.Support.Conductor.StandingOvation");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_SympatheticResonance, "Progression.Node.Support.Conductor.SympatheticResonance");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_DetachedBaton, "Progression.Node.Support.Conductor.DetachedBaton");
    UE_DEFINE_GAMEPLAY_TAG(Node_CO_Downbeat, "Progression.Node.Support.Conductor.Downbeat");

    // Support / WARDEN (Class-Kits-Support §4.3).
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_Painted, "Progression.Node.Support.Warden.Painted");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_LongWatch, "Progression.Node.Support.Warden.LongWatch");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_FieldOfView, "Progression.Node.Support.Warden.FieldOfView");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_Handoff, "Progression.Node.Support.Warden.Handoff");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_Pressure, "Progression.Node.Support.Warden.Pressure");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_Tell, "Progression.Node.Support.Warden.Tell");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_Suppression, "Progression.Node.Support.Warden.Suppression");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_DeepMark, "Progression.Node.Support.Warden.DeepMark");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_ExecutionersLedger, "Progression.Node.Support.Warden.ExecutionersLedger");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_BlackoutProtocol, "Progression.Node.Support.Warden.BlackoutProtocol");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_HuntersEconomy, "Progression.Node.Support.Warden.HuntersEconomy");
    UE_DEFINE_GAMEPLAY_TAG(Node_WA_Blackout, "Progression.Node.Support.Warden.Blackout");
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
        int32 CostPerRank,
        // Core constellation membership (audit item 5). Trailing and
        // defaulted to None so every pre-existing call site — every Swift
        // branch node, which has no constellation — is unchanged; only the
        // Core tree's 30 call sites pass one.
        FName Constellation = NAME_None)
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
        Node->Constellation = Constellation;
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

    void BreakerLinkNodes(UBreakerProgressionTree* Tree, FName A, FName B)
    {
        FBreakerNodeEdge Edge;
        Edge.A = A;
        Edge.B = B;
        Tree->AdjacencyEdges.Add(Edge);
    }

    // THESE OBJECTS LIVE FOR THE WHOLE PROCESS, AND THAT LEAKS BETWEEN TESTS.
    // Every tree below is a static, root-set singleton built once and handed
    // out by pointer, so a test that MUTATES a node — changing a cost, a gate,
    // an effect, a rank — changes it for every subsequent test in the same
    // editor session, in declaration order, with no reset between them. The
    // failure looks like a test that passes alone and fails in the suite, or
    // worse, one that passes in the suite because an earlier test happened to
    // set it up. Read the tree; copy anything you intend to change.
    UBreakerProgressionTree* MakeTree(FName TreeId, const TCHAR* DisplayName, EBreakerPointCurrency Currency, EBreakerClassId RequiredClass)
    {
        UBreakerProgressionTree* Tree = NewObject<UBreakerProgressionTree>(GetTransientPackage(), UBreakerProgressionTree::StaticClass(), NAME_None, RF_Standalone);
        Tree->AddToRoot();
        Tree->TreeId = TreeId;
        Tree->DisplayName = FText::FromString(DisplayName);
        Tree->Currency = Currency;
        Tree->RequiredClass = RequiredClass;
        // THE CORNERSTONE GATE IS A CORE-ONLY CONCEPT NOW (owner ruling).
        //
        // It was 8 points spent in the tree, authored when a class tree drew on
        // a per-level budget where 8 + the keystone's 3 was affordable. O111 set
        // the doctrine wallet to 8 and did not move the gate, so all fifteen
        // doctrine keystones needed 11 out of 8 and not one of them could be
        // bought. That is the second time this project has shipped an
        // unpurchasable keystone from exactly this cause.
        //
        // Zeroed HERE rather than on fifteen trees on purpose. Fifteen
        // assignments are fifteen chances for the sixteenth doctrine to be
        // authored without one, and a gate that has to be remembered is the
        // thing that failed. A doctrine's depth is its tier gates, which are
        // derived from the budget by GateForTier rather than pinned beside it.
        if (Currency == EBreakerPointCurrency::DoctrinePoints)
        {
            Tree->CornerstoneInvestmentGate = 0;
        }
        return Tree;
    }
}

// Node stat magnitudes below are gym-perceptibility tuning; wave-mode re-anchors.
int32 UBreakerProgressionLibrary::AbilityTokenLevelForIndex(int32 Index, int32 Count)
{
    // Convex interpolation between the two authored ends (see the O138 block
    // at the declaration). Index clamps rather than errors so a caller
    // walking one past the end reads the completion level instead of garbage.
    if (Count <= 0) return TNumericLimits<int32>::Max();
    if (Count == 1 || Index <= 0) return FirstAbilityTokenLevel;
    const int32 Clamped = FMath::Min(Index, Count - 1);
    const float T = static_cast<float>(Clamped) / static_cast<float>(Count - 1);
    const float Shaped = FMath::Pow(T, AbilityTokenSpacingExponent);
    return FMath::RoundToInt32(FirstAbilityTokenLevel
        + Shaped * static_cast<float>(AbilityCompletionLevel - FirstAbilityTokenLevel));
}

int32 UBreakerProgressionLibrary::AbilityTokenEntitlement(int32 CharacterLevel, int32 UnlockableCount)
{
    // Count the derived milestones the character has reached. The schedule's
    // length IS the unlockable count (O138), so the old clamp — a token that
    // cannot buy anything is a counter the player watches and cannot use —
    // now holds by construction rather than by truncation.
    int32 Reached = 0;
    for (int32 Index = 0; Index < UnlockableCount; ++Index)
    {
        if (CharacterLevel >= AbilityTokenLevelForIndex(Index, UnlockableCount)) ++Reached;
    }
    return Reached;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetCoreSliceTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    FString Error;
    Tree = BreakerCoreRoster::BuildCandidate(GetTransientPackage(), Error);
    if (!Tree)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Cannot build the authored Core roster: %s"), *Error);
        return nullptr;
    }
    // Preserve the public tree identity while layout versioning refunds every
    // old allocation before any reused node ID can acquire its new meaning.
    Tree->TreeId = TEXT("Core.Slice");
    Tree->AddToRoot();
    return Tree;
}

// ---------------------------------------------------------------------------
// SWIFT TIER 4 — THE REWRITE TIER, AUTHORED AS ONE PASS ACROSS ALL THREE
// BRANCHES. Stated here once rather than nine times.
//
// Class-Kits §0.2 gives every branch a five-tier shape and §1.3-1.5 author the
// Tier-4 rewrite trio for each Swift branch — F9-F11, K9-K11, M9-M11. The
// shipped slice stopped at tier 3 and dropped all nine. They are authored here.
// Caster's three branches were cut the same way and are NOT touched by this
// pass; only Swift is expanded.
//
// EVERY ONE OF THE NINE SHIPS AS A GRANTED TAG AND NO STAT EFFECT. That is not
// a shortfall to fix later, it is what the tier IS. Read the nine rows in
// §1.3-1.5: "Cadence Break's bonus no longer resets on target swap", "Hard
// Stop's window becomes full immunity, cost rises to 60", "Momentum does not
// decay while ADS", "shots that kill continue with full remaining damage".
// Every one is an ABILITY rewrite, a MOMENTUM-LOOP rewrite, or an AFFIX-rule
// rewrite. Not one is a percentage, and §0.2's own rule — "no node in this
// document is a flat percentage" — is loudest exactly here. There is no
// EBreakerNodeStatTarget for Momentum decay, ability cost, incoming damage
// reduction, ADS behaviour, or Damage Ramp stack rate, and O30 keeps
// EBreakerBuildCondition movement-only so a combat / ammunition /
// ability-state condition cannot be authored either. The precedent is the
// whole Caster branch set added tonight and, before it, Swift's own Cadence
// Break rule on Slipcut Mastery: carry the rule verbatim as a tag, name the
// consumer it is waiting on, and author no number.
//
// WHY NO STAT HALF, given tiers 1-3 of Frenzy carry one. §1.3.1 authored a
// stat half for each of TEN specific nodes and lists them in a table, because
// a branch of pure tags is a branch a player can buy and cannot feel. A tier-3
// branch had to pay for itself somehow. The tier-4 trio does not have that
// problem: it sits on top of ten nodes that already pay, and inventing a
// magnitude for it would be authoring balance under an O2 freeze on content
// §1.3.1 does not record. Structure and identity here; numbers when the loops
// that own them can read these tags.
//
// NO MORE MULTIPLIER IS AUTHORED IN THIS PASS. O3 confines a class-layer More
// to a branch KEYSTONE, and all three Swift keystones already exist and are
// already spent (Bloodrhythm x1.20 Redline, Overpressure x1.20 sliding,
// Culling x1.18 unconditional). Swift's budget of three is full; a tier-4 More
// would be a fourth. The composed worst case is unchanged by this pass.
//
// TIER NUMBER vs. GATE, A REAL INVERSION, RECORDED NOT PAPERED OVER. These
// nodes carry Tier == 4, so GateForTier prices them at 6 points of branch
// investment. The three shipped keystones carry Tier == 3 (gate 4) because the
// slice compressed §0.2's five tiers into three. The consequence is that a
// branch's keystone is now reachable EARLIER than its rewrites, which inverts
// §0.2's ladder. Re-tiering the keystones to 5 would move authored gates and
// costs under an O2 freeze and would break the commitment-gate investment
// figures pinned in BreakerProgressionAuditTests; authoring the rewrites at
// tier 3 would flatten the ladder entirely and hide the gap. So the honest
// shape ships and the inversion is written down. Owner's to rule.
// ---------------------------------------------------------------------------
UBreakerProgressionTree* UBreakerProgressionLibrary::GetSwiftKineticTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Swift.Kinetic"), TEXT("Swift — Kinetic"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift);

    // THE GRANTED NODE (ORDERS ruling 1): Swift's second free verb, the
    // enhanced-dash passive — a node, never a slot occupant. Seeded at rank 1
    // wherever a character becomes or loads as Swift
    // (UBreakerProgressionComponent::SeedGrantedNodes), COST ZERO on purpose:
    // a refund of rank x 0 is 0, so the doctrine respec's refund arithmetic
    // and every spent-points total are untouched by construction — the
    // respec-no-refund property as arithmetic rather than as a special case.
    // Single rank today; higher purchasable ranks are the ruled extension and
    // arrive with their own costs, never by making rank 1 purchasable.
    // DISTANCE, not cooldown — the seat's argument: a cooldown shave is
    // invisible until the player spams, distance is felt on the first dash.
    UBreakerProgressionNode* Longstride = MakeNode(TEXT("Swift.Kinetic.Longstride"), TEXT("Longstride"),
        TEXT("Your dash carries further. Granted with the class."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 1, 0);
    AddEffect(Longstride, EBreakerNodeStatTarget::DashDistance, EBreakerNodeStatBucket::IncreasedPercent, 20.0f); // O2 PLACEHOLDER — felt on the first dash
    Tree->Nodes.Add(Longstride);

    // Kinetic's entry loop knob: lengthening the airborne credit window is what
    // lets a jump chain pay across the gap between two surfaces rather than only
    // while a foot is on one.
    // WAITING ON: the Momentum component's airborne credit window reading this
    // tag. Cheap - the valve is already there, nothing turns it.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Swift.Kinetic.ReadTheRoom"), TEXT("Read the Room"),
        TEXT("Airborne Momentum generation credit lasts longer."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_ReadTheRoom.GetTag());
    Tree->Nodes.Add(Node);

    // Makes SHORT walls worth riding. Without a grace period after contact is
    // lost, a two-metre wall pays nothing and the branch quietly demands
    // architecture the field does not always have.
    // O144 retired wall-ride. Contact now rewards the replacement verbs.
    Node = MakeNode(TEXT("Swift.Kinetic.Contact"), TEXT("Contact"),
        TEXT("After completing a vault or mantle, gain 8 Momentum per second for 0.35/0.70 seconds at ranks 1/2. Shares the movement income cap and traversal interval."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Contact.GetTag());
    Tree->Nodes.Add(Node);

    // The slide-chain node. Its SlideSpeed line is live and does the felt half;
    // the tag is the income half, so chaining slides is a Momentum decision and
    // not only a speed one.
    // WAITING ON: a slide-chain generation credit in the Momentum component.
    Node = MakeNode(TEXT("Swift.Kinetic.Carry"), TEXT("Carry"),
        TEXT("Slide chaining pays flat Momentum and carries more speed."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::SlideSpeed, EBreakerNodeStatBucket::IncreasedPercent, 12.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Carry.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16 (partially): the AbilityCooldown lane exists, so the
    // node's cooldown half pays — cooldowns started while airborne run 20%
    // shorter per rank (O2 PLACEHOLDER; the divisor convention, evaluated at
    // cast, including a Slipcut cast airborne). The designed
    // once-per-airtime EVENT ("sharp direction changes refund") is a rule the
    // lane cannot say; the tag stays for that consumer, and the text below
    // describes what the purchase actually does today rather than what it
    // will do then.
    Node = MakeNode(TEXT("Swift.Kinetic.Redirect"), TEXT("Redirect"),
        TEXT("Abilities cast airborne start a shorter cooldown."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.ReadTheRoom"));
    AddEffect(Node, EBreakerNodeStatTarget::AbilityCooldown, EBreakerNodeStatBucket::IncreasedPercent, 20.0f, EBreakerBuildCondition::Airborne); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Redirect.GetTag());
    Tree->Nodes.Add(Node);

    // Kinetic's identity loop - evasion IS the resource, which is the thing no
    // other class converts. The DodgeChance line is live; the rule half (a
    // larger yield on a shorter internal cooldown) is what makes the loop a
    // build rather than a trickle.
    // WAITING ON: the dodge proc's yield and ICD reading this tag.
    Node = MakeNode(TEXT("Swift.Kinetic.EvadeConversion"), TEXT("Evade Conversion"),
        TEXT("The passive dodge proc yields more Momentum on a shorter internal cooldown."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::DodgeChance, EBreakerNodeStatBucket::Flat, 4.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_EvadeConversion.GetTag());
    Tree->Nodes.Add(Node);

    // The node that turns HEIGHT into a resource, so the vertical half of the
    // movement kit has an income and a fall is a choice rather than a cost.
    // WAITING ON: a fall-distance credit paid on landing.
    Node = MakeNode(TEXT("Swift.Kinetic.Landing"), TEXT("Landing"),
        TEXT("Long falls convert into Momentum on landing."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.ReadTheRoom"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Landing.GetTag());
    Tree->Nodes.Add(Node);

    // O258 retires Skim and its airtime-only node; v9 -> v10 refunds paid ranks.

    // Air Work also carries the airborne-multishot buy-up (owner ruling
    // 2026-08-16): the base momentum coupling's airborne bonus halved
    // 1.0 -> 0.5 (MomentumChannelBonus), and the other half moved HERE as a
    // purchase — +0.5 projectile while airborne, restoring the full doubled
    // shot for a build that pays for it. Air Work is the natural home: it is
    // Kinetic's airborne-craft node, already about doing more with airtime.
    // The ProjectileCount Flat lane composes with the coupling's 0.5 in
    // GetShotChannels, and the Airborne condition means the fraction only
    // accumulates while off the ground.
    Node = MakeNode(TEXT("Swift.Kinetic.AirWork"), TEXT("Air Work"),
        TEXT("Airborne handling improves sharply, and airborne shots recover their full second projectile."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.Landing"));
    AddEffect(Node, EBreakerNodeStatTarget::AirControl, EBreakerNodeStatBucket::IncreasedPercent, 12.0f); // O2 PLACEHOLDER
    AddEffect(Node, EBreakerNodeStatTarget::ProjectileCount, EBreakerNodeStatBucket::Flat, 0.5f, EBreakerBuildCondition::Airborne); // O2 PLACEHOLDER — owner ruling 2026-08-16: restores the halved airborne coupling to a full double
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AirWork.GetTag());
    Tree->Nodes.Add(Node);

    // Kinetic's offensive half. The branch was eight nodes of Momentum-loop
    // knobs, every one of which made movement better at generating Momentum and
    // none of which made movement worth anything offensively — so a Kinetic
    // player's damage came entirely from Core and gear.
    Node = MakeNode(TEXT("Swift.Kinetic.Downforce"), TEXT("Downforce"),
        TEXT("Shots fired while airborne land significantly harder."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.ReadTheRoom"));
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 11.0f, EBreakerBuildCondition::Airborne); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Downforce.GetTag());
    Tree->Nodes.Add(Node);

    // Swift.Kinetic.Grind stood here and is DELETED, not retired-in-place
    // (Part One-U item 20, closing O144's deferral): its whole fantasy was
    // the wall and the wall's verb is gone. The save story that deferral
    // owed landed FIRST, in the same push — a loaded rank on an id that no
    // longer resolves is dropped and credited at the fallback cost
    // (DropUnknownRanksAndCredit), so the owner's saves that bought Grind
    // get their points back instead of paying a silent permanent tax.

    // --- Tier 4 (K9-K11), the rewrite tier -----------------------------------
    // Kinetic's rewrites all move the same idea: the branch's airborne
    // privileges start applying on the ground, and the branch's survival tool
    // gets more expensive and more absolute. See the block comment above this
    // function for why all three are tags with no stat line.

    // K9. "Changes WHEN an existing stat applies, not its magnitude" is the
    // design's own description, and it is precisely what no node effect can
    // say: EBreakerNodeStatTarget has no incoming-damage-reduction entry at
    // all (the node layer has never been able to author defence beyond flat
    // Health), and the affix whose value it re-sites — Damage Reduction While
    // Airborne — lives in the item layer, which a node may read but never
    // duplicate (Class-Kits §6.4). WAITING ON: the damage-taken path learning
    // to ask for this tag before it applies the airborne-only reduction.
    Node = MakeNode(TEXT("Swift.Kinetic.MomentumShield"), TEXT("Momentum Shield"),
        TEXT("At Redline, your Damage Reduction While Airborne applies with both feet on the ground."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.AirWork"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MomentumShield.GetTag());
    Tree->Nodes.Add(Node);

    // Hard Stop remains a standalone token unlock. O258 keeps the six-point
    // tier gate and redirects this rewrite through the live Redirect node.
    Node = MakeNode(TEXT("Swift.Kinetic.SpendToLive"), TEXT("Spend to Live"),
        TEXT("Hard Stop's window becomes true immunity, and it costs twice the Momentum."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.Redirect"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SpendToLive.GetTag());
    Tree->Nodes.Add(Node);

    // K11. Pure Momentum-loop rewrite with a real downside, the Kinetic twin
    // of Frenzy's No Safety: airborne decay is removed outright and grounded
    // decay is increased.
    // LIVE 2026-08-16: the WAITING ON is answered by the loop valve — the
    // ClassResourceDecay lane composes these lines and
    // UBreakerProgressionComponent::PushLoopValveOverrides delivers the
    // multiplier through the Momentum component's PushLoopOverride seam.
    // Class-Kits §1.4 K11's number is transcribed: "grounded decay increases
    // by 50%" (+50 while Grounded). The airborne half is authored as -100
    // while Airborne — the base loop already never decays airborne, so the
    // line is belt-and-braces that survives a future loop retune rather than
    // a new behaviour. K11's OTHER clause, "or within 0.5s of leaving the
    // ground", stays waiting: no coyote-time condition exists and the decay
    // grace timer is not it.
    Node = MakeNode(TEXT("Swift.Kinetic.NoGround"), TEXT("No Ground"),
        TEXT("Momentum stops decaying the moment your feet leave the floor, and decays faster while they are on it."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.ReadTheRoom"));
    AddEffect(Node, EBreakerNodeStatTarget::ClassResourceDecay, EBreakerNodeStatBucket::IncreasedPercent, 50.0f, EBreakerBuildCondition::Grounded); // Class-Kits §1.4 K11: grounded decay +50%
    AddEffect(Node, EBreakerNodeStatTarget::ClassResourceDecay, EBreakerNodeStatBucket::IncreasedPercent, -100.0f, EBreakerBuildCondition::Airborne); // Class-Kits §1.4 K11: no airborne decay
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_NoGround.GetTag());
    Tree->Nodes.Add(Node);

    // THE MORE IS GONE (O95): a doctrine authors none, every slot lives in Core.
    // What replaces it is a CONDITION CHANGE on the loop the doctrine is built
    // around -- momentum stops decaying while sliding -- which is the shape O95
    // names, and it pays through ClassResourceDecay: composed at
    // BreakerProgressionComponent.cpp:1109, bridged to the resource component at
    // :1356, consumed by BreakerMomentumComponent.cpp:640. Sliding is evaluated.
    // The keystone's rewrite half was always the live part: it grants
    // Keystone.Swift.TerminalVelocity and Overdrive resolves that row.
    Node = MakeNode(TEXT("Swift.Kinetic.Overpressure"), TEXT("Overpressure"),
        TEXT("Branch keystone. Momentum stops decaying entirely while you are sliding."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Kinetic.Carry"));
    AddEffect(Node, EBreakerNodeStatTarget::ClassResourceDecay, EBreakerNodeStatBucket::IncreasedPercent, -100.0f, EBreakerBuildCondition::Sliding); // O2 PLACEHOLDER
    // O37: every branch keystone is a cornerstone, so commitment gates all
    // three of Swift's branches identically (Bloodrhythm alone was flagged
    // first because its unset bit was a recorded content gap).
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Overpressure.GetTag());
    // REACHABILITY (O40c). Overdrive's `Keystone.Swift.TerminalVelocity`
    // variant row existed and NO node granted the tag, so the Kinetic ultimate
    // rewrite was unreachable by construction — the same failure class as the
    // third jump, and recorded as an open fork in Class-Kits §6.1.1 consequence
    // 1: "either the shipped keystones adopt those tags or the rewrites are
    // re-sited onto them". Adopting is the half that changes NO authored value:
    // Overpressure keeps its 1.20x-while-sliding More exactly as authored and
    // additionally resolves Overdrive to its Kinetic row. Re-siting would mean
    // authoring a new condition and magnitude, which O2 forbids. The ledger
    // fork itself is still the owner's to close.
    Node->GrantedTags.AddTag(BreakerAbilityTags::Keystone_Swift_TerminalVelocity.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetSwiftMarksmanTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Swift.Marksman"), TEXT("Swift — Marksman"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift);

    // Marksman's entry node. Its crit-damage and damage lines are live; the
    // DISTANCE half of its promise - that distant weak-point hits also generate
    // Momentum - is the part that ties the branch to the resource, and it is the
    // half that keeps Marksman from being a pure stat branch (the duplication
    // Core-Tree-Redesign flags: these two lines restate Precision's).
    // WAITING ON: a range-gated weak-point generation credit.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Swift.Marksman.LongLens"), TEXT("Long Lens"),
        TEXT("Distant weak-point hits generate Momentum, and down sights everything lands harder."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    // The crit line stays unconditional: CriticalDamage is not a generic damage
    // pool, so it is the second half of the rule -- a stat target Core's wheels
    // do not own outright -- rather than the conditional half.
    AddEffect(Node, EBreakerNodeStatTarget::CriticalDamage, EBreakerNodeStatBucket::Flat, 18.0f); // O2 PLACEHOLDER
    // AIMING, and it used to be unconditional. The node is called Long Lens and
    // its own text has always said "distant weak-point hits"; the shared Damage
    // line was the one part of it that paid from the hip.
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 3.0f, EBreakerBuildCondition::Aiming); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_LongLens.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16: §1.5 M2's spread rule is CONSUMED by the weapon's
    // spread path — FBreakerWeaponMath::SteadyMovementSpreadDegrees, applied
    // identically to the fired cone (FireOnce) and the predicted one
    // (GetNextShotSpreadDegrees / GetMovementSpreadDegrees). R1 is the
    // grounded rule, R2 extends it airborne, exactly the doc's rank split.
    // A rule rewrite with no percentage, so still no stat line — correctly.
    Node = MakeNode(TEXT("Swift.Marksman.Steady"), TEXT("Steady"),
        TEXT("Aiming while moving no longer widens spread. A second rank keeps the sights steady in the air."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Steady.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16: §1.5 M3 transcribed — "refunded at 25% (R2: 50%) if
    // the ability's effect lands a hit within its window". Lead is the
    // Marksman ability that exists; UBreakerWeaponComponent::FireOnce refunds
    // once per mark window when a shot connects with the marked target, at
    // the registry's own authored cost (§1.2 S6: 40 Momentum).
    Node = MakeNode(TEXT("Swift.Marksman.Ledger"), TEXT("Ledger"),
        TEXT("Momentum spent on Marksman abilities is partly refunded when they connect."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Ledger.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16 (owner ruling: Swift = multishot/pierce/chain/ricochet).
    // The weapon layer now has a real ricochet: a shot that hits geometry
    // bounces toward the nearest enemy in line of sight. Class-Kits §1.5 M4
    // authors Angle as a rewrite of the Ricochet Chance AFFIX's geometric
    // reflection — but no such affix exists in the item layer yet, so with
    // nothing to rewrite, this node is also the count's source: +1 bounce per
    // rank (AUTHORED, O2 PLACEHOLDER, the §1.3.1 stat-half pattern). The two
    // seek radii ARE transcribed doc values — 12 m at R1, 20 m at R2 — read
    // per-rank by UBreakerWeaponComponent::ResolvePelletImpacts.
    Node = MakeNode(TEXT("Swift.Marksman.Angle"), TEXT("Angle"),
        TEXT("Shots that hit the world bounce toward the nearest enemy in sight. A second rank bounces again, and seeks further."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.LongLens"));
    AddEffect(Node, EBreakerNodeStatTarget::RicochetCount, EBreakerNodeStatBucket::Flat, 1.0f); // O2 PLACEHOLDER (authored count; radii are §1.5 M4's 12 m / 20 m)
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Angle.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16 (partially): §1.5 M5 transcribed — the mark persists
    // through the target's death and jumps to the nearest enemy within 15 m
    // (R2: 25 m), proc coefficient 0 on the jump (it moves the mark and
    // nothing else). Consumed by UBreakerWeaponComponent::FireOnce off the
    // shared mark surface (UBreakerAbilityStateComponent::SetMark), which is
    // why the weapon can re-site the mark without owning it. PARTIAL because
    // the weapon is the only killer it can see: a marked target dying to a
    // DoT or an ally does not jump the mark until the spec's
    // UBreakerMarkComponent owns marks and deaths in one place.
    Node = MakeNode(TEXT("Swift.Marksman.MarkEconomy"), TEXT("Mark Economy"),
        TEXT("Lead's mark survives its target's death and jumps to a nearby enemy."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.Ledger"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MarkEconomy.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16: pierce exists on the weapon path, and BOTH halves of
    // §1.5 M6 now pay. The rule half — "+4 Momentum per target pierced (R2:
    // +7), capped at 3" — is consumed by UBreakerWeaponComponent::FireOnce
    // reading this node's rank (values transcribed there). The Pierce line
    // below is the AUTHORED half that gives the discipline something to
    // discipline: +1 penetration per rank, the enum's own naming of this node
    // ("Pierce... Named by Swift.Marksman.PierceDiscipline") made real. The
    // pre-existing crit/damage stat half is kept unchanged — removing an
    // authored value would be a retune under the O2 freeze.
    Node = MakeNode(TEXT("Swift.Marksman.PierceDiscipline"), TEXT("Pierce Discipline"),
        TEXT("Shots punch through one more enemy per rank, and every target pierced pays Momentum, up to three."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.Steady"));
    AddEffect(Node, EBreakerNodeStatTarget::Pierce, EBreakerNodeStatBucket::Flat, 1.0f); // O2 PLACEHOLDER (authored count)
    // Pierce and CriticalChance stay unconditional -- neither is a generic
    // damage pool, and pierce is Marksman's alone.
    AddEffect(Node, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 6.0f); // O2 PLACEHOLDER
    // AIMING, and it used to be unconditional. Punching through a line of
    // targets is an aimed shot; the shared Damage line paid whether or not the
    // player was doing the thing the node is about.
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 3.0f, EBreakerBuildCondition::Aiming); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_PierceDiscipline.GetTag());
    Tree->Nodes.Add(Node);

    // The pierce-ignores-armour rule is CONSUMED by
    // UBreakerWeaponComponent::ResolvePelletImpacts, which reads this tag and
    // grants full armour penetration to the second and subsequent targets of
    // a pierced shot (§1.5 M7's rule half, verbatim).
    //
    // THE +2 PIERCE STAND-IN IS RETIRED (O140). It was authored so an armour
    // rule never rode zero penetrations while the S5 Sightline ABILITY did
    // not exist, with its own retirement scheduled "the day the grant goes
    // in" — the ability landed (O175, Swift.Sightline: the next shot pierces
    // through its own channel bonus), so keeping the always-on +2 here would
    // double-pay the same fantasy and quietly out-value the unlockable. The
    // node is the RULE half alone now, which the ability's pierce feeds.
    Node = MakeNode(TEXT("Swift.Marksman.Sightline"), TEXT("Sightline"),
        TEXT("Pierced targets after the first take full damage through Armour."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Marksman.PierceDiscipline"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Sightline.GetTag());
    Tree->Nodes.Add(Node);

    // THE ABILITY GRANT IS RETIRED (O140). This node granted Swift.Lead from
    // the era when Lead was a starter and the grant was redundant decoration;
    // ruling 1 made Lead a quartermaster UNLOCKABLE, which turned the grant
    // into a free route around the token — two doctrine points buying what
    // the token economy prices, from a node whose real payload is the
    // two-target rule. The node keeps that rule (the tag's consumer holds
    // marks on two targets); a save that bought this node before the
    // retirement keeps its rank and its rule and buys Lead with a token like
    // every other Swift. This was also the LAST GrantedAbilityIds writer in
    // the project — the path's readers-with-no-writers question is already in
    // DECISIONS' Open list and is the owner's.
    Node = MakeNode(TEXT("Swift.Marksman.Lead"), TEXT("Lead"),
        TEXT("Lead may be held on two targets at once."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Marksman.MarkEconomy"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Lead.GetTag());
    Tree->Nodes.Add(Node);

    // Marksman's crit-chance ladder. Long Lens gave the branch crit DAMAGE with
    // almost no chance to apply it to, which is a stat that reads well on a card
    // and does close to nothing.
    Node = MakeNode(TEXT("Swift.Marksman.Deadeye"), TEXT("Deadeye"),
        TEXT("Held aim finds the weak point. A large flat critical chance per rank."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Marksman.LongLens"));
    AddEffect(Node, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 4.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Deadeye.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (M9-M11), the rewrite tier -----------------------------------
    // Marksman is the branch that makes Momentum BANKABLE, and §1.5 is explicit
    // that it "pays for that privilege with a Tier-4 node rather than getting
    // it free" — so of the three branches, Marksman is the one whose identity
    // was most damaged by the tier being dropped. Reserve is the node the
    // branch description points at.

    // M9. The stationary-Swift unlock, and the deliberate half-measure — the
    // bar HOLDS while ADS but still does not GENERATE — held exactly.
    // LIVE 2026-08-16: the WAITING ON is answered by the loop valve. Class-
    // Kits §1.5 M9, "Momentum does not decay while ADS", is authored as a
    // -100% ClassResourceDecay line conditioned on Aiming (the O30 posture
    // predicate added by name for this node), composed to a decay multiplier
    // of exactly 0 while ADS and delivered through the Momentum component's
    // PushLoopOverride seam. Generation while ADS is untouched — the lane
    // scales DECAY only, so "holds a bar, does not fill one" is structural.
    Node = MakeNode(TEXT("Swift.Marksman.Reserve"), TEXT("Reserve"),
        TEXT("Momentum stops decaying while you are aiming down sights. It still does not build there — this holds a bar, it does not fill one."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Marksman.Steady"));
    AddEffect(Node, EBreakerNodeStatTarget::ClassResourceDecay, EBreakerNodeStatBucket::IncreasedPercent, -100.0f, EBreakerBuildCondition::Aiming); // Class-Kits §1.5 M9: no decay while ADS
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Reserve.GetTag());
    Tree->Nodes.Add(Node);

    // M10. A projectile-behaviour rewrite: the shot keeps its FULL remaining
    // damage through a kill instead of taking the Pierce falloff. The falloff
    // is a weapon-layer curve, so "do not apply it" has no node-stat form —
    // and expressing it as Increased Damage would be the affix-layer
    // duplication §6.4 forbids, not to mention unconditional where the design
    // is bounded by the Pierce cap.
    // LIVE 2026-08-16: the WAITING ON is answered — pierce falloff exists
    // (UBreakerWeaponComponent::PierceDamageFalloff) and
    // FBreakerWeaponMath::NextPierceMultiplier skips the falloff step after a
    // killing hit exactly when this tag is owned. Still a tag with no stat
    // line, correctly: the node changes a rule, not a number.
    Node = MakeNode(TEXT("Swift.Marksman.Overpenetration"), TEXT("Overpenetration"),
        TEXT("A shot that kills carries on at full damage instead of falling off, up to the pierce cap."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Marksman.PierceDiscipline"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Overpenetration.GetTag());
    Tree->Nodes.Add(Node);

    // M11. The one tier-4 node in the class whose CONDITION is expressible —
    // Redline is a real EBreakerBuildCondition — and it still cannot be
    // authored as an effect, because what the condition gates is a RANGE GATE
    // ON AN ABILITY (Lead's 25 m drops to 10 m), not a magnitude. The
    // prerequisite is Lead itself, since a node that rewrites Lead's rule
    // with no Lead is a purchase that does nothing.
    // LIVE 2026-08-16: the rule is CONSUMED by UBreakerWeaponComponent::
    // FireOnce via FBreakerWeaponMath::LeadRangeGateCm — with this tag owned
    // and the bar at Redline, the gate the weak-point treatment tests against
    // is 10 m instead of Lead's authored 25 m. Still, correctly, a tag with
    // no stat line: the node changes a rule, not a number.
    Node = MakeNode(TEXT("Swift.Marksman.CalledShot"), TEXT("Called Shot"),
        TEXT("At Redline, Lead's range gate drops from 25 m to 10 m, so the mark pays at conversational distance."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Marksman.Lead"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MarksmanCalledShot.GetTag());
    Tree->Nodes.Add(Node);

    // Marksman's branch keystone, and the node that took two rulings to settle.
    //
    // It began as the only UNCONDITIONAL More outside Core, deliberately: the
    // pick for a build that refuses to organise itself around a movement state.
    // O95 removed every doctrine More, and the replacement written then was an
    // unconditional +18% WeaponDamage — same reasoning, same shape, one bucket
    // down. That was wrong, and the measurement said so before anyone ruled:
    // the at-cap band moved 6.53x -> 6.79x and weapon/ability parity 0.647 ->
    // 0.622 on that single edit. A doctrine node moving two Core-level metrics
    // is a doctrine doing Core's job.
    //
    // THE RULING (owner, and it is the general form): a doctrine may not author
    // a magnitude on a generic damage pool. Where a doctrine's rule carries a
    // magnitude it is conditional on THE DOCTRINE'S OWN AXIS, or it sits on a
    // stat target no Core wheel authors. Generic Increased Damage is Core's, and
    // a doctrine spending any of eight points on it has spent its identity on
    // what Core does with 222.
    //
    // AIMING IS MARKSMAN'S AXIS, and it is the honest condition here rather than
    // a movement state the node was written to avoid: aimed fire IS what this
    // doctrine is about, so the line pays exactly when the build is doing the
    // thing it is built for. It is live — BreakerBuildConditions.cpp:250 sets it
    // from the weapon's own ADS state, not from a flag nothing writes — and
    // Swift.Marksman.Reserve already authors against it, so this is the tree's
    // existing vocabulary rather than a new one. Core authors nothing on Aiming.
    //
    // The lane stays WeaponDamage rather than the shared Damage pool, which is
    // the second half of the rule and the reason Progression.AxisOverlap reads
    // buckets and not just conditions.
    //
    // TARGET RIDER (Stage 6): the node is NAMED Culling and until TargetLowHealth
    // existed nothing about it culled. Increased-bucket and additive per the §4a
    // rider canon, because a target-conditional More is forbidden by rule.
    Node = MakeNode(TEXT("Swift.Marksman.Culling"), TEXT("Culling"),
        TEXT("Branch keystone. Aiming down sights, your weapon hits considerably harder — and the cull itself: targets already near death take increased damage."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 15.0f, EBreakerBuildCondition::TargetLowHealth); // O2 PLACEHOLDER — execute window is the narrowest target condition, priced at the top of the conditional band
    AddPrerequisite(Node, TEXT("Swift.Marksman.PierceDiscipline"));
    AddEffect(Node, EBreakerNodeStatTarget::WeaponDamage, EBreakerNodeStatBucket::IncreasedPercent, 18.0f, EBreakerBuildCondition::Aiming); // O2 PLACEHOLDER
    Node->bCornerstone = true; // O37: keystone tier requires branch commitment
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Culling.GetTag());
    // REACHABILITY (O40c) — the Marksman half of the same fork recorded in
    // Class-Kits §6.1.1. Standing Wave is "the stationary Swift ultimate", and
    // Culling is the branch keystone authored for the build that refuses to
    // organise around a movement state, so the adoption is not merely
    // mechanical: the two say the same thing about the branch.
    Node->GrantedTags.AddTag(BreakerAbilityTags::Keystone_Swift_StandingWave.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

// Class-Kits §1.3. Frenzy is "trigger discipline and cadence — the branch that
// wants a full magazine and a target that stays in front of it", and the LEAST
// mobile Swift branch on purpose. That gives the three branches three distinct
// build conditions and makes the branch strip a real choice under O27:
// Kinetic owns Airborne / WallRiding / Sliding, Marksman owns unconditional
// output, and Frenzy owns REDLINE — the state you have to work to hold and can
// hold with both feet on the ground.
UBreakerProgressionTree* UBreakerProgressionLibrary::GetSwiftFrenzyTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Swift.Frenzy"), TEXT("Swift — Frenzy"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift);

    // Every Frenzy node in the design document is a Momentum-LOOP rewrite, and
    // the Momentum loop is not a node stat target — so transcribed verbatim the
    // whole branch would have been ten tags and nothing else. Each node
    // therefore carries its design-document rule as a tag AND a stat line that
    // states the same intent in a currency that reaches gameplay today. The
    // stat lines are authored, not transcribed; the doc's implementation-status
    // section names each one.

    // --- Tier 1 ------------------------------------------------------------
    // LIVE 2026-08-16: §1.3 F1's rule half, both clauses transcribed, is
    // CONSUMED by UBreakerMomentumComponent::HandleShot — grounded weak-point
    // hits pay Momentum with any rank owned, and R2 runs the internal
    // cooldown at 0.15s instead of 0.25s. The crit line below stays: it is
    // the authored stat half, not a stand-in for the rule.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Swift.Frenzy.TriggerDiscipline"), TEXT("Trigger Discipline"),
        TEXT("Weak-point hits pay Momentum with your feet on the ground, and you find weak points more often."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 3.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FrenzyTrigger.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16: §1.3 F2's rule half, transcribed — "reloading while at
    // Redline refunds ammunition to the magazine equal to the shots fired in
    // the previous 2s (R1: half, R2: all). Rule rewrite; does not touch
    // reload speed." Consumed by UBreakerWeaponComponent::StartReload/
    // FinishReload: the Redline read and the 2s window are captured when the
    // reload is committed, and the free rounds settle ahead of the reserve
    // draw, so the refund is paid in reserve saved.
    Node = MakeNode(TEXT("Swift.Frenzy.Loaded"), TEXT("Loaded"),
        TEXT("Reloading at Redline returns the rounds you just spent, and a loaded magazine hits harder at Redline."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 6.0f, EBreakerBuildCondition::Redline); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Loaded.GetTag());
    Tree->Nodes.Add(Node);

    // Short Leash delays decay below the walking threshold, so raising the
    // threshold speed itself is the same node said in a stat: a faster Frenzy
    // spends less of its time under the line it is being paid to stay above.
    Node = MakeNode(TEXT("Swift.Frenzy.ShortLeash"), TEXT("Short Leash"),
        TEXT("Momentum decay waits longer when you slow down, and you move quicker on the ground."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 1, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::MoveSpeed, EBreakerNodeStatBucket::IncreasedPercent, 5.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_ShortLeash.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 ------------------------------------------------------------
    // LIVE 2026-08-16: §1.3 F4's rule half, transcribed — every 5th
    // consecutive hit on any target generates +8 Momentum outside the global
    // per-second cap (R2: every 4th; missing resets). Consumed by
    // UBreakerMomentumComponent::HandleShot off the weapon's OnShot event;
    // "outside the cap" is GrantMomentum's direct-credit path.
    Node = MakeNode(TEXT("Swift.Frenzy.Rhythm"), TEXT("Rhythm"),
        TEXT("Every fifth consecutive hit pays Momentum outside the cap, and a maintained rhythm finds weak points."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.TriggerDiscipline"));
    AddEffect(Node, EBreakerNodeStatTarget::CriticalChance, EBreakerNodeStatBucket::Flat, 3.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Rhythm.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16 (partially): §1.3 F5's first clause, transcribed —
    // firing the last round in a magazine generates +12 Momentum. Consumed by
    // UBreakerMomentumComponent::HandleMagazineEmptied off the weapon's
    // OnMagazineEmptied event (which fires on the last round LEAVING, never
    // on the reload). Rank two also refunds one second of active ability cooldown.
    Node = MakeNode(TEXT("Swift.Frenzy.DryFire"), TEXT("Dry Fire"),
        TEXT("Firing the last round of a magazine pays Momentum. Emptying rather than tapping is rewarded at Redline. R2: also refunds 1s of active ability cooldown."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.Loaded"));
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 5.0f, EBreakerBuildCondition::Redline); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_DryFire.GetTag());
    Tree->Nodes.Add(Node);

    // Frenzy is the branch that stands its ground while the other two leave it,
    // so its loop node pays in the currency standing still actually costs.
    // LIVE 2026-08-16: §1.3 F6's rule half, transcribed — kills refund
    // Momentum equal to 10% of the ability cost most recently paid (R2: 20%).
    // Consumed by UBreakerMomentumComponent::HandleKillDealt off the combat
    // component's attacker-side OnKillDealt; "the cost most recently paid" is
    // the loop's own spend observer (every write the component makes goes
    // through ApplyMomentumDelta, so an external drop in the class resource
    // is an ability cost by elimination).
    Node = MakeNode(TEXT("Swift.Frenzy.Feed"), TEXT("Feed"),
        TEXT("Kills refund part of the Momentum you last spent, and holding the line leaves you with more to lose."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.ShortLeash"));
    AddEffect(Node, EBreakerNodeStatTarget::Health, EBreakerNodeStatBucket::Flat, 45.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Feed.GetTag());
    Tree->Nodes.Add(Node);

    // Frenzy's offensive spine and the largest conditional ladder in the
    // branch, the counterpart to Kinetic's Downforce and Grind. Redline is
    // harder to hold than airborne is to enter but easier than a wall ride, and
    // it is priced between them.
    Node = MakeNode(TEXT("Swift.Frenzy.Overrev"), TEXT("Overrev"),
        TEXT("Shots fired at Redline Momentum land significantly harder. Worth nothing the moment the bar drops."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.ShortLeash"));
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 12.0f, EBreakerBuildCondition::Redline); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Overrev.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 ------------------------------------------------------------
    // F7 grants S2 Cadence Break in Class-Kits §1.3. NO ability grant is
    // authored: `Swift.CadenceBreak` does not exist in the ability fallback
    // registry, and a node that unlocks a loadout entry resolving to nothing is
    // the "node that lies" failure mode. The rule half ships as a tag; the
    // grant goes in the same line the day the ability does.
    Node = MakeNode(TEXT("Swift.Frenzy.SlipcutMastery"), TEXT("Slipcut Mastery"),
        TEXT("Slipcut's window widens for each ability cooldown running. Its cadence window bites deeper."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.Rhythm"));
    AddEffect(Node, EBreakerNodeStatTarget::CriticalDamage, EBreakerNodeStatBucket::Flat, 20.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SlipcutMastery.GetTag());
    Tree->Nodes.Add(Node);

    // Frenzy's one unconditional damage increase, deliberately sited at the top
    // of the branch so the cadence nodes below it stay conditional. Live: the
    // damage line pays. The ammo-return-also-pays-Momentum half is what closes
    // the branch's kill-to-fire-again loop.
    // WAITING ON: the on-kill ammunition return crediting Momentum.
    Node = MakeNode(TEXT("Swift.Frenzy.AmmunitionEconomy"), TEXT("Ammunition Economy"),
        TEXT("Ammunition returned on a kill also pays Momentum, and at Redline your shots land harder."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.DryFire"));
    // AT REDLINE, and it used to be unconditional -- the description even said
    // so: "the branch's one unconditional increase to damage". A doctrine may
    // not author a magnitude on a generic damage pool without a condition on
    // its own axis (Progression.AxisOverlap), and Frenzy's axis is Redline,
    // which Loaded, Dry Fire and Overrev already author against.
    //
    // NOT RecentlyKilled, which is what the node's own fantasy suggests and
    // would have been the wrong answer: the whole Recently* family has no
    // recorder, and BreakerBuildConditions.cpp:135 returns false for every one
    // of them unconditionally. A line gated on it would pay nothing forever --
    // silent content wearing a condition, which is worse than the unconditional
    // line it replaced because it would look fixed.
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 5.0f, EBreakerBuildCondition::Redline); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AmmunitionEconomy.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (F9-F11), the rewrite tier -----------------------------------
    // Frenzy's rewrites are the branch saying its identity out loud: hold the
    // rhythm through a target swap, convert Redline into cadence, and pay for
    // cheap abilities with a bar that empties twice as fast. See the block
    // comment above GetSwiftKineticTree for why all three are tags only.

    // F9. A rewrite of Cadence Break's stacking rule. `Swift.CadenceBreak` is
    // STILL not in the ability fallback registry — the same gap Slipcut
    // Mastery records above — so this node rewrites an ability that does not
    // exist yet, and its prerequisite is the node that will grant it the day
    // it does. Deliberately authored anyway: the design's Frenzy is unreadable
    // without its ability rewrite, and a tag waiting on a named consumer is
    // the project's established way to say so. NO ability grant is added here
    // for the same reason Slipcut Mastery carries none.
    Node = MakeNode(TEXT("Swift.Frenzy.SecondWind"), TEXT("Second Wind"),
        TEXT("Cadence Break's stack no longer breaks when you change targets. Only a full second without a hit resets it."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.SlipcutMastery"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SecondWind.GetTag());
    Tree->Nodes.Add(Node);

    // F10. Reads an affix and changes its RULE — Damage Ramp's stacks accrue
    // as though the weapon were a cadence tier faster — which §6.4 lists as
    // compliant precisely because it adds no percentage. It is therefore
    // un-authorable as an effect twice over: there is no stat target for a
    // ramp's accrual rate, and inventing an Increased Damage line in its place
    // would be the affix duplication that same section forbids. The weapon's
    // Damage Ramp consumer doubles accrual once per successful shot at Redline.
    Node = MakeNode(TEXT("Swift.Frenzy.RedlineTrigger"), TEXT("Redline Trigger"),
        TEXT("At Redline your weapon is treated as a cadence tier faster for Damage Ramp, so its stacks build twice as quickly."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.Overrev"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_RedlineTrigger.GetTag());
    Tree->Nodes.Add(Node);

    // F11. §1.3 calls this "the node that makes Frenzy read as a CLASS choice
    // rather than a bonus", and both of its halves are now authorable.
    // LIVE 2026-08-16: both halves at once, exactly as the old WAITING ON
    // demanded — shipping either alone would invert the design (a pure
    // upgrade, or a pure tax). Class-Kits §1.3 F11 transcribed: "Momentum
    // decay is doubled" (+100% ClassResourceDecay, unconditional, through the
    // loop valve) "and abilities cost 40% less Momentum" (+40 on the
    // AbilityCost lane, which is authored as an Increased percentage OF THE
    // REDUCTION — the enum's own convention — and joins gear's Resource
    // Efficiency bucket additively).
    Node = MakeNode(TEXT("Swift.Frenzy.NoSafety"), TEXT("No Safety"),
        TEXT("Abilities cost far less Momentum, and the bar drains twice as fast. A real downside, taken on purpose."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.ShortLeash"));
    AddEffect(Node, EBreakerNodeStatTarget::ClassResourceDecay, EBreakerNodeStatBucket::IncreasedPercent, 100.0f); // Class-Kits §1.3 F11: decay doubled
    AddEffect(Node, EBreakerNodeStatTarget::AbilityCost, EBreakerNodeStatBucket::IncreasedPercent, 40.0f); // Class-Kits §1.3 F11: abilities cost 40% less
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_NoSafety.GetTag());
    Tree->Nodes.Add(Node);

    // The branch keystone O3 permits, and the only one of Swift's three that
    // ALSO rewrites the ultimate: `Keystone.Swift.Bloodrhythm` is already a row
    // in Overdrive's variant table, and the progression component now publishes
    // node tags onto the ability system component, so owning this node really
    // does resolve Overdrive to its Bloodrhythm row. Class-Kits §1.3 prices the
    // keystone at 4; the implemented branch grammar uses 3 (Overpressure,
    // Culling) and the cost curve is kept internally consistent instead.
    Node = MakeNode(TEXT("Swift.Frenzy.Bloodrhythm"), TEXT("Bloodrhythm"),
        TEXT("Branch keystone. Your rate of fire climbs at Redline, and Overdrive refunds Momentum on every hit."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Swift, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Swift.Frenzy.Overrev"));
    // THE MORE IS GONE (O95). Frenzy's identity is Redline uptime and sustained
    // fire, so the replacement is FireRate rather than damage: it composes into
    // the FireRateMultiplier attribute and UBreakerWeaponComponent's
    // GetEffectiveRoundsPerMinute reads it, so the doctrine changes the WEAPON'S
    // BEHAVIOUR rather than a number behind it. Deliberately NOT AbilityCost,
    // which would have been the obvious economy rule: that lane composes but
    // only Caster abilities read it at cast -- the base ApplyCost pays the raw
    // definition cost -- so a Swift node authored against it would pay nothing.
    // Redline is evaluated. The rewrite half, Overdrive's Bloodrhythm row and
    // its per-hit Momentum refund, was always the live part.
    AddEffect(Node, EBreakerNodeStatTarget::FireRate, EBreakerNodeStatBucket::IncreasedPercent, 20.0f, EBreakerBuildCondition::Redline); // O2 PLACEHOLDER
    // RECORDED GAP FIXED (audit item 9): bCornerstone was never set here, so
    // UI/BreakerMenu.cpp's ClassifyNode fell through to "single-rank costing
    // 3+" and drew Swift's first working keystone as an ordinary Convergence
    // square — the UI worked around it by ALSO reading the Keystone.* tag
    // below, but the authoring flag itself, and everything that reads it
    // directly (O37's commitment gate in CanPurchaseNode included), stayed
    // wrong until now. The read was fixed; this is the data.
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_Bloodrhythm.GetTag());
    Node->GrantedTags.AddTag(BreakerAbilityTags::Keystone_Swift_Bloodrhythm.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

// ---------------------------------------------------------------------------
// CASTER. Class-Kits §2 names three branches — Spellblade, Void Whisperer,
// Multispell — and until now none of them existed: Caster played through its
// seven abilities (all granted as starters, per the O39 comment on
// GetFallbackClassDefinition below) and the class-agnostic Core tree only.
// This is the same content gap Frenzy was for Swift, and it is closed the
// same way: tiers 1-3 only, each branch's tier-4 rewrite trio dropped rather
// than authored, one keystone per branch, and every magnitude an O2
// PLACEHOLDER.
//
// UPDATED: "the slice cut every Swift branch already uses" was true when this
// was written and is not any more — Swift's nine Tier-4 rewrites (F9-F11,
// K9-K11, M9-M11) have since been authored, so SB9-SB11 / VW9-VW11 /
// MS9-MS11 are now the only branch content Class-Kits specifies and the
// library omits. Recorded here rather than silently left reading as parity.
//
// THE ENUM GAP THIS BRANCH SET EXPOSES, STATED ONCE RATHER THAN PER NODE.
// Swift's nodes are legible against EBreakerNodeStatTarget because Momentum
// converts into crit / damage / speed — combat and movement stats the enum
// already models. Caster's Mana loop does not: "generate Mana at the
// weak-point rate instead of the weapon-hit rate", "Overcast's negative
// floor extends", "zones refresh instead of stack", "the cycle advances on
// hit instead of on cast" are RESOURCE-ECONOMY and ABILITY-BEHAVIOR rewrites,
// and neither a resource-generation-rate target nor a maximum-resource target
// exists on EBreakerNodeStatTarget (MS3 Reservoir's "+15/+25 Maximum Mana" is
// the clearest single case — there is no analogue of gear's "Maximum
// Resource" affix on this enum at all). None of that is expressible, so
// EVERY non-keystone node below ships as its Class-Kits rule verbatim, as a
// GrantedTag, with NO stat effect — the same pattern the Swift.CadenceBreak
// comment established for one node, here for a whole class's worth of
// content. This is not a content shortfall to fix later; it is the honest
// reading of "a node may only author effects the enum can express."
//
// ABILITY GRANTS ARE NOT RE-AUTHORED HERE, DELIBERATELY. Class-Kits gates
// four of Caster's six abilities behind Tier-3 "Grants" nodes (SB7/VW7/MS7/
// MS8). GetFallbackClassDefinition(Caster) already lists all seven ability
// ids across StarterAbilityIds and UnlockableAbilityIds — the O39 fix for a
// null-class-definition
// bug that predates this file — and Tests/BreakerProgressionAuditTests.cpp's
// CasterAbilitiesUnlockTest equips every one of them with ZERO node
// purchases. Gating them behind these new tree nodes would un-equip that
// test's abilities on a fresh Caster and is out of scope for a branch-content
// pass; it is recorded as the divergence it is. Each Tier-3 node below still
// carries the REST of its design row — the rule-rewrite half distinct from
// the grant — as a tag.
UBreakerProgressionTree* UBreakerProgressionLibrary::GetCasterSpellbladeTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Caster.Spellblade"), TEXT("Caster — Spellblade"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster);

    // --- Tier 1 (SB1-SB3) ---------------------------------------------------
    UBreakerProgressionNode* Node = MakeNode(TEXT("Caster.Spellblade.ContactCharge"), TEXT("Contact Charge"),
        TEXT("Melee hits generate Mana at the weak-point rate instead of the weapon-hit rate."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_ContactCharge.GetTag());
    Tree->Nodes.Add(Node);

    // Spellblade's answer to Cleave's own Bleed jamming the branch's income:
    // the branch is built on melee uptime, and a status already present must
    // not stop paying for the swing that reapplied it.
    // WAITING ON: the Mana component's status-application generation skipping
    // its already-present suppression for this tag, and the kill refund.
    Node = MakeNode(TEXT("Caster.Spellblade.FollowThrough"), TEXT("Follow Through"),
        TEXT("Cleave's Bleed generates its status-application Mana even when Bleed is already present, and refunds Mana on kill."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_FollowThrough.GetTag());
    Tree->Nodes.Add(Node);

    // The node that decides where Spellblade stands. Doubling weapon income at
    // close range is the branch's whole positional argument — without it a
    // Spellblade is a Caster who happens to own a melee ability.
    // WAITING ON: the weapon-hit generation path reading a range band.
    Node = MakeNode(TEXT("Caster.Spellblade.Close"), TEXT("Close"),
        TEXT("Weapon hits at close range generate double Mana. Defines the branch's play distance."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Close.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (SB4-SB6) ---------------------------------------------------
    Node = MakeNode(TEXT("Caster.Spellblade.Debt"), TEXT("Debt"),
        TEXT("Overcast's negative Mana floor extends further below zero. More rope to Overcast on."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.ContactCharge"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Debt.GetTag());
    Tree->Nodes.Add(Node);

    // RESOLVED [O1] pattern: Closequarter's follow-up cancels the TARGET's
    // passive block/evade ROLL, not the player's own — re-expressed against
    // the passive chance layer exactly as Kinetic's Evade Conversion is,
    // except here it is the enemy's roll being suppressed, which has no
    // authorable stat on the caster's own sheet.
    Node = MakeNode(TEXT("Caster.Spellblade.MomentumTransfer"), TEXT("Momentum Transfer"),
        TEXT("Closequarter's arrival briefly suppresses the target's passive block and evade rolls on the next melee hit."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.FollowThrough"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_MomentumTransfer.GetTag());
    Tree->Nodes.Add(Node);

    // The branch's only sustain, and the reason Overcast is survivable here:
    // debt is paid in melee range, where the branch already wants to be.
    // WAITING ON: the Lifesteal stat target's aggregation lane, plus a melee
    // damage-dealt hook that can read the negative-Mana state.
    Node = MakeNode(TEXT("Caster.Spellblade.Bloodprice"), TEXT("Bloodprice"),
        TEXT("While Mana is negative, melee hits restore health equal to a portion of damage dealt."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.Close"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Bloodprice.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (SB7-SB8) ---------------------------------------------------
    // "Grants C2 Closequarter" is not authored (see the block comment above);
    // the no-target blink rewrite is the real content of this node.
    Node = MakeNode(TEXT("Caster.Spellblade.Blink"), TEXT("Blink"),
        TEXT("Closequarter may be cast with no target to blink in the aim direction."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.MomentumTransfer"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Blink.GetTag());
    Tree->Nodes.Add(Node);

    // LIVE 2026-08-16: the tag found its consumer. UBreakerAbility_Cleave::
    // ComputeEffectiveArcDegrees reads Node_SB_Edge off the progression
    // component and widens the swing to Class-Kits §2.3 SB8's 180 degrees
    // (the geometry then rides the AbilityArea lane's multiplier, clamped to
    // 360). Correctly still a tag with NO stat effect — SB8 is explicit that
    // this is a rule change with no damage percentage, and the Bleed-to-every-
    // target half is Cleave's base behaviour already (every swept target takes
    // the 100%-chance Bleed).
    Node = MakeNode(TEXT("Caster.Spellblade.Edge"), TEXT("Edge"),
        TEXT("Cleave's arc widens to a full sweep and its Bleed applies to every target hit."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.Bloodprice"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Edge.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Caster.Spellblade.Overreach"), TEXT("Overreach"),
        TEXT("While Mana is negative, all Caster abilities are free. Your Overcast incoming-damage penalty rises to 30%."),
        EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.Debt"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Overreach.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Caster.Spellblade.Reprisal"), TEXT("Reprisal"),
        TEXT("After a passive Block proc, your next Cleave within two seconds costs no Mana."),
        EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.Bloodprice"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Reprisal.GetTag());
    Tree->Nodes.Add(Node);
    Node = MakeNode(TEXT("Caster.Spellblade.NoDistance"), TEXT("No Distance"),
        TEXT("Closequarter refunds Mana on arrival at any target health. Its base Mana cost rises to 50."),
        EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.MomentumTransfer"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_NoDistance.GetTag());
    Tree->Nodes.Add(Node);
    // The branch keystone, and THE FIRST TAG RIDER IN THE CONTENT.
    //
    // This comment used to say there was "no way to key an effect to 'this hit
    // was melee' without inventing an enum value, which is out of scope here",
    // and concluded the slot was empty by rule. The first half stopped being
    // true when O98 landed: MeleeDamage is RIDER-DELIVERED, keyed on the native
    // Damage.Melee tag the request already carries, and
    // RiorsEdge.Combat.TargetRiders.SourceTagMelee has proved the slice for a
    // synthetic node ever since. No shipped node used it. A mechanism built,
    // tested, and authored against by nothing is the same shape as a lane with
    // no author -- and the paragraph asserting it could not be done is what
    // kept it that way, which is the second time in this pass a justification
    // outliving its cause has acted as a wall.
    //
    // WHAT IS AUTHORED IS AN INCREASED LINE, NOT A MORE. O95's prohibition is
    // untouched: the More slot stays empty by rule and the keystone earns its
    // place by rewriting the ultimate. The tag is the gate, which is what makes
    // an unconditional-looking line honest here -- it pays only on a hit that
    // says it IS melee, exactly the delivery tax Class-Kits SB12 prices, and
    // never on the generic pools Progression.AxisOverlap reserves for Core.
    Node = MakeNode(TEXT("Caster.Spellblade.Edgework"), TEXT("Edgework"),
        TEXT("Branch keystone. Your melee strikes land considerably harder. Rewrites Unmake: during it, Cleave has no animation lock and Closequarter loses its range limit."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Spellblade.Debt"));
    AddEffect(Node, EBreakerNodeStatTarget::MeleeDamage, EBreakerNodeStatBucket::IncreasedPercent, 25.0f); // O2 PLACEHOLDER -- melee-only is the tax, so it prices above the unconditional lanes
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_SB_Edgework.GetTag());
    Node->GrantedTags.AddTag(BreakerAbilityTags::Keystone_Caster_Edgework.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetCasterVoidWhispererTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Caster.VoidWhisperer"), TEXT("Caster — Void Whisperer"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster);

    // --- Tier 1 (VW1-VW3) ----------------------------------------------------
    UBreakerProgressionNode* Node = MakeNode(TEXT("Caster.VoidWhisperer.Seep"), TEXT("Seep"),
        TEXT("Status applications generate more Mana than the base rate."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_Seep.GetTag());
    Tree->Nodes.Add(Node);

    // Zone income deliberately INDEPENDENT of enemy count, so Void Whisperer
    // still pays in a one-enemy fight. Scaling it per-enemy would make the
    // branch's income a crowd-size stat and its worst case unplayable.
    // WAITING ON: the zone actor crediting Mana per second while occupied.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.StandingWater"), TEXT("Standing Water"),
        TEXT("Zones generate Mana per second while at least one enemy is inside, independent of enemy count."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_StandingWater.GetTag());
    Tree->Nodes.Add(Node);

    // The not-shooting income — the branch's argument for putting the gun down.
    // Adds recovery to the competitive starter rate after the live no-fire delay.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.Patience"), TEXT("Patience"),
        TEXT("Adds Mana regeneration while you have not fired a weapon for 4s (R2: 2s)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_Patience.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (VW4-VW6) ----------------------------------------------------
    // The AbilityDuration lane is the node's perceptible half: zones linger
    // longer per rank (O2 PLACEHOLDER, consumed by Rot's
    // ComputeEffectiveDurationSeconds on the spawn and refresh paths). O271:
    // a recast spawns a new puddle and nothing merges, so the only refresh a
    // zone still receives is Wellspring's following puddle renewed by a
    // recast; R2's one-time metre is claimed there
    // (RiorsEdge.Abilities.RotPurchasedZones pins it). QUESTION, RECORDED: whether R2
    // should instead grow a puddle cast over a live one is the owner's.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.Lingering"), TEXT("Lingering"),
        TEXT("Zones linger longer. R2: a refreshed zone grows by 1 m, once."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.StandingWater"));
    AddEffect(Node, EBreakerNodeStatTarget::AbilityDuration, EBreakerNodeStatBucket::IncreasedPercent, 15.0f); // O2 PLACEHOLDER
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_Lingering.GetTag());
    Tree->Nodes.Add(Node);

    // Keeps the branch's slow damage from also being its slow income: a DoT
    // kill pays, so committing to attrition is not a resource penalty.
    // WAITING ON: the kill path asking whether a Caster DoT was on the victim.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.Attrition"), TEXT("Attrition"),
        TEXT("Enemies killed while affected by a Caster damage-over-time effect refund Mana."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Seep"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_Attrition.GetTag());
    Tree->Nodes.Add(Node);

    // Makes Siphon castable DURING a fight rather than only after one. As
    // shipped the channel breaks on any damage, which means the branch's
    // sustain is only available when it is not needed.
    // WAITING ON: Siphon's channel-break check reading a damage threshold.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.Drain"), TEXT("Drain"),
        TEXT("Siphon's channel no longer breaks on damage below a fraction of the caster's max health."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Patience"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_Drain.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (VW7-VW8) -----------------------------------------------------
    // "Grants C3 Rot upgrade path / grants C4 Siphon" is not authored (see the
    // block comment above); Rot's flat armour-reduction rewrite is the real
    // content, and it stays flat rather than percentage to protect the boss
    // cap (Class-Kits VW7, Master 7.10.5).
    Node = MakeNode(TEXT("Caster.VoidWhisperer.Zonework"), TEXT("Zonework"),
        TEXT("Rot's Armour reduction gains an additional flat amount against targets already affected by a damage-over-time effect."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Attrition"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_Zonework.GetTag());
    Tree->Nodes.Add(Node);

    // The branch's only mobile play — every other Void Whisperer node rewards
    // standing still, and one node has to answer the encounter that will not
    // let you. One at a time, deliberately: two would be a moving safe zone.
    // WAITING ON: a zone actor that can attach to the caster, and a
    // one-at-a-time registry to enforce the limit.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.Wellspring"), TEXT("Wellspring"),
        TEXT("A zone may be placed on the caster's own position and move with them for its duration. One at a time."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Lingering"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_Wellspring.GetTag());
    Tree->Nodes.Add(Node);

    // The branch keystone. It carried Class-Kits VW12's canon text verbatim —
    // "damage over time is multiplied by 1.30", a DoT More specifically — and
    // the paragraph that used to sit here argued for exactly that, at length,
    // from the A4 ruling that built the lane for it. O95 supersedes it: a
    // doctrine authors no multiplier, so the argument goes with the number.
    // What that leaves behind is recorded at the lane it leaves behind.
    // Historical Class-Kits VW9. O2 PLACEHOLDER: +25 critical-chance points.
    // Physical applications retain the original critical sample and check own-zone geometry.
    // Rot retains the applying hit sample and replaces its critical factor at most once.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.SnapshotDiscipline"), TEXT("Snapshot Discipline"),
        TEXT("Your periodic statuses applied inside your own zone gain 25 percentage points Critical Chance using the applying sample. An already-critical hit is never multiplied twice."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Zonework"));
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.SnapshotDiscipline")));
    Tree->Nodes.Add(Node);

    // Historical Class-Kits VW10. O2 PLACEHOLDER: target below 25% health.
    // Terminal latches on low target health and spends finite remaining tick funding.
    // It does not reserve Core Long Dark's single-target lease.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.Terminal"), TEXT("Terminal"),
        TEXT("Your DoTs persist on targets below 25% health until death or cleanse. Ticks stop when their remaining funded damage is exhausted."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Attrition"));
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.Terminal")));
    Tree->Nodes.Add(Node);

    // Historical Class-Kits VW11/O10. O2 PLACEHOLDER: double tick frequency,
    // 25% incoming penalty instead of the historical 15% Overcast penalty.
    // Physical cadence snapshots at application; the existing incoming modifier is replaced.
    // Rot doubles its lifetime and redistributes the same finite damage budget.
    Node = MakeNode(TEXT("Caster.VoidWhisperer.LongDebt"), TEXT("Long Debt"),
        TEXT("While Mana is negative, Bleed and Poison snapshot double tick frequency; Rot lasts twice as long with the same total funded damage. You take 25% increased damage instead of 15%."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Drain"));
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.VoidWhisperer.LongDebt")));
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Caster.VoidWhisperer.LongDark"), TEXT("Long Dark"),
        TEXT("Branch keystone. Rewrites Unmake: duration extends to 12s at 50% cost instead of free, and zones placed during it stop aging until that Unmake ends. Their damage continues."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.VoidWhisperer.Attrition"));
    // THE MORE IS GONE (O95), and it was the largest of the four at x1.30 as
    // well as the only one that never went through AddDamageMore -- it targeted
    // the DamageOverTime pool directly, which is why a search for the helper
    // found three keystones rather than four.
    //
    // AbilityDuration replaces it, which is the doctrine's own axis rather than
    // a substitute for a multiplier: Void Whisperer is zones that keep working
    // after you have looked away, and the keystone's own rewrite already extends
    // Unmake. The lane composes and UBreakerAbility_Rot reads it through
    // ComputeEffectiveDurationSeconds on both spawn and refresh.
    AddEffect(Node, EBreakerNodeStatTarget::AbilityDuration, EBreakerNodeStatBucket::IncreasedPercent, 30.0f); // O2 PLACEHOLDER LIVE under A4 (owner ruling 2026-08-16)
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_VW_LongDark.GetTag());
    Node->GrantedTags.AddTag(BreakerAbilityTags::Keystone_Caster_LongDark.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetCasterMultispellTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Caster.Multispell"), TEXT("Caster — Multispell"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster);

    // --- Tier 1 (MS1-MS3) ----------------------------------------------------
    UBreakerProgressionNode* Node = MakeNode(TEXT("Caster.Multispell.Variance"), TEXT("Variance"),
        TEXT("Applying a status type the target does not already have generates a multiple of the base Mana rate. The core sequencing incentive stated as a resource rule."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Variance.GetTag());
    Tree->Nodes.Add(Node);

    // The rotation is the branch, so a missed cast must not cost a position in
    // it. Advancing on hit rather than on cast is what makes the fantasy
    // survivable at the skill floor O33 asks for.
    // WAITING ON: Fracture advancing its status cycle on hit, not on cast.
    Node = MakeNode(TEXT("Caster.Multispell.Cycle"), TEXT("Cycle"),
        TEXT("Fracture's status cycle advances on hit rather than on cast, so a missed cast does not waste a position."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Cycle.GetTag());
    Tree->Nodes.Add(Node);

    // "The one intentional stat node" per Class-Kits MS3, AND IT NOW AUTHORS
    // ONE. This comment used to say the node "genuinely cannot author even a
    // placeholder number, because EBreakerNodeStatTarget has no
    // Maximum-Resource counterpart to gear's Maximum Resource affix". That
    // stopped being true when MaxClassResource landed: the target exists, the
    // lane composes at BreakerProgressionComponent.cpp:1198, and every resource
    // component reads GetMaxClassResource. Nothing was blocking it but this
    // paragraph, which is why a justification outliving its cause is worse than
    // no justification -- it reads like a decision and acts like a wall.
    //
    // Reservoir was the only silent node in the game whose lane was already
    // finished end to end. It is also the first node anywhere to author
    // MaxClassResource, which had a live lane and zero authors.
    Node = MakeNode(TEXT("Caster.Multispell.Reservoir"), TEXT("Reservoir"),
        TEXT("Your Mana pool deepens."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 1, 2, 1);
    AddEffect(Node, EBreakerNodeStatTarget::MaxClassResource, EBreakerNodeStatBucket::IncreasedPercent, 12.0f); // O2 PLACEHOLDER -- Class-Kits MS3 gives +15/+25 flat; the lane is Increased, so this is the shape not the number
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Reservoir.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (MS4-MS6) ----------------------------------------------------
    Node = MakeNode(TEXT("Caster.Multispell.Chain"), TEXT("Chain"),
        TEXT("A target carrying two distinct status types spreads the newest one to the nearest enemy on application. Proc coefficient 0 on the spread; the spread cannot itself spread."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Variance"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Chain.GetTag());
    Tree->Nodes.Add(Node);

    // Makes BREADTH the income: paying per distinct status consumed is what
    // separates Multispell's rotation from Void Whisperer's mastery of one
    // element (O19's stated split between the two branches).
    // WAITING ON: Resonance's consumption path counting distinct types and
    // refunding per type.
    Node = MakeNode(TEXT("Caster.Multispell.Payment"), TEXT("Payment"),
        TEXT("Resonance refunds Mana per distinct status consumed."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Cycle"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Payment.GetTag());
    Tree->Nodes.Add(Node);

    // The three-status lump sum — the explicit reward for rotating all three
    // rather than mastering one. Once per target on a cooldown because the
    // uncapped version is a Mana engine, not a rotation.
    const FBreakerCasterResourceTuning& SequenceTuning = UBreakerManaComponent::GetResourceTuning();
    const FString SequenceDescription = FString::Printf(
        TEXT("Apply three distinct status types to one target within %.0fs to gain %.0f/%.0f Mana at ranks 1/2. Each target can pay once every %.0fs. Secondary applications scale the payout."),
        SequenceTuning.SequenceWindowSeconds, SequenceTuning.SequenceRankOneMana,
        SequenceTuning.SequenceRankTwoMana, SequenceTuning.SequenceCooldownSeconds);
    Node = MakeNode(TEXT("Caster.Multispell.Sequence"), TEXT("Sequence"),
        *SequenceDescription, EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Reservoir"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Sequence.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (MS7-MS8) ----------------------------------------------------
    // "Grants C5 Fracture / grants C6 Resonance" is not authored (see the
    // block comment above); each node's rewrite half is the real content.
    Node = MakeNode(TEXT("Caster.Multispell.Fracture"), TEXT("Fracture"),
        TEXT("Fracture applies two cycle positions at once instead of one."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Chain"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Fracture.GetTag());
    Tree->Nodes.Add(Node);

    // Detonating must not reset the rotation. Halving remaining duration
    // instead of consuming is what lets the branch detonate and keep cycling,
    // which is the difference between a rotation and a reload.
    // WAITING ON: Resonance's consumption path learning to halve rather than
    // consume.
    Node = MakeNode(TEXT("Caster.Multispell.Resonance"), TEXT("Resonance"),
        TEXT("Resonance no longer consumes the statuses it detonates; it halves their remaining duration instead."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Payment"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Resonance.GetTag());
    Tree->Nodes.Add(Node);

    // Status income already shares baseline Overcast doubling. Prepared adds
    // deeper debt allowance, never a second doubling of that same income.
    Node = MakeNode(TEXT("Caster.Multispell.Prepared"), TEXT("Prepared"),
        TEXT("Your Mana can run to -35 before a cast is refused."),
        EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Reservoir"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Prepared.GetTag());
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Caster.Multispell.Interference"), TEXT("Interference"),
        TEXT("Resonance uses a lower fixed damage amount per distinct status and adds a flat bonus at three or more statuses."),
        EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Resonance"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Interference.GetTag());
    Tree->Nodes.Add(Node);
    // The branch keystone O3 permits (3 of 3 for Caster; Caster's budget is
    // now fully allocated across its three branches, same shape as Swift's).
    // Class-Kits MS12's designed line was "1.25x More vs targets carrying 3+
    // distinct status types". The stacking condition it needed EXISTS now
    // (EBreakerBuildCondition::TargetMultiStatus, threshold 3 — O30's axis,
    // landed with the target-condition pass), but a target-conditional More
    // is unsupported BY RULE (Hook-And-Condition-Vocabulary §3.3: it would
    // re-run the strongest-three selection per event per target), so the
    // owner RE-AUTHORED it (ruling 2026-08-16): the payoff ships as a
    // TARGET-RIDER INCREASED line — +25% Increased Damage against targets
    // carrying 3+ distinct statuses, published through
    // BuildTargetConditionRiders and resolved in ReceiveDamage, joining the
    // one additive bucket while it holds. Same trigger, same magnitude
    // number, honest bucket. Caster's third More SLOT stays unspent.
    // Historical Class-Kits MS11. O2 PLACEHOLDER: one reaction per target per
    // 0.5s; each refused reaction queues 10 Mana through conditional income.
    // The original status creditor owns the per-target throttle and capped
    // refusal income; a refused transaction retains fuel without new buildup.
    Node = MakeNode(TEXT("Caster.Multispell.ConductorRule"), TEXT("Conductor's Rule"),
        TEXT("Only one reaction may trigger per target per 0.5s. Reactions that would have triggered instead grant 10 Mana through the conditional income cap."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Chain"));
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Caster.Multispell.ConductorRule")));
    Tree->Nodes.Add(Node);

    Node = MakeNode(TEXT("Caster.Multispell.Cascade"), TEXT("Cascade"),
        TEXT("Branch keystone. Rewrites Unmake: your status applications during it echo the next physical status in Fracture's cycle. Echoes cannot trigger further echoes. Damage is Increased by 25% against targets carrying 3 or more distinct status types."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Caster, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Caster.Multispell.Sequence"));
    AddEffect(Node, EBreakerNodeStatTarget::Damage, EBreakerNodeStatBucket::IncreasedPercent, 25.0f, EBreakerBuildCondition::TargetMultiStatus); // O2 PLACEHOLDER — owner ruling 2026-08-16: MS12's 1.25x More re-authored as a target-rider Increased line
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MS_Cascade.GetTag());
    Node->GrantedTags.AddTag(BreakerAbilityTags::Keystone_Caster_Cascade.GetTag());
    Tree->Nodes.Add(Node);

    return Tree;
}

// ---------------------------------------------------------------------------
// GUNSMITH / TANK / SUPPORT — THE LAST THREE BRANCH LAYERS, AUTHORED AS ONE
// PASS. Owner authorization 2026-08-16 ("feel free to do all 5 classes",
// "keep building"); design sources Class-Kits-Gunsmith §4, Class-Kits-Tank
// §3-5, Class-Kits-Support §4 — the full treatments, which WIN over
// Class-Kits-Unbuilt.md where they disagree. Shared decisions stated once:
//
// TIER COMPRESSION, CITED. Each treatment authors the §0.2 five-tier grammar:
// T1 entry (3 nodes, 2 ranks, cost 1, gate 0), T2 loop (3 nodes, 2 ranks,
// cost 1, gate 3), T3 ability (2 nodes, 1 rank, cost 2, gate 6), T4 rewrite
// (3 nodes, 1 rank, cost 2, gate 10), T5 keystone (1 node, cost 4, gate 16).
// The library compresses that onto the shape every built class already uses:
// doc tiers 1-4 keep their tier numbers (gates via GateForTier: 0/2/4/6, not
// the doc's 0/3/6/10), and the keystone is AN ORDINARY TIER-4 NODE: cost 2,
// gate 6 via GateForTier, still flagged bCornerstone because that flag is what
// makes it the ultimate's rewrite site and what O37 gates on commitment.
//
// It used to be tier 3, cost 3, behind an 8-point CornerstoneInvestmentGate,
// and that arithmetic was written against a per-level class budget where 11
// was affordable. Against O111's 8-point doctrine wallet it was not, and every
// keystone in this file was unbuyable. The gate is gone (see MakeTree) and the
// cost is 2, which makes EVERY doctrine node cost two points to max — so the
// wallet divides into exactly FOUR picks with nothing stranded, where a 3-point
// keystone left one point that could buy nothing. A doctrine is four 2-point
// picks, one of which rewrites the ultimate.
//
// The recorded keystone-before-rewrites tier inversion above GetSwiftKineticTree
// goes with it: the keystone now sits in the same tier as the rewrites it used
// to precede, and competes with them for the last pick rather than preceding
// them.
//
// EVERY NON-KEYSTONE NODE SHIPS AS ITS TREATMENT RULE, VERBATIM, AS A TAG
// WITH NO STAT EFFECT — the Caster posture (see the block comment above
// GetCasterSpellbladeTree), and for the same reason, only more so. All three
// treatments say it themselves: "Every node below is a rule rewrite or a
// resource-loop modifier. No node is a flat percentage." Their subject matter
// is Scrap/Grit/Charge loop routing, deployable lifetime and triggers, shield
// routing, threat, marks and buff cadence — and EBreakerNodeStatTarget has no
// entry for any of it, while the lanes that DO exist either lack these
// classes' consumers (AbilityArea/AbilityDuration are read only by Cleave and
// Rot's geometry accessors; ClassResourceDecay's valve is consumed by
// Momentum and Grit, and no node below changes an unconditional Grit decay)
// or would need a condition O30's vocabulary cannot say (resource bands like
// Dry/Surplus/Ironclad/Resonant, "near your own Anchor Point", "while a
// deployable is active"). Authoring an unconditional stand-in line would be a
// STRONGER node than designed — the invention O2 forbids — so the rule rides
// the tag and the WAITING ON comment names the consumer. Not a shortfall; the
// honest reading of "a node may only author effects the enum can express."
//
// THE KEYSTONE MORES ARE NOT OWED. O95 rules that a doctrine authors NO More
// multiplier at all: every one of the three slots lives in Core, where a
// convergence behind a deep investment gate makes reaching one genuinely
// expensive, and a doctrine pays in RULES instead — conversion, condition
// change, rule rewrite.
//
// So the empty More slot on every keystone below is CORRECT rather than
// pending, and the comments at each site say so. They used to read "RESERVED,
// not spent", which was honest about the code and wrong about the design: it
// described a debt the ledger has since cancelled. Nothing here is waiting for
// a condition vocabulary wide enough to say "while no deployable is active" or
// "within 4 m of your own Anchor Point" — even with that vocabulary the
// multiplier would still be forbidden.
//
// What each keystone DOES pay is its rule, and the rule is live: each grants
// its branch identity tag AND its Keystone.* tag, and the ultimate variant rows
// in BreakerAbilityDefinition.cpp resolve from the day the node is bought.
// That is the whole payload by design, not the surviving half of one.
//
// "GRANTS X" TIER-3 NODES DO NOT RE-AUTHOR ABILITY GRANTS. All seven ability
// ids per class are partitioned across StarterAbilityIds, UnlockableAbilityIds
// and BaseUltimateId on the class definitions below
// (the O39 kits-playable pass), and BreakerBuiltClassKitTests equips them
// with zero node purchases — gating them now would repeat the exact failure
// the Caster catalogue comment documents. Each Tier-3 node carries the REST
// of its design row (the rewrite half distinct from the grant) as its tag.
//
// KEYSTONE TAGS ARE REQUESTED BY STRING. The nine Keystone.Gunsmith/Tank/
// Support.* tags are UE_DEFINE_GAMEPLAY_TAG_STATIC file-locals of
// Abilities/BreakerAbilityDefinition.cpp, which this file must not edit this
// wave; the string is what the ability layer's ResolveVariant matches anyway,
// and the built-class kit tests already take exactly this posture ("the
// string is what a granted GameplayEffect and a save actually key off").
// ---------------------------------------------------------------------------

UBreakerProgressionTree* UBreakerProgressionLibrary::GetGunsmithArmoryTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Gunsmith.Armory"), TEXT("Gunsmith — Armory"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith);

    // --- Tier 1 (AR1-AR3) ---------------------------------------------------
    // AR1. WAITING ON: UBreakerScrapComponent's reload-completed credit
    // learning to fire on a partial reload (and R2, the magazine-dump source
    // dropping its full-at-cycle-start requirement).
    UBreakerProgressionNode* Node = MakeNode(TEXT("Gunsmith.Armory.FieldStripping"), TEXT("Field Stripping"),
        TEXT("Reload Scrap also pays on a reload begun with rounds still chambered, provided at least one was fired. Opens the reload economy to tap-fire play."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_FieldStripping.GetTag());
    Tree->Nodes.Add(Node);

    // AR2. Reads a Scrap band and an affix reload tier; grants no percentage
    // (the treatment's own Swift-K8-shape compliance note, §7). WAITING ON:
    // the Dry/Stocked band state and a reload-tier affix reader.
    Node = MakeNode(TEXT("Gunsmith.Armory.WorkingStock"), TEXT("Working Stock"),
        TEXT("While Dry, your reload is treated one tier faster by anything that reads reload tier. A band rewrite, not a speed percentage."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_WorkingStock.GetTag());
    Tree->Nodes.Add(Node);

    // AR3. WAITING ON: UBreakerWeaponComponent's reload-to-fire boundary
    // reading this tag before it debits the magazine.
    Node = MakeNode(TEXT("Gunsmith.Armory.Chambered"), TEXT("Chambered"),
        TEXT("The first shot after a completed reload consumes no ammunition."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_Chambered.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (AR4-AR6) ---------------------------------------------------
    // AR4. Anti-farm by construction: requires reserve at maximum at pickup.
    // WAITING ON: the ammo pickup path reporting over-max reserve to
    // UBreakerScrapComponent.
    Node = MakeNode(TEXT("Gunsmith.Armory.DeepPockets"), TEXT("Deep Pockets"),
        TEXT("Reserve ammunition picked up over your maximum converts to Scrap instead of vanishing. Only while the reserve is actually full."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.FieldStripping"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_DeepPockets.GetTag());
    Tree->Nodes.Add(Node);

    // AR5. Rewrites where "end of magazine" sits, for the Scrap source AND for
    // Sidearm Rig's window. WAITING ON: UBreakerScrapComponent's magazine-dump
    // source and UBreakerAbility_SidearmRig reading this tag.
    Node = MakeNode(TEXT("Gunsmith.Armory.LastRound"), TEXT("Last Round"),
        TEXT("The magazine-dump payout fires on your last round rather than on empty, and Sidearm Rig's window does not end on that round."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.FieldStripping"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_LastRound.GetTag());
    Tree->Nodes.Add(Node);

    // AR6. An EVENT-driven cooldown refund (per empty reload, once per
    // reload), which the flat AbilityCooldown divisor lane cannot say — a
    // divisor line here would pay on every ability at all times, a different
    // and stronger node. WAITING ON: UBreakerAbility_SidearmRig ticking its
    // own cooldown down when the reload event fires with this tag owned.
    Node = MakeNode(TEXT("Gunsmith.Armory.ColdBarrel"), TEXT("Cold Barrel"),
        TEXT("Completing a reload from an empty magazine shaves 1.5s from Sidearm Rig's cooldown (R2: 2.5s), once per reload."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.Chambered"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_ColdBarrel.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (AR7-AR8) ---------------------------------------------------
    // AR7. "Grants G2 Overhaul" is not authored (block comment above); the
    // conversion tail is the node. WAITING ON: UBreakerAbility_Overhaul
    // reading this tag when its window ends.
    Node = MakeNode(TEXT("Gunsmith.Armory.BenchWork"), TEXT("Bench Work"),
        TEXT("Overhaul's conversion also applies to the next magazine loaded after the window ends, at half strength. The window has a tail."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.DeepPockets"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_BenchWork.GetTag());
    Tree->Nodes.Add(Node);

    // AR8. WAITING ON: UBreakerAbility_SidearmRig measuring its window in
    // shots rather than the magazine.
    Node = MakeNode(TEXT("Gunsmith.Armory.RigDiscipline"), TEXT("Rig Discipline"),
        TEXT("Sidearm Rig's window is counted in shots, not magazines: it survives one reload and ends only when its shots are spent."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.ColdBarrel"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_RigDiscipline.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (AR9-AR11), the rewrite tier ---------------------------------
    // AR9. The explicit affix-to-class bridge (the treatment's F8 pattern):
    // reads the Ammo Returned on Kill affix, duplicates nothing, does nothing
    // for a player without it — and that is correct. WAITING ON:
    // UBreakerScrapComponent crediting off the affix's return event.
    Node = MakeNode(TEXT("Gunsmith.Armory.Reciprocal"), TEXT("Reciprocal"),
        TEXT("Ammo Returned on Kill triggers also pay Scrap, outside the per-second cap. Does nothing without the affix, and that is the design."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.LastRound"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_Reciprocal.GetTag());
    Tree->Nodes.Add(Node);

    // AR10. A rewrite OF a Tier-3 rewrite's subject, so Bench Work is the
    // load-bearing prerequisite. WAITING ON: UBreakerAbility_Overhaul
    // inverting its conversion under this tag.
    Node = MakeNode(TEXT("Gunsmith.Armory.Overpressure"), TEXT("Overpressure"),
        TEXT("Overhaul's bet reverses: capacity converts into reserve instead, and every shot in the window restores a little of it."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.BenchWork"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_Overpressure.GetTag());
    Tree->Nodes.Add(Node);

    // AR11. The cost-for-power rewrite with a real downside (the F11 pattern,
    // named as such by the treatment). WAITING ON: the weapon reserve cap and
    // UBreakerScrapComponent's reload/magazine sources reading this tag.
    Node = MakeNode(TEXT("Gunsmith.Armory.NoReserve"), TEXT("No Reserve"),
        TEXT("Your maximum reserve is halved, and reload and magazine Scrap pay double. A real downside, taken on purpose."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.FieldStripping"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_NoReserve.GetTag());
    Tree->Nodes.Add(Node);

    // AR12 MACHINIST — the branch keystone, compressed to tier 3 / cost 3
    // (block comment above). Its 1.25x weapon-damage More "while you have no
    // deployables active" is NOT OWED (O95 bars a doctrine More): no deployable-state
    // condition exists and WeaponDamage has no composed More lane. The
    // keystone tag is live — Field Assembly's Machinist row resolves.
    Node = MakeNode(TEXT("Gunsmith.Armory.Machinist"), TEXT("Machinist"),
        TEXT("Branch keystone. Rewrites Field Assembly: the ultimate becomes a personal buff for the Gunsmith who placed nothing."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Armory.ColdBarrel"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_AR_Machinist.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Gunsmith.Machinist")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetGunsmithFieldTechTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Gunsmith.FieldTech"), TEXT("Gunsmith — Field Tech"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith);

    // --- Tier 1 (FT1-FT3) ---------------------------------------------------
    // FT1. Refund, never profit: the treatment hard-caps the ceiling at 80%.
    // WAITING ON: the deployable destruction refund path in
    // Abilities/BreakerGunsmithAbilities.cpp reading this node's rank.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Gunsmith.FieldTech.Salvage"), TEXT("Salvage"),
        TEXT("Destroyed deployables refund 65% of cost instead of 50% (R2: 80%, the hard ceiling). Refund, never profit."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Salvage.GetTag());
    Tree->Nodes.Add(Node);

    // FT2. Rewrites targeting; grants no damage. WAITING ON:
    // UBreakerAbility_Turret's target acquisition reading this tag.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Overwatch"), TEXT("Overwatch"),
        TEXT("Your turrets prioritise the target you last damaged over the nearest one (R2: and re-acquire it instantly on its death)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Overwatch.GetTag());
    Tree->Nodes.Add(Node);

    // FT3. Capped at double base lifetime — the anti-farm rule is the cap.
    // WAITING ON: deployable lifetime extension off the reload event.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.SecondShift"), TEXT("Second Shift"),
        TEXT("Reloading near a deployable adds 8s of lifetime (R2: 14s), once per deployable per reload, never past double its base."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_SecondShift.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (FT4-FT6) ---------------------------------------------------
    // FT4. Band-gated (Surplus), so it accelerates the top of the bar where
    // the ultimate lives. WAITING ON: UBreakerScrapComponent's band state and
    // per-deployable ICD reading this tag.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Tithe"), TEXT("Tithe"),
        TEXT("While Surplus, deployable-damage Scrap ignores the per-second cap (R2: and its per-deployable cooldown shortens)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Salvage"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Tithe.GetTag());
    Tree->Nodes.Add(Node);

    // FT5. Compensates for being punished, never for being efficient —
    // destroyed by an ENEMY only, not expiry, not the density cap. WAITING
    // ON: the deployable destruction path distinguishing its causes.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Requisition"), TEXT("Requisition"),
        TEXT("A deployable an enemy destroys refunds immediately, and its replacement placed within 8s costs 10 less Scrap (R2: 18 less)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Salvage"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Requisition.GetTag());
    Tree->Nodes.Add(Node);

    // FT6. Field Tech's only sustain and its solo answer. WAITING ON:
    // UBreakerAbility_AmmoCrate's interaction path (healing has no node lane
    // either — the Support §5.1 overheal hook is the same missing seam).
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Foreman"), TEXT("Foreman"),
        TEXT("Ammo Crate charges also restore a little health (R2: doubled), and a full-reserve interactor consumes charges at half rate."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.SecondShift"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Foreman.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (FT7-FT8) ---------------------------------------------------
    // FT7. Sharpens the starter rather than granting anything. WAITING ON:
    // UBreakerAbility_Turret's acquisition and LOS-grace behaviour.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Emplacement"), TEXT("Emplacement"),
        TEXT("Turret acquires through your own crosshair's priority and holds a target through 1.2s of broken line of sight."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Overwatch"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Emplacement.GetTag());
    Tree->Nodes.Add(Node);

    // FT8. "Grants G4 Ammo Crate" is not authored (block comment above); the
    // density-cap exemption is the node — the single most build-enabling line
    // in the branch. WAITING ON: the deployable density cap reading this tag.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Logistics"), TEXT("Logistics"),
        TEXT("Ammo Crate no longer counts against the density cap. Utility stops competing with firepower."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Foreman"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Logistics.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (FT9-FT11), the rewrite tier ---------------------------------
    // FT9. The only node in the treatment that touches the total density cap,
    // priced at tier 4 in one branch deliberately. WAITING ON: the density
    // cap (4 total / 2 per type) reading this tag.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Redundancy"), TEXT("Redundancy"),
        TEXT("The deployable cap rises from 4 to 5 in total. Per-type stays 2."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Requisition"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Redundancy.GetTag());
    Tree->Nodes.Add(Node);

    // FT10. Bounded by requiring a kill; fired at one-shot proc coefficient.
    // WAITING ON: UBreakerAbility_Turret's reacquire path.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Automation"), TEXT("Automation"),
        TEXT("When a turret's target dies, it fires a free burst at its next target instead of waiting out the reacquire delay."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Emplacement"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Automation.GetTag());
    Tree->Nodes.Add(Node);

    // FT11. Turns the branch's failure state into its payoff; explicitly
    // cannot chain (a Deadman blast destroying a deployable triggers nothing).
    // WAITING ON: the enemy-destruction path in BreakerGunsmithAbilities.cpp.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Deadman"), TEXT("Deadman"),
        TEXT("A deployable destroyed by an enemy detonates before refunding. Detonations never chain into other deployables."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Requisition"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Deadman.GetTag());
    Tree->Nodes.Add(Node);

    // FT12 FOUNDRY — keystone, tier-3/cost-3 compression. Its 1.30x More on
    // DEPLOYABLE damage is NOT OWED (O95): no deployable-damage stat target exists
    // and no other partition can carry "the machines hit harder" honestly.
    // Field Assembly's Foundry row resolves off the tag below.
    Node = MakeNode(TEXT("Gunsmith.FieldTech.Foundry"), TEXT("Foundry"),
        TEXT("Branch keystone. Rewrites Field Assembly for the builder who spends their whole loadout on machines."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.FieldTech.Tithe"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_FT_Foundry.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Gunsmith.Foundry")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetGunsmithTinkererTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Gunsmith.Tinkerer"), TEXT("Gunsmith — Tinkerer"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith);

    // --- Tier 1 (TK1-TK3) ---------------------------------------------------
    // TK1. A band-gated cost reduction (Dry), which the AbilityCost lane
    // cannot say: the lane is unconditional and class-wide, and O30's
    // vocabulary has no resource-band bit. Unconditional would rescue the
    // rich, not the broke — a different node. WAITING ON: the Dry band and a
    // Tinkerer-scoped cost read in BreakerGunsmithAbilities.cpp.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Gunsmith.Tinkerer.CheapWork"), TEXT("Cheap Work"),
        TEXT("While Dry, Tinkerer deployables cost 10 less Scrap (R2: 18 less), to a floor of 10. Rescues a broke Gunsmith, never subsidises a rich one."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_CheapWork.GetTag());
    Tree->Nodes.Add(Node);

    // TK2. R2's trade is explicit: no delay, smaller trigger radius for 1s.
    // WAITING ON: UBreakerAbility_MineCluster's arming delay.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.QuickSet"), TEXT("Quick Set"),
        TEXT("Mine Cluster's arming delay is halved (R2: removed, with a smaller trigger radius for the first second)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_QuickSet.GetTag());
    Tree->Nodes.Add(Node);

    // TK3. The node the deployable definition's swappable trigger field
    // exists for. WAITING ON: UBreakerAbility_MineCluster's trigger condition
    // becoming player-selectable.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.Tripwire"), TEXT("Tripwire"),
        TEXT("Mine charges may trigger on line of sight instead of proximity (R2: choose the condition per placement)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_Tripwire.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (TK4-TK6) ---------------------------------------------------
    // TK4. Loop modifier on the trap economy that does not extend lifetime.
    // WAITING ON: UBreakerAbility_MineCluster's charge bookkeeping.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.Rearm"), TEXT("Rearm"),
        TEXT("An emptied Mine Cluster rearms one charge every 6s (R2: 4s) for the rest of its lifetime, up to its original count."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.QuickSet"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_Rearm.GetTag());
    Tree->Nodes.Add(Node);

    // TK5. WAITING ON: kill-inside-field detection on
    // UBreakerAbility_Disruptor and a capless-but-proc-bounded Scrap credit.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.AttritionField"), TEXT("Attrition Field"),
        TEXT("Enemies killed inside a Disruptor field refund 8 Scrap (R2: 14), outside the global cap. A dense fight pays the field back."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.CheapWork"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_AttritionField.GetTag());
    Tree->Nodes.Add(Node);

    // TK6. The explicit anti-stack rule stated as a benefit (the treatment's
    // MS11-pattern citation). WAITING ON: Disruptor field overlap handling.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.Overlap"), TEXT("Overlap"),
        TEXT("Overlapping Disruptor fields never stack their armour cut, but extend each other's lifetime to the longer of the two."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.Tripwire"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_Overlap.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (TK7-TK8) ---------------------------------------------------
    // TK7. "Grants G5 Mine Cluster" is not authored (block comment above).
    // The second clause is the anti-explosion clause and it is not optional
    // (treatment's own words). WAITING ON: UBreakerAbility_MineCluster's
    // scatter count and 1s same-instance detonation window.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.Ordnance"), TEXT("Ordnance"),
        TEXT("Mine Cluster scatters 4 charges instead of 3, and charges detonating within 1s count as ONE damage instance for procs."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.Rearm"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_Ordnance.GetTag());
    Tree->Nodes.Add(Node);

    // TK8. "Grants G6 Disruptor" is not authored. Delay, never cancel —
    // cancellation is a control verb the class does not own. WAITING ON:
    // UBreakerAbility_Disruptor and the enemy telegraph timers.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.Interdiction"), TEXT("Interdiction"),
        TEXT("Disruptor also delays the wind-up of telegraphed attacks begun inside it. Delays — never cancels."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.Overlap"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_Interdiction.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (TK9-TK11), the rewrite tier ---------------------------------
    // TK9. Rewards the pre-placed field over the panic-placed one. WAITING
    // ON: armed-time tracking on Tinkerer deployables.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.Patience"), TEXT("Patience"),
        TEXT("A trap armed and untriggered for 10s triggers harder: one extra charge, or double the Disruptor's flat armour cut on first entry."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.Rearm"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_Patience.GetTag());
    Tree->Nodes.Add(Node);

    // TK10. The class's stated weakness (placement takes time) inverted for
    // the branch that most needs it, re-imposed on the band that least does.
    // WAITING ON: the Dry/Stocked/Surplus bands and deploy cast time.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.DeadGround"), TEXT("Dead Ground"),
        TEXT("While Dry or Stocked, Tinkerer placements are instant. While Surplus, their cast time doubles."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.CheapWork"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_DeadGround.GetTag());
    Tree->Nodes.Add(Node);

    // TK11. A timing input added without a base-kit verb — re-uses the
    // equipped ability's own input. WAITING ON:
    // UBreakerAbility_MineCluster's re-activation path.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.CommandDetonation"), TEXT("Command Detonation"),
        TEXT("With no charges left to place, re-activating Mine Cluster detonates every armed charge you own at once. Refunds nothing."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.Ordnance"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_CommandDetonation.GetTag());
    Tree->Nodes.Add(Node);

    // TK12 MINEFIELD — keystone, tier-3/cost-3 compression. Its 1.20x More
    // ("inside one of your Disruptor fields, or by a 10s-patient mine") is
    // NOT OWED (O95) — both conditions are deployable state the vocabulary cannot
    // say. Field Assembly's Minefield row resolves off the tag below.
    Node = MakeNode(TEXT("Gunsmith.Tinkerer.Minefield"), TEXT("Minefield"),
        TEXT("Branch keystone. Rewrites Field Assembly for the player who was right about where the enemy would be."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Gunsmith, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Gunsmith.Tinkerer.AttritionField"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_TK_Minefield.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Gunsmith.Minefield")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetTankLeechTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Tank.Leech"), TEXT("Tank — Leech"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank);

    // --- Tier 1 (L1-L3) ------------------------------------------------------
    // L1. Rewrites the routing ratio; grants no shield capacity — Leech owns
    // where healing GOES, never how much there is (§3's territory rule).
    // WAITING ON: the Rend overheal-to-shield path in
    // Abilities/BreakerTankAbilities.cpp reading this node's rank.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Tank.Leech.Clot"), TEXT("Clot"),
        TEXT("Rend's overheal converts to shield at 1.25:1 instead of 1:1 (R2: 1.5:1)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_Clot.GetTag());
    Tree->Nodes.Add(Node);

    // L2. A duration rewrite, not a capacity one. WAITING ON: the Leech
    // shield decay timer.
    Node = MakeNode(TEXT("Tank.Leech.SlowBleed"), TEXT("Slow Bleed"),
        TEXT("Leech shield holds for 5s before decaying instead of 3s (R2: 8s). The decay rate itself is untouched."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_SlowBleed.GetTag());
    Tree->Nodes.Add(Node);

    // L3. Explicit affix-to-class bridge: Life on Hit read, not duplicated.
    // WAITING ON: the multi-hit sweep path crediting Life on Hit per target
    // at proc coefficient.
    Node = MakeNode(TEXT("Tank.Leech.OpenWound"), TEXT("Open Wound"),
        TEXT("Life on Hit also triggers on the first target of a sweep (R2: on every target, at proc coefficient). Reads your gear; adds none."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_OpenWound.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (L4-L6) ------------------------------------------------------
    // L4. Still bound by the shared G1+G2 10/s cap — the cap is what keeps
    // this from being a shield-farming engine. WAITING ON:
    // UBreakerGritComponent's G2 source rate reading this node's rank.
    Node = MakeNode(TEXT("Tank.Leech.FeedTheWound"), TEXT("Feed the Wound"),
        TEXT("Damage taken on shield pays Grit at two-thirds rate instead of half (R2: full rate). The shared cap still binds."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Leech.Clot"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_FeedTheWound.GetTag());
    Tree->Nodes.Add(Node);

    // L5. Overheal from this routes through the normal path and its cap.
    // WAITING ON: the melee-kill heal event (no healing stat target exists).
    Node = MakeNode(TEXT("Tank.Leech.Bloodlet"), TEXT("Bloodlet"),
        TEXT("Melee kills heal 8% of maximum health (R2: 14%). Overheal routes to shield through the normal, capped path."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Leech.OpenWound"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_Bloodlet.GetTag());
    Tree->Nodes.Add(Node);

    // L6. Authored against the O1 passive layer exactly as Swift K5 is: it
    // raises an RNG proc's YIELD, and scales with gear Block Chance, not
    // play. G4's per-source cap is why the shorter ICD is not a back door.
    // WAITING ON: UBreakerGritComponent's block-proc source.
    Node = MakeNode(TEXT("Tank.Leech.Transfusion"), TEXT("Transfusion"),
        TEXT("While shielded, block procs pay +9 Grit instead of +6 (R2: +12) on a slightly faster internal cooldown."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Leech.SlowBleed"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_Transfusion.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (L7-L8) ------------------------------------------------------
    // L7. "Grants T2 Bloodline" is not authored (block comment above); the
    // arc and per-target heal rewrite is the node. WAITING ON:
    // UBreakerAbility_Rend's sweep geometry and heal accounting.
    Node = MakeNode(TEXT("Tank.Leech.RendMastery"), TEXT("Rend Mastery"),
        TEXT("Rend's arc widens to 180 degrees and its heal pays per target hit, at proc coefficient, rather than once per cast."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Leech.Bloodlet"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_RendMastery.GetTag());
    Tree->Nodes.Add(Node);

    // L8. Band-gated: changes WHEN shield persists, never how much there is.
    // WAITING ON: the IRONCLAD Grit band and the shield decay timer.
    Node = MakeNode(TEXT("Tank.Leech.SecondHeart"), TEXT("Second Heart"),
        TEXT("Leech shield does not decay at all while you are at IRONCLAD."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Leech.FeedTheWound"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_SecondHeart.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (L9-L11), the rewrite tier -----------------------------------
    // L9. The branch's thesis node: ALL healing routes its overheal, from any
    // source. Cap unchanged (§7.1). WAITING ON: a class-wide overheal report
    // — the same missing healing seam Support §5.1 names.
    Node = MakeNode(TEXT("Tank.Leech.NothingWasted"), TEXT("Nothing Wasted"),
        TEXT("Every heal you receive — any source — routes its overheal into Leech shield, not just Rend's."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Leech.FeedTheWound"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_NothingWasted.GetTag());
    Tree->Nodes.Add(Node);

    // L10. Deliberately post-break, so it cannot join an absorb-heal-reabsorb
    // cycle inside one shield's lifetime (§7.4's composition audit). WAITING
    // ON: the shield-break event.
    Node = MakeNode(TEXT("Tank.Leech.Reciprocity"), TEXT("Reciprocity"),
        TEXT("When Leech shield breaks, 20% of what it absorbed returns as healing over 2s. After the break, never during."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Leech.Transfusion"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_Reciprocity.GetTag());
    Tree->Nodes.Add(Node);

    // L11. Rewrites Bloodline, which Rend Mastery's design row grants — the
    // prerequisite is load-bearing, the SpendToLive pattern. A real downside:
    // forced off the target, the window ends immediately. WAITING ON:
    // UBreakerAbility_Bloodline's window timer.
    Node = MakeNode(TEXT("Tank.Leech.Exsanguinate"), TEXT("Exsanguinate"),
        TEXT("Bloodline's window no longer expires on time — it expires 2 seconds after your last melee hit lands."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Leech.RendMastery"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_Exsanguinate.GetTag());
    Tree->Nodes.Add(Node);

    // L12 VEIN — keystone, tier-3/cost-3 compression. Its 1.25x MELEE More
    // while Leech shield is active is NOT OWED (O95), and was blocked twice over: MeleeDamage has no
    // composed More lane, and no shield-state condition exists — and the
    // treatment says the double tax IS the design, so an unconditional
    // stand-in would be strictly wrong. Hold's Vein row resolves off the tag.
    Node = MakeNode(TEXT("Tank.Leech.Vein"), TEXT("Vein"),
        TEXT("Branch keystone. Rewrites Hold: the cap comes off and incoming damage converts to healing at a damped rate — attrition, not immunity."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Leech.Transfusion"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_L_Vein.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Vein")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetTankBastionTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Tank.Bastion"), TEXT("Tank — Bastion"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank);

    // Bastion's territory division with the Bulwark constellation (§4.1) is
    // load-bearing: NO node below touches Block chance/quality, Parry, armour
    // quantities, or Dodge, in any direction. Threat, cover-as-an-object, and
    // shield conversion are Bastion's; nothing else is.

    // --- Tier 1 (B1-B3) ------------------------------------------------------
    // B1. Anchor Point's lifetime is a differently-named UPROPERTY on its own
    // ability, and UBreakerAbility_AnchorPoint does not read the
    // AbilityDuration accessor seam yet — an AbilityDuration line here would
    // compose into the aggregate and change nothing in play. WAITING ON:
    // UBreakerAbility_AnchorPoint adopting AbilityDurationMultiplierFor, at
    // which point this becomes the library's first authored line against it.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Tank.Bastion.LineOfSight"), TEXT("Line of Sight"),
        TEXT("Anchor Point stands for 16s instead of 12s (R2: 20s)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_LineOfSight.GetTag());
    Tree->Nodes.Add(Node);

    // B2. Ties the branch's geometry to the branch's resource; still
    // count-independent. WAITING ON: UBreakerGritComponent's proximity source
    // and an own-Anchor-Point distance check.
    Node = MakeNode(TEXT("Tank.Bastion.Footing"), TEXT("Footing"),
        TEXT("Near your own Anchor Point, the proximity Grit source reaches 7 m instead of 5 (R2: 9 m)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_Footing.GetTag());
    Tree->Nodes.Add(Node);

    // B3. WAITING ON: UBreakerAbility_Provoke's radius (same missing
    // area-seam adoption as B1's duration).
    Node = MakeNode(TEXT("Tank.Bastion.Loud"), TEXT("Loud"),
        TEXT("Provoke reaches 13 m instead of 10 (R2: 16 m)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_Loud.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (B4-B6) ------------------------------------------------------
    // B4. A banking rewrite bounded by the deployable's own lifetime and
    // cooldown. The Grit decay valve exists (ClassResourceDecay), but the
    // condition — within 3 m of your own Anchor Point — does not, so the rule
    // rides the tag rather than a mis-conditioned line. WAITING ON: an
    // anchor-proximity predicate feeding the loop valve.
    Node = MakeNode(TEXT("Tank.Bastion.HeldGround"), TEXT("Held Ground"),
        TEXT("Grit does not decay while you stand within 3 m of your own Anchor Point (R2: and placing one re-triggers the entry grant, once)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Bastion.LineOfSight"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_HeldGround.GetTag());
    Tree->Nodes.Add(Node);

    // B5. Count-independent and bound by the 20/s global cap. WAITING ON:
    // UBreakerGritComponent's proximity source reading Provoke's threat list.
    Node = MakeNode(TEXT("Tank.Bastion.AnsweringFire"), TEXT("Answering Fire"),
        TEXT("Enemies you have Provoked pay proximity Grit at 1.5x rate (R2: 2x). Still count-independent, still capped."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Bastion.Loud"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_AnsweringFire.GetTag());
    Tree->Nodes.Add(Node);

    // B6. Durability on an OBJECT, not the player — explicitly outside
    // Bulwark's armour territory. WAITING ON: the Anchor Point actor's health
    // pool and AoE-attribution rule.
    Node = MakeNode(TEXT("Tank.Bastion.Bulk"), TEXT("Bulk"),
        TEXT("Anchor Point carries 50% more health (R2: double) and shrugs off AoE that was not aimed at it."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Bastion.LineOfSight"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_Bulk.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (B7-B8) ------------------------------------------------------
    // B7. "Grants T4 Provoke" is not authored (block comment above). The
    // stationary-spread clause reads a posture the weapon layer already has.
    // WAITING ON: UBreakerAbility_AnchorPoint's placement surface rules and
    // the behind-cover spread read.
    Node = MakeNode(TEXT("Tank.Bastion.Emplacement"), TEXT("Emplacement"),
        TEXT("Anchor Point may be placed on walls and ceilings, and behind your own Anchor Point your spread reads as stationary."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Bastion.AnsweringFire"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_Emplacement.GetTag());
    Tree->Nodes.Add(Node);

    // B8. The self-facing twin is AUTHORED, not assumed — solo, the sharing
    // field pays the Tank itself (§4's solo-conversion requirement). WAITING
    // ON: the shield-sharing field on the Anchor Point actor.
    Node = MakeNode(TEXT("Tank.Bastion.Interposition"), TEXT("Interposition"),
        TEXT("Anchor Point projects a 4 m field behind it: allies inside share your Leech shield — alone, the share pays you as headroom instead."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Bastion.Bulk"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_Interposition.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (B9-B11), the rewrite tier -----------------------------------
    // B9. The solo-conversion thesis node. FLAT bucket by design (before the
    // Increased bucket, so it cannot double-dip) — but the magnitude is a
    // FUNCTION of current shield value, which no static per-rank line can
    // say; a fixed flat-damage line would pay with no shield at all, the
    // exact inversion of "hold it or use it". WAITING ON: the damage path
    // reading current shield through this tag.
    Node = MakeNode(TEXT("Tank.Bastion.Conversion"), TEXT("Conversion"),
        TEXT("While you hold shield, your hits gain flat damage scaled to its CURRENT value. Spend the shield and the bonus falls with it."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Bastion.Interposition"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_Conversion.GetTag());
    Tree->Nodes.Add(Node);

    // B10. Rewrite of the threat RULE. WAITING ON: UBreakerAbility_Provoke's
    // forced-target duration.
    Node = MakeNode(TEXT("Tank.Bastion.StandingOrder"), TEXT("Standing Order"),
        TEXT("Provoke holds until the enemy is damaged by someone who is not you, or 10s pass — whichever comes first."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Bastion.AnsweringFire"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_StandingOrder.GetTag());
    Tree->Nodes.Add(Node);

    // B11. Straight cost-for-power with a real downside. WAITING ON: the
    // Anchor Point actor's damage gate and lifetime.
    Node = MakeNode(TEXT("Tank.Bastion.ImmovableObject"), TEXT("Immovable Object"),
        TEXT("Anchor Point is indestructible for its first 4s — and its total lifetime is halved."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Bastion.Bulk"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_ImmovableObject.GetTag());
    Tree->Nodes.Add(Node);

    // B12 WALL — keystone, tier-3/cost-3 compression. Its 1.20x all-damage
    // More within 4 m of your own Anchor Point is NOT OWED (O95): the tax is
    // positional and no anchor-proximity condition exists. Deliberately NOT
    // authored unconditional — the ability's cooldown and lifetime bound its
    // uptime, and stripping the position strips the tax. Hold's Wall row
    // resolves off the tag below.
    Node = MakeNode(TEXT("Tank.Bastion.Wall"), TEXT("Wall"),
        TEXT("Branch keystone. Rewrites Hold: your per-hit cap extends to allies, and doubles when you are alone."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Bastion.HeldGround"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_B_Wall.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Wall")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetTankDemolitionistTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Tank.Demolitionist"), TEXT("Tank — Demolitionist"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank);

    // O13 compliance is structural here (§5's three rules): self-damage
    // reduction floors at 80% and NEVER reaches 100; self-knockback control
    // is total and granted at tier 1; rocket-jumping is never required. Every
    // node below carries those bounds in its rule text.

    // --- Tier 1 (D1-D3) ------------------------------------------------------
    // D1. A falloff SHAPE rewrite; peak damage unchanged. WAITING ON: the
    // explosive falloff curve in Abilities/BreakerTankAbilities.cpp.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Tank.Demolitionist.ShapedCharge"), TEXT("Shaped Charge"),
        TEXT("Explosive falloff flattens into a full-damage plateau over the inner 40% of the radius (R2: 60%), then falls off normally."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_ShapedCharge.GetTag());
    Tree->Nodes.Add(Node);

    // D2. THE O13 "full self-knockback control" clause, granted at tier 1 and
    // free of any self-damage requirement. WAITING ON: the self-impulse
    // vector on the explosive abilities.
    Node = MakeNode(TEXT("Tank.Demolitionist.Bootstraps"), TEXT("Bootstraps"),
        TEXT("Your own blasts launch you along your aim vector, not the blast normal (R2: reducing the self-damage never weakens the launch)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_Bootstraps.GetTag());
    Tree->Nodes.Add(Node);

    // D3. The node's own text carries the §1.3 interaction: better bracing is
    // WORSE Grit generation from self-damage, by rule. IncomingDamageReduction
    // has no aggregation lane, and this is self-damage-only besides. WAITING
    // ON: the self-damage computation on the explosive abilities.
    Node = MakeNode(TEXT("Tank.Demolitionist.BracedForImpact"), TEXT("Braced for Impact"),
        TEXT("Self-damage reduction rises from 50% to 65% (R2: 80%, the branch ceiling — never 100%). This LOWERS your Grit from self-damage, by rule."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_BracedForImpact.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (D4-D6) ------------------------------------------------------
    // D4. Proc coefficient 0 on the secondary blast and it cannot chain — the
    // anti-recursion guard, mirroring Caster MS4's normalization. WAITING ON:
    // the kill-by-explosive event.
    Node = MakeNode(TEXT("Tank.Demolitionist.Fragmentation"), TEXT("Fragmentation"),
        TEXT("Enemies your explosives kill detonate for a portion of their health in 3 m (R2: 4 m). The echo procs nothing and never chains."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.ShapedCharge"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_Fragmentation.GetTag());
    Tree->Nodes.Add(Node);

    // D5. The accepted landing blast applies the shared interrupt state.
    Node = MakeNode(TEXT("Tank.Demolitionist.Concussion"), TEXT("Concussion"),
        TEXT("Ground Zero staggers for 2.0s instead of 1.5 (R2: 2.5s), and now staggers enemies caught mid-air."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.Bootstraps"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_Concussion.GetTag());
    Tree->Nodes.Add(Node);

    // D6. A trigger-condition rewrite; no damage change. WAITING ON:
    // UBreakerAbility_BreachCharge's fuse and re-press input.
    Node = MakeNode(TEXT("Tank.Demolitionist.Overpressure"), TEXT("Overpressure"),
        TEXT("Breach Charge's fuse may be popped early by re-pressing the input (R2: and it sticks to the first enemy it touches)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.ShapedCharge"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_Overpressure.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (D7-D8) ------------------------------------------------------
    // D7. "Grants T5 Breach Charge" is not authored (block comment above).
    // WAITING ON: UBreakerAbility_BreachCharge holding two charges on one
    // cooldown.
    Node = MakeNode(TEXT("Tank.Demolitionist.Demolition"), TEXT("Demolition"),
        TEXT("Breach Charge holds two charges, sharing one cooldown."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.Overpressure"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_Demolition.GetTag());
    Tree->Nodes.Add(Node);

    // D8. "Grants T6 Ground Zero" is not authored. The from-any-jump clause
    // is restated ON the node so the O13 "never required" rule is visible
    // where a player reads it. Ground Zero reads actual landed fall distance.
    Node = MakeNode(TEXT("Tank.Demolitionist.TerminalDescent"), TEXT("Terminal Descent"),
        TEXT("Ground Zero's fall scaling caps at 25 m instead of 12 — and it casts from ANY airborne state, a plain jump included."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.Concussion"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_TerminalDescent.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (D9-D11), the rewrite tier -----------------------------------
    // D9. Widening the blast must not widen the self-hit: each side of the
    // explosion reads a DIFFERENT geometry, which is exactly why this is not
    // an AbilityArea line (the lane could only scale both). WAITING ON: the
    // explosive radius reads splitting enemy-facing from self-facing.
    Node = MakeNode(TEXT("Tank.Demolitionist.BlastRadius"), TEXT("Blast Radius"),
        TEXT("Your explosive radii grow by half — and your self-damage is still computed at the old, smaller radius."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.Fragmentation"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_BlastRadius.GetTag());
    Tree->Nodes.Add(Node);

    // D10. Makes the rocket-jump LAND cleanly without making it free: takeoff
    // damage untouched, O13 floor intact. The movement component consumes
    // an owned launch only at its authoritative landing.
    Node = MakeNode(TEXT("Tank.Demolitionist.KineticRecovery"), TEXT("Kinetic Recovery"),
        TEXT("Landing within 3s of your own blast launch cancels fall damage and grants 1.5s of stagger immunity. The takeoff still costs."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.Bootstraps"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_KineticRecovery.GetTag());
    Tree->Nodes.Add(Node);

    // D11. Explicitly FLAT, explicitly capped at 3 stacks — the
    // anti-explosion rewrite in the branch that most needs one. A static flat
    // line cannot say "on the later blast, within 1.5s, same target, stacking
    // to 3", so the rule rides the tag. WAITING ON: the explosive damage path
    // stamping blast timestamps per target.
    Node = MakeNode(TEXT("Tank.Demolitionist.ChainReaction"), TEXT("Chain Reaction"),
        TEXT("Explosives landing within 1.5s on one target add damage to the later blast, stacking three times."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.Demolition"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_ChainReaction.GetTag());
    Tree->Nodes.Add(Node);

    // D12 DETONATION — keystone, tier-3/cost-3 compression. Its 1.30x More on
    // EXPLOSIVE damage inside D1's inner plateau is NOT OWED (O95): no explosive
    // partition target and no blast-geometry condition exist, and the
    // treatment's point — a point-blank multiplier on a class that must be
    // point-blank — dies if authored unconditional. Hold's Detonation row
    // resolves off the tag below. Tank's three Mores stay reserved, not spent.
    Node = MakeNode(TEXT("Tank.Demolitionist.Detonation"), TEXT("Detonation"),
        TEXT("Branch keystone. Hold stores damage taken. Press the ultimate again to release 70% in an 8 m blast, without self-damage; expiry also releases it."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Tank, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Tank.Demolitionist.Fragmentation"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_D_Detonation.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Detonation")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetSupportMedicTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Support.Medic"), TEXT("Support — Medic"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support);

    // Medic's branch-wide compliance statement (§4.1): solo, every node
    // functions with self as the target, at the same rate, without exception.
    // The overheal seam every deep Medic node needs is the §5.1 missing hook
    // — healing with overheal reporting — named once here rather than per
    // node.

    // --- Tier 1 (MD1-MD3) ----------------------------------------------------
    // MD1. The 6 Charge/s self-heal cap is what keeps this from becoming a
    // gear engine, and it is untouched. WAITING ON: UBreakerChargeComponent
    // crediting non-Support self-heal sources under this tag.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Support.Medic.FieldDressing"), TEXT("Field Dressing"),
        TEXT("Healing yourself pays Charge at the ally rate even from non-Support sources — leech, regen, pickups (R2: and heals received from allies)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_FieldDressing.GetTag());
    Tree->Nodes.Add(Node);

    // MD2. Redistributes the heal's value; total throughput unchanged.
    // WAITING ON: UBreakerAbility_Patch's heal computation.
    Node = MakeNode(TEXT("Support.Medic.TriagePriority"), TEXT("Triage Priority"),
        TEXT("Patch heals harder the further below full its target is, and less on the healthy, at equal Charge yield (R2: Purge's immunity scales too)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_TriagePriority.GetTag());
    Tree->Nodes.Add(Node);

    // MD3. Explicitly bounded by the cleanse source's 0.5s ICD. WAITING ON:
    // UBreakerAbility_Purge's cleanse accounting.
    Node = MakeNode(TEXT("Support.Medic.CleanHands"), TEXT("Clean Hands"),
        TEXT("Purge pays Charge and refunds cooldown per status removed (R2: doubled refund), bounded by the cleanse's own cooldown."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_CleanHands.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (MD4-MD6) ----------------------------------------------------
    // MD4. The loop-closing node: sustain buys tempo. Band-gated (Attuned)
    // and event-driven, neither of which the cooldown lane can say. WAITING
    // ON: the Attuned band and a per-Charge-credit cooldown tick.
    Node = MakeNode(TEXT("Support.Medic.SteadyHands"), TEXT("Steady Hands"),
        TEXT("At Attuned or better, self-heal Charge credits shave Support cooldowns (R2: ally-heal credits too). At most once a second."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Medic.FieldDressing"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_SteadyHands.GetTag());
    Tree->Nodes.Add(Node);

    // MD5. Turns overheal-generates-nothing from a dead end into a decision.
    // WAITING ON: UBreakerAbility_Patch's full-health cast path and the
    // shielding Charge source.
    Node = MakeNode(TEXT("Support.Medic.SecondOpinion"), TEXT("Second Opinion"),
        TEXT("Patch cast on a full-health target grants a shield instead, paying from the shielding source (R2: half of it echoes onto you)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Medic.TriagePriority"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_SecondOpinion.GetTag());
    Tree->Nodes.Add(Node);

    // MD6. Actual restored health pays the marked source at proc weight;
    // rank two refreshes both the mark's payload clock and its display.
    Node = MakeNode(TEXT("Support.Medic.Attending"), TEXT("Attending"),
        TEXT("Healing while your mark is live also pays the marked-target source at the damage rate (R2: and refreshes the mark)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Medic.FieldDressing"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_Attending.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (MD7-MD8) ----------------------------------------------------
    // MD7. "Grants U2 Purge" is not authored (block comment above); the
    // buff-strip and application-suppression rewrite is the node. WAITING ON:
    // UBreakerAbility_Purge's target rules and immunity window.
    Node = MakeNode(TEXT("Support.Medic.FieldKit"), TEXT("Field Kit"),
        TEXT("Purge cast on an enemy strips one buff, and its immunity window also blocks NEW statuses from landing."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Medic.SteadyHands"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_FieldKit.GetTag());
    Tree->Nodes.Add(Node);

    // MD8. Medic's second tier-3 node is a REWRITE, mirroring Swift F8/K8 —
    // and its ticks generate at the heal source's proc coefficient, not 1.0,
    // so it must not out-generate the instant it replaces. WAITING ON:
    // UBreakerAbility_Patch's heal delivery.
    Node = MakeNode(TEXT("Support.Medic.SustainedCare"), TEXT("Sustained Care"),
        TEXT("Patch splits into an instant portion and a heal-over-time. The ticks pay Charge at proc coefficient, never at full rate."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Medic.SecondOpinion"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_SustainedCare.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (MD9-MD11), the rewrite tier ---------------------------------
    // MD9. The distinction is exact and must be exact in code (§4.1's own
    // words): the overheal still generates NOTHING; the shield it becomes
    // generates through the shield door and its over-shield cap. WAITING ON:
    // the §5.1 overheal report.
    Node = MakeNode(TEXT("Support.Medic.Overflow"), TEXT("Overflow"),
        TEXT("Overheal is no longer discarded: it becomes a shield at a fraction of its value. The overheal pays nothing; the shield pays as shield."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Medic.SecondOpinion"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_Overflow.GetTag());
    Tree->Nodes.Add(Node);

    // MD10. The Medic's damage-conversion path — flat bucket, before the
    // Increased bucket, per Item-Foundation — but the magnitude is an
    // accumulated POOL, not a per-rank constant, so no static line can carry
    // it. WAITING ON: the debt pool and the marked-hit consumption read.
    Node = MakeNode(TEXT("Support.Medic.BloodDebt"), TEXT("Blood Debt"),
        TEXT("Healing — self-healing included, at full rate — banks into a pool. Your next weapon hit on a marked target spends it as flat damage."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Medic.Attending"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_BloodDebt.GetTag());
    Tree->Nodes.Add(Node);

    // MD11. The deliberate solo-specialist rewrite — a downgrade in a party,
    // an upgrade alone, the treatment's own F11-No-Safety citation. The
    // cost/cooldown halves are Patch/Purge-scoped, which the class-wide
    // AbilityCost/AbilityCooldown lanes cannot scope. WAITING ON:
    // UBreakerAbility_Patch and UBreakerAbility_Purge's target gates.
    Node = MakeNode(TEXT("Support.Medic.NoTriage"), TEXT("No Triage"),
        TEXT("Patch and Purge become self-only — and far cheaper, on shorter cooldowns. Worse in a party, better alone, chosen on purpose."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Medic.SustainedCare"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_NoTriage.GetTag());
    Tree->Nodes.Add(Node);

    // MD12. Conduit's healing field owns one lethal rescue per target/cast.
    Node = MakeNode(TEXT("Support.Medic.Triage"), TEXT("Triage"),
        TEXT("Branch keystone. Rewrites Conduit: a continuous healing field with one lethal-hit save per target, and no free casts."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Medic.SteadyHands"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_MD_Triage.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Triage")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetSupportConductorTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Support.Conductor"), TEXT("Support — Conductor"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support);

    // Conductor's non-negotiable rule (§4.2): every Conductor buff applies to
    // the Support FIRST and allies second — the branch's solo guarantee.
    // Attunement converts held weapons to the available Entropy element.
    // Sympathetic adds independent buildup and protects the recipient's
    // contributions with a gradual fade while the cast is actually buffed.

    // --- Tier 1 (CO1-CO3) ----------------------------------------------------
    // CO1. The self-first rule expressed as duration. WAITING ON: the
    // Conductor buff layer (§5.4's count-independent uptime system).
    UBreakerProgressionNode* Node = MakeNode(TEXT("Support.Conductor.DownbeatDiscipline"), TEXT("Downbeat Discipline"),
        TEXT("Your own copy of every Conductor buff outlasts the copies you hand out (R2: longer still)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_DownbeatDiscipline.GetTag());
    Tree->Nodes.Add(Node);

    // CO2. Cadence enlarges its actual footprint and follows at sprint speed.
    Node = MakeNode(TEXT("Support.Conductor.Section"), TEXT("Section"),
        TEXT("Cadence's aura reaches further and keeps pace at sprint speed (R2: further again)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Section.GetTag());
    Tree->Nodes.Add(Node);

    // CO3. Smooths the +2/s uptime source's sawtooth; still combat-gated,
    // still count-independent. WAITING ON: UBreakerChargeComponent's
    // buff-uptime source.
    Node = MakeNode(TEXT("Support.Conductor.Sustain"), TEXT("Sustain"),
        TEXT("Buff-uptime Charge keeps paying for a short grace after the last buff expires (R2: longer grace)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Sustain.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (CO4-CO6) ----------------------------------------------------
    // CO4. Explicit anti-stack, the treatment's own VW4-Lingering citation.
    // WAITING ON: the buff re-application path.
    Node = MakeNode(TEXT("Support.Conductor.Rehearsal"), TEXT("Rehearsal"),
        TEXT("Re-applying a live Conductor buff refreshes it — stacks intact — and refunds part of its cost (R2: a larger refund)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Conductor.DownbeatDiscipline"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Rehearsal.GetTag());
    Tree->Nodes.Add(Node);

    // CO5. Rank one improves the caster's ramp; rank two improves every holder.
    Node = MakeNode(TEXT("Support.Conductor.Tempo"), TEXT("Tempo"),
        TEXT("Metronome stacks higher and resets slower for you specifically (R2: for everyone you buffed)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Conductor.Sustain"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Tempo.GetTag());
    Tree->Nodes.Add(Node);

    // CO6 converts weapon type without adding damage. Entropy is the first
    // available element; Void and Rift choices follow their complete pipelines.
    Node = MakeNode(TEXT("Support.Conductor.Attunement"), TEXT("Attunement"),
        TEXT("Your Conductor buffs convert the recipient's weapon damage to Entropy, the currently available element. Adds no damage. R2: conversion lingers briefly after the buff ends."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Conductor.Section"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Attunement.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (CO7-CO8) ----------------------------------------------------
    // CO7. Cadence shortens each actual recipient's cooldowns and retains a
    // brief caster-owned effect after leaving a detached footprint.
    Node = MakeNode(TEXT("Support.Conductor.Conducting"), TEXT("Conducting"),
        TEXT("Cadence also speeds ability cooldown recovery for the buffed, and clings to you briefly if its aura ever leaves you."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Conductor.Rehearsal"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Conducting.GetTag());
    Tree->Nodes.Add(Node);

    // CO8. Actual damaging ability hits and ticks advance each holder's ramp
    // at their proc coefficient; this node does not grant the ability itself.
    Node = MakeNode(TEXT("Support.Conductor.Counterpoint"), TEXT("Counterpoint"),
        TEXT("Metronome stacks from ANY damage the buffed target deals — abilities, ticks at proc coefficient, deployables — not weapon hits alone."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Conductor.Tempo"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Counterpoint.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (CO9-CO11), the rewrite tier ---------------------------------
    // CO9. Band-gated (Resonant); reads the state, adds no percentage.
    // WAITING ON: the Resonant band and buff application path.
    Node = MakeNode(TEXT("Support.Conductor.StandingOvation"), TEXT("Standing Ovation"),
        TEXT("At Resonant, your Conductor buffs land extended and cannot be stripped by enemies."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Conductor.Rehearsal"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_StandingOvation.GetTag());
    Tree->Nodes.Add(Node);

    // CO10 feeds the buildup track without adding damage to the applying hit
    // or to its Rot snapshot. A maintained buff is required; R2 tails do not pay.
    Node = MakeNode(TEXT("Support.Conductor.SympatheticResonance"), TEXT("Sympathetic Resonance"),
        TEXT("While your Conductor buff is maintained, the recipient's Entropy hits add buildup independent of damage dealt. Their buildup fades gradually after its grace period. Adds no hit or Rot damage and triggers no reaction."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Conductor.Attunement"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_SympatheticResonance.GetTag());
    Tree->Nodes.Add(Node);

    // CO11. The ONE node in the branch that suspends the self-first rule, and
    // the treatment flags it exactly so: a deliberate party-play trade the
    // solo player declines, optional, with nothing depending on it.
    Node = MakeNode(TEXT("Support.Conductor.DetachedBaton"), TEXT("Detached Baton"),
        TEXT("Cadence may be planted as a much larger stationary zone — which no longer applies to you first. A party trade, declined solo."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Conductor.Conducting"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_DetachedBaton.GetTag());
    Tree->Nodes.Add(Node);

    // CO12 DOWNBEAT — keystone, tier-3/cost-3 compression. Its 1.25x WEAPON
    // More while a self-authored Conductor buff is live on YOU is NOT OWED (O95):
    // no buff-state condition exists and WeaponDamage has no composed More
    // lane. "On yourself" is the deliberate solo-satisfiable condition, and
    // dropping it would change the node's meaning, not its number. Conduit's
    // Downbeat row resolves off the tag below.
    Node = MakeNode(TEXT("Support.Conductor.Downbeat"), TEXT("Downbeat"),
        TEXT("Branch keystone. Rewrites Conduit for the Support whose first instrument is themselves."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Conductor.Rehearsal"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_CO_Downbeat.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Downbeat")));
    Tree->Nodes.Add(Node);

    return Tree;
}

UBreakerProgressionTree* UBreakerProgressionLibrary::GetSupportWardenTree()
{
    static UBreakerProgressionTree* Tree = nullptr;
    if (Tree) return Tree;

    Tree = MakeTree(TEXT("Doctrine.Support.Warden"), TEXT("Support — Warden"), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support);

    // Warden is the natively solo branch (§4.3) — marks generate, debuff, and
    // convert identically at any party size, which is why Class-Kits §5
    // points the solo player here first.

    // --- Tier 1 (WA1-WA3) ----------------------------------------------------
    // WA1. Own weapon hits generate by default; purchased ranks admit own
    // abilities/ticks, then allied damage at a reduced proc-weighted rate.
    UBreakerProgressionNode* Node = MakeNode(TEXT("Support.Warden.Painted"), TEXT("Painted"),
        TEXT("Marked-target Charge pays on your ability and DoT damage, not weapon hits alone (R2: allied damage too, at a reduced rate)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_Painted.GetTag());
    Tree->Nodes.Add(Node);

    // WA2. Warden's whole tempo is mark uptime. Mark's duration is an
    // ability-owned number and UBreakerAbility_Mark does not read the
    // AbilityDuration seam — same posture as Tank B1. WAITING ON:
    // UBreakerAbility_Mark adopting AbilityDurationMultiplierFor and the
    // re-mark cooldown exemption.
    Node = MakeNode(TEXT("Support.Warden.LongWatch"), TEXT("Long Watch"),
        TEXT("Marks last longer, and re-marking a still-marked target spends no cooldown (R2: longer again)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_LongWatch.GetTag());
    Tree->Nodes.Add(Node);

    // WA3. Handling rewrite. WAITING ON: UBreakerAbility_Suppress's radius
    // and slow-application delay.
    Node = MakeNode(TEXT("Support.Warden.FieldOfView"), TEXT("Field of View"),
        TEXT("Suppress reaches further and its slow lands the instant an enemy enters (R2: the accuracy cut lands instantly too)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 1, 2, 1);
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_FieldOfView.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 2 (WA4-WA6) ----------------------------------------------------
    // WA4. Proc coefficient 0 on the jump — the anti-chain-generation guard —
    // and DELIBERATELY the same rule as Swift M5 Mark Economy, so one shared
    // mark implementation carries one rule, not two. WAITING ON: the shared
    // mark-death handoff (the seam Node_MarkEconomy already waits on).
    Node = MakeNode(TEXT("Support.Warden.Handoff"), TEXT("Handoff"),
        TEXT("A mark survives its target's death and jumps to the nearest unmarked enemy (R2: further). The jump itself pays nothing."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Warden.LongWatch"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_Handoff.GetTag());
    Tree->Nodes.Add(Node);

    // WA5. Count-independence applied to an enemy-facing source: pack density
    // must not be a resource multiplier. WAITING ON: UBreakerChargeComponent
    // and Suppress's occupancy check.
    Node = MakeNode(TEXT("Support.Warden.Pressure"), TEXT("Pressure"),
        TEXT("Enemies inside Suppress pay you Charge at a slow, count-independent rate — one pays the same as six (R2: faster)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Warden.FieldOfView"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_Pressure.GetTag());
    Tree->Nodes.Add(Node);

    // WA6. A defensive node in an offensive branch — Warden's answer to being
    // alone in the room it aggravated. The damage-taken reduction is
    // mark-scoped, which IncomingDamageReduction (no lane anyway) could not
    // scope. WAITING ON: the mark's telegraph and damage-out read.
    Node = MakeNode(TEXT("Support.Warden.Tell"), TEXT("Tell"),
        TEXT("Marked targets telegraph their next attack to you, and hit you softer while the mark lives (R2: your allies too)."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 2, 2, 1);
    AddPrerequisite(Node, TEXT("Support.Warden.Painted"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_Tell.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 3 (WA7-WA8) ----------------------------------------------------
    // WA7. "Grants U6 Suppress" is not authored (block comment above). FLAT
    // armour cut, never a percentage — the boss-cap protection, matching
    // Caster VW7 Zonework's precedent by the treatment's own citation.
    // WAITING ON: UBreakerAbility_Suppress's field effect list.
    Node = MakeNode(TEXT("Support.Warden.Suppression"), TEXT("Suppression"),
        TEXT("Suppress also cuts the Armour of enemies inside it by a flat amount. Flat, never a percentage."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Warden.Pressure"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_Suppression.GetTag());
    Tree->Nodes.Add(Node);

    // WA8. Warden's second tier-3 node is a rewrite, no grant. Anti-farm rule
    // 7 still applies — deepening does not refresh generation eligibility.
    // WAITING ON: the mark's stacking state.
    Node = MakeNode(TEXT("Support.Warden.DeepMark"), TEXT("Deep Mark"),
        TEXT("Marking a marked target deepens it: more damage taken, richer Charge yield. Deepening never resets the anti-farm window."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 3, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Warden.Handoff"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_DeepMark.GetTag());
    Tree->Nodes.Add(Node);

    // --- Tier 4 (WA9-WA11), the rewrite tier ---------------------------------
    // WA9. Rewards marking what you can kill, not marking everything. WAITING
    // ON: the marked-kill event and Mark's cost/cooldown refund.
    Node = MakeNode(TEXT("Support.Warden.ExecutionersLedger"), TEXT("Executioner's Ledger"),
        TEXT("Killing a marked target refunds Mark's cost and cooldown in proportion to the mark's unspent duration."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Warden.Handoff"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_ExecutionersLedger.GetTag());
    Tree->Nodes.Add(Node);

    // WA10. Band-gated (Resonant) rule rewrite; no percentage. The
    // prerequisite is the node whose ability it rewrites — load-bearing, the
    // SpendToLive pattern. WAITING ON: the Resonant band and enemy
    // buff/heal-suppression inside Suppress.
    Node = MakeNode(TEXT("Support.Warden.BlackoutProtocol"), TEXT("Blackout Protocol"),
        TEXT("At Resonant, marked enemies inside Suppress can be neither buffed nor healed."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Warden.Suppression"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_BlackoutProtocol.GetTag());
    Tree->Nodes.Add(Node);

    // WA11. The class's floor-recovery answer: a Support at zero Charge is
    // never soft-locked, because the ignition source becomes free. WAITING
    // ON: UBreakerAbility_Mark's cost, duration and single-target gates.
    Node = MakeNode(TEXT("Support.Warden.HuntersEconomy"), TEXT("Hunter's Economy"),
        TEXT("Mark costs nothing — but runs much shorter and holds one target only. A free, constantly-cycling ignition source."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Warden.LongWatch"));
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_HuntersEconomy.GetTag());
    Tree->Nodes.Add(Node);

    // WA12 BLACKOUT — keystone, tier-3/cost-3 compression. Its 1.30x More
    // against targets marked by you is NOT OWED (O95): no target-marked condition
    // exists (a mark is ability state, not a Status.* tag, so TargetAiling
    // cannot stand in). The condition being self-supplied at level 1 by a
    // starter is exactly why unconditional would be wrong — it would delete
    // the one requirement the solo loop is built around. Conduit's Blackout
    // row resolves off the tag below. Support's three Mores stay reserved.
    Node = MakeNode(TEXT("Support.Warden.Blackout"), TEXT("Blackout"),
        TEXT("Branch keystone. Rewrites Conduit for the Support who plays the enemy, not the ally."), EBreakerPointCurrency::DoctrinePoints, EBreakerClassId::Support, 4, 1, 2);
    AddPrerequisite(Node, TEXT("Support.Warden.Handoff"));
    Node->bCornerstone = true;
    Node->GrantedTags.AddTag(BreakerNodeTags::Node_WA_Blackout.GetTag());
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Blackout")));
    Tree->Nodes.Add(Node);

    return Tree;
}

const TArray<UBreakerProgressionTree*>& UBreakerProgressionLibrary::GetAllFallbackTrees()
{
    static TArray<UBreakerProgressionTree*> Trees;
    if (Trees.Num() == 0)
    {
        Trees.Add(GetCoreSliceTree());
        Trees.Add(GetSwiftFrenzyTree());
        Trees.Add(GetSwiftKineticTree());
        Trees.Add(GetSwiftMarksmanTree());
        Trees.Add(GetCasterSpellbladeTree());
        Trees.Add(GetCasterVoidWhispererTree());
        Trees.Add(GetCasterMultispellTree());
        Trees.Add(GetGunsmithArmoryTree());
        Trees.Add(GetGunsmithFieldTechTree());
        Trees.Add(GetGunsmithTinkererTree());
        Trees.Add(GetTankLeechTree());
        Trees.Add(GetTankBastionTree());
        Trees.Add(GetTankDemolitionistTree());
        Trees.Add(GetSupportMedicTree());
        Trees.Add(GetSupportConductorTree());
        Trees.Add(GetSupportWardenTree());
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

// ---------------------------------------------------------------------------
// The quartermaster's stock is a row in Data/class-kits.json: what a class
// starts with, what it sells and in which order (the screen offers
// Unlockable[0] first), and its ultimate. A row that fails any check fails
// the WHOLE file: every class keeps empty lists behind an ensure, because a
// file that served four classes and dropped one would ship a class whose
// loadout resolves to nothing.
namespace
{
    using BreakerDataFile::FBreakerDataErrors;

    struct FBreakerClassKitRow
    {
        EBreakerClassId ClassId = EBreakerClassId::None;
        TArray<FName> StarterAbilityIds;
        TArray<FName> UnlockableAbilityIds;
        FName BaseUltimateId;
    };

    struct FBreakerClassKitLoad
    {
        TArray<FBreakerClassKitRow> Rows;
        TArray<FString> Errors;
    };

    // One string array into one id list; every id non-empty and unseen in
    // this row so far.
    bool BreakerClassKitReadIds(const FJsonObject& Row, const TCHAR* Field, const FString& Context,
        TArray<FName>& Out, TSet<FName>& SeenInRow, FBreakerDataErrors& Errors)
    {
        Out.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Row.TryGetArrayField(Field, Values))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not an array"), *Context, Field));
            return false;
        }
        bool bOk = true;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Id;
            if (!Value.IsValid() || !Value->TryGetString(Id) || Id.IsEmpty())
            {
                Errors.Add(FString::Printf(TEXT("%s: \"%s\" holds an empty or non-string id"), *Context, Field));
                bOk = false;
                continue;
            }
            const FName Name(*Id);
            if (SeenInRow.Contains(Name))
            {
                Errors.Add(FString::Printf(TEXT("%s: \"%s\" appears twice in the row"), *Context, *Id));
                bOk = false;
                continue;
            }
            SeenInRow.Add(Name);
            Out.Add(Name);
        }
        return bOk;
    }

    bool BreakerClassKitReadRow(const FJsonObject& Row, FBreakerClassKitRow& Out, FBreakerDataErrors& Errors)
    {
        FString ClassName;
        if (!Row.TryGetStringField(TEXT("classId"), ClassName)
            || !BreakerDataFile::ParseEnum(ClassName, Out.ClassId)
            || Out.ClassId == EBreakerClassId::None)
        {
            Errors.Add(FString::Printf(TEXT("class-kits: \"classId\" \"%s\" is not a class with a kit"), *ClassName));
            return false;
        }
        const FString Context = ClassName;

        TSet<FName> SeenInRow;
        bool bOk = BreakerClassKitReadIds(Row, TEXT("starterAbilityIds"), Context, Out.StarterAbilityIds, SeenInRow, Errors);
        bOk = BreakerClassKitReadIds(Row, TEXT("unlockableAbilityIds"), Context, Out.UnlockableAbilityIds, SeenInRow, Errors) && bOk;
        // Slot one seeds from Starter[0]; a class with no starter has no
        // level-one loadout.
        if (Out.StarterAbilityIds.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: \"starterAbilityIds\" is empty"), *Context));
            bOk = false;
        }

        FString Ultimate;
        if (!Row.TryGetStringField(TEXT("baseUltimateId"), Ultimate) || Ultimate.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: \"baseUltimateId\" is missing or empty"), *Context));
            bOk = false;
        }
        else
        {
            Out.BaseUltimateId = FName(*Ultimate);
            if (SeenInRow.Contains(Out.BaseUltimateId))
            {
                Errors.Add(FString::Printf(TEXT("%s: \"%s\" appears twice in the row"), *Context, *Ultimate));
                bOk = false;
            }
        }
        return bOk;
    }

    FBreakerClassKitLoad BreakerClassKitLoadData()
    {
        FBreakerClassKitLoad Load;
        FBreakerDataErrors Errors;
        TArray<FBreakerClassKitRow> Rows;
        const FString File = UBreakerProgressionLibrary::ClassKitsRelativePath();

        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
            if (!Root->TryGetArrayField(TEXT("classKits"), RowValues))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"classKits\" array"), *File));
            }
            else
            {
                for (const TSharedPtr<FJsonValue>& Value : *RowValues)
                {
                    const TSharedPtr<FJsonObject>* RowObject = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(RowObject))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: a \"classKits\" entry is not an object"), *File));
                        continue;
                    }
                    FBreakerClassKitRow Row;
                    if (BreakerClassKitReadRow(**RowObject, Row, Errors))
                    {
                        Rows.Add(Row);
                    }
                }
            }

            // Exactly one row per class with a kit: None has none, and the
            // generated _MAX is a bound, not a class.
            const UEnum* ClassEnum = StaticEnum<EBreakerClassId>();
            for (int32 Index = 0; Index < ClassEnum->NumEnums() - 1; ++Index)
            {
                const EBreakerClassId ClassId = static_cast<EBreakerClassId>(ClassEnum->GetValueByIndex(Index));
                if (ClassId == EBreakerClassId::None) { continue; }
                int32 Count = 0;
                for (const FBreakerClassKitRow& Row : Rows) { if (Row.ClassId == ClassId) { ++Count; } }
                if (Count != 1)
                {
                    Errors.Add(FString::Printf(TEXT("%s: %d rows for %s; exactly one is expected"),
                        *File, Count, *ClassEnum->GetNameStringByIndex(Index)));
                }
            }
        }

        if (!Errors.IsClean())
        {
            Load.Errors = Errors.Messages;
            ensureMsgf(false, TEXT("%s failed to load; every class kit is EMPTY.\n%s"), *File, *Errors.Join());
            return Load;
        }
        Load.Rows = MoveTemp(Rows);
        return Load;
    }

    const FBreakerClassKitLoad& BreakerClassKitLoaded()
    {
        static const FBreakerClassKitLoad Load = BreakerClassKitLoadData();
        return Load;
    }

    // The three id lists of a definition, from its class's row. A definition
    // whose row did not load keeps its default-constructed empty lists.
    void BreakerClassKitApply(UBreakerClassDefinition& Definition)
    {
        for (const FBreakerClassKitRow& Row : BreakerClassKitLoaded().Rows)
        {
            if (Row.ClassId != Definition.ClassId) { continue; }
            Definition.StarterAbilityIds = Row.StarterAbilityIds;
            Definition.UnlockableAbilityIds = Row.UnlockableAbilityIds;
            Definition.BaseUltimateId = Row.BaseUltimateId;
            return;
        }
    }
}

FString UBreakerProgressionLibrary::ClassKitsRelativePath()
{
    return TEXT("Data/class-kits.json");
}

const TArray<FString>& UBreakerProgressionLibrary::GetClassKitDataErrors()
{
    return BreakerClassKitLoaded().Errors;
}

UBreakerClassDefinition* UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId ClassId)
{
    // O39 SLICE CLASS HONESTY: a class gets a row here only once its kit
    // EXECUTES. All five now do — Gunsmith, Tank and Support landed 2026-08-16
    // (owner authorization: "feel free to do all 5 classes"), and their rows
    // below were registered AFTER their ability classes, never before: a class
    // definition registered ahead of executing abilities is the exact ordering
    // that made every Caster ability read as locked (T7 step order).
    //
    // Each class's kit row lives in Data/class-kits.json and mirrors the ability
    // fallback registry's ids EXACTLY (Abilities/BreakerAbilityDefinition.cpp), all seven per class, starters
    // first — IsAbilityUnlocked answers from this list, and gating any id
    // behind an unpurchased node would repeat the "grants nothing reachable"
    // failure the Caster row's own comment documents.
    //
    // BRANCH TREES JOINED 2026-08-16, same owner authorization: the branch
    // layers the kits-playable pass deliberately left out are now authored
    // (see the Gunsmith/Tank/Support block comment above GetGunsmithArmoryTree)
    // and each row lists its three branches plus Core, exactly as Swift and
    // Caster do. The keystone reachability suite's honest-emptiness arm no
    // longer applies to these classes — from this commit, its FULL arm does,
    // and the nine Keystone.* tags resolve from the branch cornerstones.
    //
    // THE ORDER A SIXTH CLASS MUST BE BUILT IN. All five above were executed
    // in exactly this sequence, and it is the template:
    //   1. Attach the resource component (Characters/BreakerCharacter.cpp,
    //      beside Momentum and Mana).
    //   2. Wire its Notify* generation entry points from real callers in
    //      combat / weapons / status / healing. An entry point with no caller
    //      is a resource bar that sits at zero forever.
    //   3. Write the UGameplayAbility subclasses and assign AbilityClass on
    //      every row.
    //   4. Add the row to Data/class-kits.json, mirroring the registry ids exactly.
    //   5. Add the class to DefaultAbilityIdForSlot
    //      (Abilities/BreakerAbilityDefinition.cpp).
    //   6. Add the resource to the HUD.
    //   7. O39's gate then opens BY ITSELF — ClassHasImplementedKit is
    //      derived, not a list, so nothing needs editing to admit the class.
    // STEPS 3 AND 4 MUST NOT BE INVERTED. Registering a class definition
    // before its abilities execute is what made every Caster ability read as
    // locked: the class becomes selectable, the loadout resolves ids that do
    // not run, and the player permanently locks into a kit that grants
    // nothing. Abilities first, always.
    if (ClassId == EBreakerClassId::Gunsmith)
    {
        static UBreakerClassDefinition* Gunsmith = nullptr;
        if (Gunsmith) return Gunsmith;

        Gunsmith = NewObject<UBreakerClassDefinition>(GetTransientPackage(), UBreakerClassDefinition::StaticClass(), NAME_None, RF_Standalone);
        Gunsmith->AddToRoot();
        Gunsmith->ClassAssetId = TEXT("Gunsmith");
        Gunsmith->ClassId = EBreakerClassId::Gunsmith;
        Gunsmith->DisplayName = LOCTEXT("GunsmithName", "Gunsmith");
        Gunsmith->Description = LOCTEXT("GunsmithDescription",
            "Scrap: a ledger of work already done -- no idle income, no decay. Deployables spend it; the gun in your hands costs nothing.");
        // Starters first (Sidearm Rig / Turret, Class-Kits-Gunsmith §3), so a
        // loadout seeded from [0]/[1] matches DefaultAbilityIdForSlot. The
        // row is in Data/class-kits.json.
        BreakerClassKitApply(*Gunsmith);
        // Class-Kits-Gunsmith §4 order: Armory, Field Tech, Tinkerer.
        Gunsmith->BranchTrees.Add(GetGunsmithArmoryTree());
        Gunsmith->BranchTrees.Add(GetGunsmithFieldTechTree());
        Gunsmith->BranchTrees.Add(GetGunsmithTinkererTree());
        Gunsmith->BranchTrees.Add(GetCoreSliceTree());
        return Gunsmith;
    }

    if (ClassId == EBreakerClassId::Tank)
    {
        static UBreakerClassDefinition* Tank = nullptr;
        if (Tank) return Tank;

        Tank = NewObject<UBreakerClassDefinition>(GetTransientPackage(), UBreakerClassDefinition::StaticClass(), NAME_None, RF_Standalone);
        Tank->AddToRoot();
        Tank->ClassAssetId = TEXT("Tank");
        Tank->ClassId = EBreakerClassId::Tank;
        Tank->DisplayName = LOCTEXT("TankName", "Tank");
        Tank->Description = LOCTEXT("TankDescription",
            "Grit: banked by taking hits and holding ground, bleeding on a lapse timer. Stronger for being hit, never wanting to be hit more than necessary.");
        // Starters first (Rend / Anchor Point, Class-Kits-Tank §2). The row is
        // in Data/class-kits.json.
        BreakerClassKitApply(*Tank);
        // Class-Kits-Tank §3-5 order: Leech, Bastion, Demolitionist.
        Tank->BranchTrees.Add(GetTankLeechTree());
        Tank->BranchTrees.Add(GetTankBastionTree());
        Tank->BranchTrees.Add(GetTankDemolitionistTree());
        Tank->BranchTrees.Add(GetCoreSliceTree());
        return Tank;
    }

    if (ClassId == EBreakerClassId::Support)
    {
        static UBreakerClassDefinition* Support = nullptr;
        if (Support) return Support;

        Support = NewObject<UBreakerClassDefinition>(GetTransientPackage(), UBreakerClassDefinition::StaticClass(), NAME_None, RF_Standalone);
        Support->AddToRoot();
        Support->ClassAssetId = TEXT("Support");
        Support->ClassId = EBreakerClassId::Support;
        Support->DisplayName = LOCTEXT("SupportName", "Support");
        Support->Description = LOCTEXT("SupportDescription",
            "Charge: banked by healing, shielding, buff uptime and marked-target damage. Solo pays exactly what a party pays, source for source.");
        // Starters first (Patch / Mark, Class-Kits-Support §3). The row is in
        // Data/class-kits.json.
        BreakerClassKitApply(*Support);
        // Class-Kits-Support §4 order: Medic, Conductor, Warden.
        Support->BranchTrees.Add(GetSupportMedicTree());
        Support->BranchTrees.Add(GetSupportConductorTree());
        Support->BranchTrees.Add(GetSupportWardenTree());
        Support->BranchTrees.Add(GetCoreSliceTree());
        return Support;
    }

    if (ClassId == EBreakerClassId::Caster)
    {
        static UBreakerClassDefinition* Caster = nullptr;
        if (Caster) return Caster;

        Caster = NewObject<UBreakerClassDefinition>(GetTransientPackage(), UBreakerClassDefinition::StaticClass(), NAME_None, RF_Standalone);
        Caster->AddToRoot();
        Caster->ClassAssetId = TEXT("Caster");
        Caster->ClassId = EBreakerClassId::Caster;
        Caster->DisplayName = LOCTEXT("CasterName", "Caster");
        Caster->Description = LOCTEXT("CasterDescription",
            "Mana: a resource that starts full, spends down per cast, and regenerates -- with Overcast allowing a temporary debt.");
        // THE FIX ITSELF. UBreakerProgressionComponent::IsAbilityUnlocked
        // answers only from ClassDefinition->BaseUltimateId, its starters or
        // the character's unlocked set (or a purchased node's
        // GrantedAbilityIds),
        // and this function returned nullptr for every class but Swift, so a
        // Caster's ClassDefinition was null and EVERY Caster ability read as
        // locked no matter what UBreakerAbilityComponent::TryEquipAbility
        // asked for (see its own comment at the call site). The catalogue
        // below lists all SEVEN ids the ability registry actually implements
        // (Abilities/BreakerAbilityDefinition.cpp), mirrored exactly. STILL
        // TRUE after Spellblade/Void Whisperer/Multispell were authored below:
        // gating any of these five behind an unpurchased tree node would
        // repeat the exact "grants nothing reachable" failure this comment
        // was written to fix, and would un-equip
        // Tests/BreakerProgressionAuditTests.cpp's CasterAbilitiesUnlockTest,
        // which equips all seven with zero node purchases. The new branch
        // trees' Tier-3 nodes therefore carry their OTHER Class-Kits content
        // (the rule-rewrite half of "Grants X") and leave the grant itself
        // exactly as catalogued here.
        // Class-Kits §2.2 starters first; the row is in Data/class-kits.json.
        BreakerClassKitApply(*Caster);
        // O39's "honest emptiness" is closed: Spellblade, Void Whisperer and
        // Multispell are now authored (Class-Kits §2.3-2.5), so BranchTrees
        // is populated exactly like Swift's, Core tree included. The seven
        // ability ids above are UNCHANGED by this -- see the block comment on
        // GetCasterSpellbladeTree() for why the Tier-3 "Grants" nodes below
        // do not also re-grant them.
        Caster->BranchTrees.Add(GetCasterSpellbladeTree());
        Caster->BranchTrees.Add(GetCasterVoidWhispererTree());
        Caster->BranchTrees.Add(GetCasterMultispellTree());
        Caster->BranchTrees.Add(GetCoreSliceTree());
        return Caster;
    }

    // Anything else (None, a future entry) has no kit and gets no row.
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
    //
    // O258: Slipcut is the sole free starter. Slot two stays empty until an
    // unlock is equipped; the data row drives the remaining token entitlement.
    BreakerClassKitApply(*Swift);
    // Class-Kits §1.3-1.5 order: Frenzy, Kinetic, Marksman. The branch strip
    // reads this list, so it now shows the three chips the design names.
    Swift->BranchTrees.Add(GetSwiftFrenzyTree());
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
