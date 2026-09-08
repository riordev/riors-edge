#include "Misc/AutomationTest.h"
#include "Save/BreakerSaveGame.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FBreakerLegacyCoreArchive : public FObjectAndNameAsStringProxyArchive
{
public:
    explicit FBreakerLegacyCoreArchive(FArchive& Inner) : FObjectAndNameAsStringProxyArchive(Inner,false) {}
    virtual bool ShouldSkipProperty(const FProperty* Property) const override
    {
        return Property && Property->GetFName()==FName(TEXT("CoreLayoutVersion"))
            ? true : FObjectAndNameAsStringProxyArchive::ShouldSkipProperty(Property);
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreMigrationTest,"RiorsEdge.Save.CoreLayoutMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreMigrationTest::RunTest(const FString& Parameters)
{
    auto* Save=NewObject<UBreakerSaveGame>();
    Save->SaveVersion=UBreakerSaveGame::CurrentSaveVersion;
    Save->Progression.UnspentCorePoints=4;
    Save->Progression.CoreNodeRanks={{TEXT("Core.Precision.Sightline"),1},{TEXT("Core.Precision.CalledShot"),1},{TEXT("Core.Precision.Fixate"),1}};
    Save->Progression.UnspentDoctrinePoints=3;
    Save->Progression.DoctrineNodeRanks={{TEXT("Support.Warden.Painted"),2}};
    Save->Progression.PermanentClass=EBreakerClassId::Support;
    Save->Progression.LevelCorePointsGranted=10;
    Save->QuestFlags.Add(TEXT("Quest.FirstContract.Accepted"));
    Save->QuestCounters.Add(TEXT("fixture.counter"),7);
    Save->DiscoveredMapSites.Add(TEXT("fernhall.maintenance"));
    Save->CharacterId=FGuid::NewGuid();
    Save->BackpackItems.Add(UBreakerLootLibrary::RollItem(TEXT("Migration.Item"),EBreakerEquipSlot::Helmet,EBreakerItemRarity::Standard,1,7));
    const FGuid Identity=Save->CharacterId;
    // Serialize an old payload with the new property genuinely absent.
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes);
    FBreakerLegacyCoreArchive OldWriter(Writer);
    Save->Serialize(OldWriter);
    auto* Loaded=NewObject<UBreakerSaveGame>();
    FMemoryReader Reader(Bytes);
    FObjectAndNameAsStringProxyArchive ReadArchive(Reader,false);
    Loaded->Serialize(ReadArchive);
    TestEqual(TEXT("missing legacy layout property defaults to one"),Loaded->CoreLayoutVersion,1);
    FString Note;
    TestEqual(TEXT("Replacement layout is active"),UBreakerSaveGame::ActiveCoreLayoutVersion,2);
    if (!TestTrue(TEXT("Normal load migrates legacy Core automatically"),UBreakerSaveGame::MigrateToCurrent(*Loaded,Note))) return false;
    TestEqual(TEXT("exact costs one plus two plus three refunded"),Loaded->Progression.UnspentCorePoints,10);
    TestEqual(TEXT("old meaning cannot survive through reused IDs"),Loaded->Progression.CoreNodeRanks.Num(),0);
    TestEqual(TEXT("layout stamped after success"),Loaded->CoreLayoutVersion,2);
    TestTrue(TEXT("replay succeeds without another refund"),UBreakerSaveGame::MigrateCoreLayout(*Loaded,2,Note));
    TestEqual(TEXT("replay preserves exact wallet"),Loaded->Progression.UnspentCorePoints,10);
    TestEqual(TEXT("Doctrine wallet preserved"),Loaded->Progression.UnspentDoctrinePoints,3);
    TestEqual(TEXT("Doctrine allocation preserved"),Loaded->Progression.DoctrineNodeRanks[0].Rank,2);
    TestEqual(TEXT("class preserved"),Loaded->Progression.PermanentClass,EBreakerClassId::Support);
    TestEqual(TEXT("earned Core counter preserved"),Loaded->Progression.LevelCorePointsGranted,10);
    TestTrue(TEXT("identity preserved"),Loaded->CharacterId==Identity);
    TestEqual(TEXT("backpack preserved"),Loaded->BackpackItems.Num(),1);
    TestTrue(TEXT("exact item identity preserved"),Loaded->BackpackItems[0].ItemId==Save->BackpackItems[0].ItemId);
    TestTrue(TEXT("quest flags preserved"),Loaded->QuestFlags==Save->QuestFlags);
    TestTrue(TEXT("quest counters preserved"),Loaded->QuestCounters.OrderIndependentCompareEqual(Save->QuestCounters));
    TestTrue(TEXT("map discovery preserved"),Loaded->DiscoveredMapSites==Save->DiscoveredMapSites);
    Save->Progression.CoreNodeRanks.Add({TEXT("Core.Unknown.Future"),1});
    TestFalse(TEXT("unknown legacy allocation refuses atomically"),UBreakerSaveGame::MigrateCoreLayout(*Save,2,Note));
    TestEqual(TEXT("refusal preserves all ranks"),Save->Progression.CoreNodeRanks.Num(),4);
    TestEqual(TEXT("refusal preserves wallet"),Save->Progression.UnspentCorePoints,4);
    TestEqual(TEXT("refusal does not stamp layout"),Save->CoreLayoutVersion,1);
    // Frozen prices are independent of the newly authored meanings. In
    // particular, Deepen changed from a one-point rim to a two-point notable.
    TestEqual(TEXT("Frozen gateway cost"),UBreakerSaveGame::LegacyCoreRankCost(TEXT("Core.Precision.Sightline")),1);
    TestEqual(TEXT("Frozen notable cost"),UBreakerSaveGame::LegacyCoreRankCost(TEXT("Core.Precision.CalledShot")),2);
    TestEqual(TEXT("Frozen convergence cost"),UBreakerSaveGame::LegacyCoreRankCost(TEXT("Core.Precision.Fixate")),3);
    TestEqual(TEXT("Frozen Deepen price is not repriced from live roster"),UBreakerSaveGame::LegacyCoreRankCost(TEXT("Core.Affliction.Deepen")),1);
    const auto* Core=UBreakerProgressionLibrary::GetCoreSliceTree();
    if (!TestNotNull(TEXT("Replacement Core is live"),Core)) return false;
    TestEqual(TEXT("Live getter exposes every accepted wedge"),Core->CoreWedgeOrder.Num(),22);
    TestEqual(TEXT("Live getter exposes every accepted node"),Core->Nodes.Num(),187);
    const auto* Linger=Core->FindNode(TEXT("Core.Affliction.Linger"));
    if (!TestNotNull(TEXT("Reused Linger identity resolves in replacement"),Linger)) return false;
    TestEqual(TEXT("Replacement Linger has its authored rank price"),Linger->CostPerRank,1);
    TestEqual(TEXT("Replacement Linger has three ranks"),Linger->MaxRank,3);

    const auto* Deepen=Core->FindNode(TEXT("Core.Affliction.Deepen"));
    if (!TestNotNull(TEXT("Changed-price Deepen identity resolves"),Deepen)) return false;
    TestEqual(TEXT("Replacement Deepen costs two"),Deepen->CostPerRank,2);
    auto* RepricedLegacy=NewObject<UBreakerSaveGame>();
    RepricedLegacy->SaveVersion=UBreakerSaveGame::CurrentSaveVersion;
    RepricedLegacy->Progression.CoreNodeRanks={{TEXT("Core.Affliction.Deepen"),1}};
    if (!TestTrue(TEXT("Normal migration refunds a changed-price legacy ID"),UBreakerSaveGame::MigrateToCurrent(*RepricedLegacy,Note))) return false;
    TestEqual(TEXT("Refund uses frozen one-point cost rather than live two"),RepricedLegacy->Progression.UnspentCorePoints,1);

    // A current-format allocation must survive serialization and ordinary
    // loading unchanged. These saved rows are the migration fixture subject;
    // acquisition and budget legality are covered by the paid roster tests.
    auto* Current=NewObject<UBreakerSaveGame>();
    Current->SaveVersion=UBreakerSaveGame::CurrentSaveVersion;
    Current->CoreLayoutVersion=UBreakerSaveGame::ActiveCoreLayoutVersion;
    Current->Progression.UnspentCorePoints=4;
    Current->Progression.CoreNodeRanks={{TEXT("Core.Affliction.OpenWound"),1},{TEXT("Core.Affliction.Linger"),3}};
    TArray<uint8> CurrentBytes;
    FMemoryWriter CurrentWriter(CurrentBytes);
    FObjectAndNameAsStringProxyArchive CurrentWriteArchive(CurrentWriter,false);
    Current->Serialize(CurrentWriteArchive);
    auto* CurrentLoaded=NewObject<UBreakerSaveGame>();
    FMemoryReader CurrentReader(CurrentBytes);
    FObjectAndNameAsStringProxyArchive CurrentReadArchive(CurrentReader,false);
    CurrentLoaded->Serialize(CurrentReadArchive);
    TestEqual(TEXT("Explicit layout survives serialization"),CurrentLoaded->CoreLayoutVersion,2);
    if (!TestTrue(TEXT("Current-format load succeeds without refund"),UBreakerSaveGame::MigrateToCurrent(*CurrentLoaded,Note))) return false;
    TestEqual(TEXT("Current wallet is unchanged"),CurrentLoaded->Progression.UnspentCorePoints,4);
    if (!TestEqual(TEXT("Current allocation retains both rows"),CurrentLoaded->Progression.CoreNodeRanks.Num(),2)) return false;
    TestEqual(TEXT("Current ranked identity is unchanged"),CurrentLoaded->Progression.CoreNodeRanks[1].NodeId,FName(TEXT("Core.Affliction.Linger")));
    TestEqual(TEXT("Current three-rank investment survives"),CurrentLoaded->Progression.CoreNodeRanks[1].Rank,3);
    return true;
}
#endif
