#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NativeGameplayTags.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerProgressionLibrary.generated.h"

class UBreakerClassDefinition;
class UBreakerProgressionNode;
class UBreakerProgressionTree;

// Tags published by rule-rewrite and verb-grant nodes. Native so the
// fallback trees work with no .ini content, exactly like the weapon
// prototype archetypes work with no Data Assets.
namespace BreakerNodeTags
{
    // Core constellation slice
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Fixate);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TunnelVision);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TriggerDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Cyclic);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_LastRound);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_OpenWound);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SetStance);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Read);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Loft);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_PhantomStep);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Parry);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_AirJump);

    // Swift / Kinetic
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_ReadTheRoom);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Contact);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Carry);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Redirect);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_EvadeConversion);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Landing);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SkimDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AirWork);
    // Kinetic tier 4 (K9-K11). Every one of these is a rule the Momentum loop,
    // the damage-taken path or UBreakerAbility_Skim must learn to read; none
    // authors a stat effect. See the block comment above GetSwiftKineticTree.
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MomentumShield);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SpendToLive);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_NoGround);

    // Swift / Marksman
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_LongLens);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Steady);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Ledger);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Angle);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MarkEconomy);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_PierceDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Sightline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Lead);
    // Marksman tier 4 (M9-M11). Node_MarksmanCalledShot is a DIFFERENT node
    // from Core's Node_CalledShot and shares only a display name — see the tag
    // definition in the .cpp for why neither is renamed.
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Reserve);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Overpenetration);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MarksmanCalledShot);

    // Swift / FRENZY (Class-Kits §1.3). The branch the design document has
    // always named and the library never authored, which is why the skill
    // matrix's branch strip showed two chips where the design says three.
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FrenzyTrigger);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Loaded);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_ShortLeash);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Rhythm);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_DryFire);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Feed);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Overrev);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SlipcutMastery);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AmmunitionEconomy);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Bloodrhythm);
    // Frenzy tier 4 (F9-F11).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SecondWind);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_RedlineTrigger);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_NoSafety);

    // Core / ELEMENTS (Core-Constellations §6, O5 + O19: Rift / Entropy / Void).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Conductive);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_ChargeUp);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Threshold);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Catalyst);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Penetrance);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_ReactionChain);

    // Caster / SPELLBLADE (Class-Kits §2.3). Tiers 1-3 only — SB9-SB11's
    // tier-4 rewrites are not authored. This WAS the same cut Swift used; it
    // no longer is, since Swift's nine tier-4 rewrites have since landed. The
    // Caster trio is the remaining half of that gap, not a settled shape.
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_ContactCharge);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_FollowThrough);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_Close);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_Debt);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_MomentumTransfer);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_Bloodprice);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_Blink);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_Edge);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SB_Edgework);

    // Caster / VOID WHISPERER (Class-Kits §2.4).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_Seep);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_StandingWater);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_Patience);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_Lingering);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_Attrition);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_Drain);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_Zonework);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_Wellspring);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_VW_LongDark);

    // Caster / MULTISPELL (Class-Kits §2.5).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Variance);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Cycle);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Reservoir);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Chain);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Payment);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Sequence);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Fracture);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Resonance);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MS_Cascade);

    // Gunsmith / ARMORY (Class-Kits-Gunsmith §4.1). Authored 2026-08-16 under
    // the owner's "do all 5 classes" ruling — the branch layer the kits-playable
    // pass deliberately left out.
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_FieldStripping);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_WorkingStock);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_Chambered);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_DeepPockets);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_LastRound);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_ColdBarrel);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_BenchWork);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_RigDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_Reciprocal);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_Overpressure);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_NoReserve);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AR_Machinist);

    // Gunsmith / FIELD TECH (Class-Kits-Gunsmith §4.2).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Salvage);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Overwatch);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_SecondShift);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Tithe);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Requisition);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Foreman);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Emplacement);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Logistics);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Redundancy);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Automation);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Deadman);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_FT_Foundry);

    // Gunsmith / TINKERER (Class-Kits-Gunsmith §4.3).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_CheapWork);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_QuickSet);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_Tripwire);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_Rearm);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_AttritionField);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_Overlap);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_Ordnance);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_Interdiction);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_Patience);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_DeadGround);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_CommandDetonation);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TK_Minefield);

    // Tank / LEECH (Class-Kits-Tank §3).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_Clot);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_SlowBleed);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_OpenWound);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_FeedTheWound);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_Bloodlet);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_Transfusion);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_RendMastery);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_SecondHeart);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_NothingWasted);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_Reciprocity);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_Exsanguinate);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_L_Vein);

    // Tank / BASTION (Class-Kits-Tank §4).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_LineOfSight);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_Footing);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_Loud);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_HeldGround);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_AnsweringFire);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_Bulk);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_Emplacement);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_Interposition);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_Conversion);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_StandingOrder);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_ImmovableObject);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_B_Wall);

    // Tank / DEMOLITIONIST (Class-Kits-Tank §5).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_ShapedCharge);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_Bootstraps);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_BracedForImpact);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_Fragmentation);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_Concussion);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_Overpressure);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_Demolition);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_TerminalDescent);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_BlastRadius);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_KineticRecovery);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_ChainReaction);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_D_Detonation);

    // Support / MEDIC (Class-Kits-Support §4.1).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_FieldDressing);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_TriagePriority);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_CleanHands);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_SteadyHands);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_SecondOpinion);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_Attending);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_FieldKit);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_SustainedCare);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_Overflow);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_BloodDebt);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_NoTriage);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MD_Triage);

    // Support / CONDUCTOR (Class-Kits-Support §4.2).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_DownbeatDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Section);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Sustain);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Rehearsal);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Tempo);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Attunement);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Conducting);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Counterpoint);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_StandingOvation);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_SympatheticResonance);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_DetachedBaton);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_CO_Downbeat);

    // Support / WARDEN (Class-Kits-Support §4.3).
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_Painted);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_LongWatch);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_FieldOfView);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_Handoff);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_Pressure);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_Tell);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_Suppression);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_DeepMark);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_ExecutionersLedger);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_BlackoutProtocol);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_HuntersEconomy);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_WA_Blackout);
}

