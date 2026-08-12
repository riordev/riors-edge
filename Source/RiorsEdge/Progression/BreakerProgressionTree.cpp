#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"

const UBreakerProgressionNode* UBreakerProgressionTree::FindNode(FName NodeId) const
{
    for (const UBreakerProgressionNode* Node : Nodes)
    {
        if (Node && Node->NodeId == NodeId) return Node;
    }
    return nullptr;
}
