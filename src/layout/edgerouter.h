#ifndef EDGEROUTER_H
#define EDGEROUTER_H

#include "graph.h"
#include "layoutconfig.h"

#include <QMap>
#include <QPointF>

struct EdgeRouting {
    bool basicBendMode = true;
    int bend = 0;
    int inAngle = 0;
    int outAngle = 0;
    qreal weight = 0.4;
};

class EdgeRouter
{
public:
    EdgeRouter(Graph *graph, const QMap<Node*, QPointF> &positions,
               const LayoutConfig &config);

    QMap<Edge*, EdgeRouting> run();

private:
    bool lineIntersectsNodeBox(QPointF from, QPointF to, QPointF nodePos, qreal margin);
    bool straightPathClear(Edge *e);
    int findClearBend(Edge *e);

    Graph *_graph;
    QMap<Node*, QPointF> _positions;
    LayoutConfig _config;
};

#endif // EDGEROUTER_H