// Zero-setup fallback skill-tree content, the same convention the weapon
// prototypes use: real runtime trees exist in C++ so the slice is playable
// before any Data Asset is authored, and the Data Assets replace them
// one-for-one later (Core-Constellations §10.3 criterion 1 is a shipping
// requirement, not a slice-prototype requirement).
//
// EVERY MAGNITUDE HERE IS AN O2 PLACEHOLDER. Wave-mode instrumentation has
// not reported; no value below is tuned and none may be signed off.
UCLASS()
class RIORSEDGE_API UBreakerProgressionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The ~15-node Core constellation slice subset (Core-Constellations §10.1).
    static UBreakerProgressionTree* GetCoreSliceTree();
    // Swift's three branches now carry tiers 1-4: the slice's tiers 1-3 plus
    // the Tier-4 rewrite trio each branch's Class-Kits table specifies. The
    // keystones still sit at tier 3 under the slice's compressed ladder rather
    // than §0.2's tier 5, which makes a keystone reachable earlier than its own
    // branch's rewrites — recorded, not fixed, above GetSwiftKineticTree.
    // Caster's three branches remain tiers 1-3.
    //
    // Swift KINETIC branch, tiers 1-4 (Class-Kits §1.4).
    static UBreakerProgressionTree* GetSwiftKineticTree();
    // Swift MARKSMAN branch, tiers 1-4 (Class-Kits §1.5).
    static UBreakerProgressionTree* GetSwiftMarksmanTree();
    // Swift FRENZY branch, tiers 1-4 (Class-Kits §1.3).
    static UBreakerProgressionTree* GetSwiftFrenzyTree();

    // Caster SPELLBLADE branch, tiers 1-3 (Class-Kits §2.3).
    static UBreakerProgressionTree* GetCasterSpellbladeTree();
    // Caster VOID WHISPERER branch, tiers 1-3 (Class-Kits §2.4).
    static UBreakerProgressionTree* GetCasterVoidWhispererTree();
    // Caster MULTISPELL branch, tiers 1-3 (Class-Kits §2.5).
    static UBreakerProgressionTree* GetCasterMultispellTree();

    // Gunsmith / Tank / Support branches, authored 2026-08-16 (owner
    // authorization: "feel free to do all 5 classes" + "keep building").
    // Tiers 1-4 plus the keystone compressed onto tier 3, the Swift shape —
    // see the block comment above GetGunsmithArmoryTree for the compression
    // citation these nine trees share.
    //
    // Gunsmith ARMORY branch (Class-Kits-Gunsmith §4.1).
    static UBreakerProgressionTree* GetGunsmithArmoryTree();
    // Gunsmith FIELD TECH branch (Class-Kits-Gunsmith §4.2).
    static UBreakerProgressionTree* GetGunsmithFieldTechTree();
    // Gunsmith TINKERER branch (Class-Kits-Gunsmith §4.3).
    static UBreakerProgressionTree* GetGunsmithTinkererTree();
    // Tank LEECH branch (Class-Kits-Tank §3).
    static UBreakerProgressionTree* GetTankLeechTree();
    // Tank BASTION branch (Class-Kits-Tank §4).
    static UBreakerProgressionTree* GetTankBastionTree();
    // Tank DEMOLITIONIST branch (Class-Kits-Tank §5).
    static UBreakerProgressionTree* GetTankDemolitionistTree();
    // Support MEDIC branch (Class-Kits-Support §4.1).
    static UBreakerProgressionTree* GetSupportMedicTree();
    // Support CONDUCTOR branch (Class-Kits-Support §4.2).
    static UBreakerProgressionTree* GetSupportConductorTree();
    // Support WARDEN branch (Class-Kits-Support §4.3).
    static UBreakerProgressionTree* GetSupportWardenTree();

    // Core tree plus every class tree. The UI enumerates from here.
    UFUNCTION(BlueprintPure, Category="Progression|Content")
    static const TArray<UBreakerProgressionTree*>& GetAllFallbackTrees();

    // Trees a character of this class can spend in (core tree included).
    UFUNCTION(BlueprintPure, Category="Progression|Content")
    static TArray<UBreakerProgressionTree*> GetTreesForClass(EBreakerClassId ClassId);

    // Fallback class definition so a component with no Data Asset can still
    // resolve branch trees, starting abilities, and the ultimate.
    static UBreakerClassDefinition* GetFallbackClassDefinition(EBreakerClassId ClassId);

    UFUNCTION(BlueprintPure, Category="Progression|Content")
    static const UBreakerProgressionNode* FindFallbackNode(FName NodeId);

    // XP-And-Pacing §9: 10 Class Points and 12 Core Points at slice cap 10.
    // O2 PLACEHOLDER — the shipping values come from the curve Data Asset.
    //
    // SEMANTICS CHANGED with the per-level economy below: the slice grant is
    // now an ADVANCE on the level entitlement, not a bonus beside it. A fresh
    // character's granted-counters are seeded to these values, so levels 1-10
    // pay nothing extra (they were pre-paid here) and level 11 pays the 11th
    // Class Point. At the doc's own milestones the totals agree exactly:
    // 10 Class at slice cap 10, 30 Class at 30, 50 Core at 50 (the Core
    // advance's extra 2 stands in for §7's first world-content grants).
    // RETIRED TO ZERO (O111). The slice lump advanced 10 Class Points against
    // the levelling entitlement; there is no Class Point to advance. It is NOT
    // folded into SliceCorePointGrant -- O27 moves power into node choices
    // rather than into a larger budget. The symbol stays because three call
    // sites seed retired save counters from it and zero is correct for all
    // three.
    static constexpr int32 SliceClassPointGrant = 0;
    static constexpr int32 SliceCorePointGrant = 12;

    // XP-And-Pacing §4: one Class Point per level, exhausted at 30; one Core
    // Point per level, exhausted at 50. Transcribed from the doc (the doc's
    // values, not invented here); O2 PLACEHOLDER only in the sense that §8.9
    // wants them read from a Data Asset eventually.
    // RETIRED (O111). No level pays a Class Point any more. The symbol is kept
    // so a reader searching for the old entitlement finds why it is gone rather
    // than finding nothing.
    static constexpr int32 ClassPointCapLevel = 0;
    // O111: the doctrine pool, paid WHOLE at commitment and only at the Forge.
    // Not a per-level entitlement, which is why no granted-counter exists for
    // it -- there is nothing to settle up against and nothing to double-pay.
    static constexpr int32 DoctrinePointGrant = 8;
    static constexpr int32 CorePointCapLevel = 50;
    // O111's Core pool, WHOLE: one point per level to CorePointCapLevel, plus
    // the world-content grants. This is the number every Core tree is measured
    // against — the offered-to-spendable floor, the atlas's own size, and
    // RiorsEdge.Progression.TreeDepthIsReachable.
    //
    // IT EXISTED ONLY AS A LITERAL IN Scripts/status.py UNTIL NOW, which is the
    // same shape as the defect that made this constant necessary: a gate and
    // the budget it gates against are one number in two places, and the two
    // drift the moment one of them is ruled on. status.py parses this
    // declaration rather than restating it, so the report and the game cannot
    // disagree about how many points a character has.
    static constexpr int32 CoreWorldPointGrant = 15;
    static constexpr int32 CorePointBudget = CorePointCapLevel + CoreWorldPointGrant;

    // O100: ABILITY UNLOCK TOKENS. One token per unlockable ability, paid on an
    // authored level schedule.
    //
    // AN ADVANCE ON THE EVENTUAL WORLD-CONTENT ENTITLEMENT, exactly as
    // SliceClassPointGrant above is an advance on the levelling entitlement.
    // The campaign is post-slice, so the milestones that will eventually hand
    // these out do not exist; a token with no source would be a screen the
    // player can open and never use, which is the reachability rule's own
    // example. When the campaign lands, these levels are replaced by the
    // milestones and the cumulative counter means no character is paid twice.
    // O2 PLACEHOLDER: the four levels are shape, not balance.
    //
    // TRUNCATED PER CLASS to UnlockableAbilityIds.Num(). A flat four would pay
    // Swift four tokens against one unlockable and strand three of them
    // permanently on a shipped class — the same kind of unreachable content
    // this whole pass exists to delete. Swift pays one today and grows to four
    // the day its three missing abilities land, with nothing here to edit.
    static constexpr int32 AbilityTokenLevels[] = {5, 12, 20, 30};

    // How many tokens a character of this level and this many unlockables has
    // earned in total. The component pays the difference against what it has
    // already paid; see UBreakerProgressionComponent::GrantAbilityTokens.
    static int32 AbilityTokenEntitlement(int32 CharacterLevel, int32 UnlockableCount);
};
