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
    TestTrue(TEXT("inactive current migration accepts legacy layout"),UBreakerSaveGame::MigrateToCurrent(*Loaded,Note));
    TestEqual(TEXT("inactive feature leaves Core allocated"),Loaded->Progression.CoreNodeRanks.Num(),3);
    TestEqual(TEXT("inactive feature leaves wallet untouched"),Loaded->Progression.UnspentCorePoints,4);
    TestTrue(TEXT("explicit new layout migration succeeds"),UBreakerSaveGame::MigrateCoreLayout(*Loaded,2,Note));
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
    // Snapshot conformance is only meaningful while the legacy roster is live.
    if (UBreakerSaveGame::ActiveCoreLayoutVersion==1)
        for (const UBreakerProgressionNode* Node:UBreakerProgressionLibrary::GetCoreSliceTree()->Nodes)
            TestEqual(*FString::Printf(TEXT("frozen legacy cost %s"),*Node->NodeId.ToString()),UBreakerSaveGame::LegacyCoreRankCost(Node->NodeId),Node->CostPerRank);
    return true;
}
#endif
