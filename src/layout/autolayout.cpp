#include "autolayout.h"
#include "layeredplacement.h"
#include "edgerouter.h"

AutoLayoutResult AutoLayout::run(Graph *graph, const LayoutConfig &config)
{
    AutoLayoutResult result;

    if (!graph || graph->nodes().isEmpty()) {
        result.success = false;
        return result;
    }

    // identify locked nodes
    QSet<Node*> lockedNodes;
    for (Node *n : graph->nodes()) {
        if (n->data() && n->data()->hasProperty("tikzit.locked")) {
            lockedNodes.insert(n);
        }
    }

    // run layered placement
    LayeredPlacement placement(graph, config, lockedNodes);
    result.newPositions = placement.run();

    // run edge router
    EdgeRouter router(graph, result.newPositions, config);
    result.newRouting = router.run();

    result.success = true;
    return result;
}
