#ifndef AUTOLAYOUT_H
#define AUTOLAYOUT_H

#include "graph.h"
#include "layoutconfig.h"
#include "edgerouter.h"

#include <QMap>
#include <QPointF>
#include <QSet>

struct AutoLayoutResult {
    QMap<Node*, QPointF> newPositions;
    QMap<Edge*, EdgeRouting> newRouting;
    bool success = false;
};

class AutoLayout
{
public:
    static AutoLayoutResult run(Graph *graph, const LayoutConfig &config);
};

#endif // AUTOLAYOUT_H
