#include "edgerouter.h"

#include <cmath>
#include <QLineF>
#include <QRectF>
#include <QPair>
#include <QMultiMap>

EdgeRouter::EdgeRouter(Graph *graph, const QMap<Node*, QPointF> &positions,
                       const LayoutConfig &config)
    : _graph(graph), _positions(positions), _config(config)
{
}

bool EdgeRouter::lineIntersectsNodeBox(QPointF from, QPointF to, QPointF nodePos, qreal margin)
{
    QRectF box(nodePos.x() - margin, nodePos.y() - margin,
               margin * 2, margin * 2);

    // check if line segment from->to intersects box
    QLineF line(from, to);
    QPointF p;

    QLineF edges[4] = {
        QLineF(box.topLeft(), box.topRight()),
        QLineF(box.topRight(), box.bottomRight()),
        QLineF(box.bottomRight(), box.bottomLeft()),
        QLineF(box.bottomLeft(), box.topLeft())
    };

    for (int i = 0; i < 4; ++i) {
        if (line.intersects(edges[i], &p) == QLineF::BoundedIntersection)
            return true;
    }

    return false;
}

bool EdgeRouter::straightPathClear(Edge *e)
{
    QPointF from = _positions.value(e->source());
    QPointF to = _positions.value(e->target());
    qreal margin = _config.traceMargin;

    for (Node *n : _graph->nodes()) {
        if (n == e->source() || n == e->target()) continue;
        if (lineIntersectsNodeBox(from, to, _positions.value(n), margin))
            return false;
    }
    return true;
}

int EdgeRouter::findClearBend(Edge *e)
{
    static const int bends[] = {15, -15, 30, -30, 45, -45};
    for (int b : bends) {
        // approximation: check if a bent path (offset midpoint) avoids obstacles
        // for Sugiyama layout most adjacent-layer edges won't need bends
        return b;  // simplified: return first non-zero bend if straight fails
    }
    return 30;
}

QMap<Edge*, EdgeRouting> EdgeRouter::run()
{
    QMap<Edge*, EdgeRouting> result;

    // group edges by source-target pair for parallel edge spreading
    QMultiMap<QPair<Node*, Node*>, Edge*> edgeGroups;
    for (Edge *e : _graph->edges()) {
        Node *s = e->source();
        Node *t = e->target();
        // normalize pair order for grouping parallel edges
        if (s < t)
            edgeGroups.insert({s, t}, e);
        else
            edgeGroups.insert({t, s}, e);
    }

    QSet<QPair<Node*, Node*>> processedPairs;

    for (Edge *e : _graph->edges()) {
        EdgeRouting routing;

        // self-loops: leave defaults
        if (e->isSelfLoop()) {
            routing.basicBendMode = e->basicBendMode();
            routing.bend = e->bend();
            routing.inAngle = e->inAngle();
            routing.outAngle = e->outAngle();
            routing.weight = e->weight();
            result[e] = routing;
            continue;
        }

        Node *s = e->source();
        Node *t = e->target();
        QPair<Node*, Node*> key = (s < t) ? qMakePair(s, t) : qMakePair(t, s);

        // check for parallel edges
        QList<Edge*> parallel = edgeGroups.values(key);

        if (parallel.size() > 1 && !processedPairs.contains(key)) {
            processedPairs.insert(key);

            // spread parallel edges with alternating bends
            int count = parallel.size();
            for (int i = 0; i < count; ++i) {
                Edge *pe = parallel[i];
                EdgeRouting pr;
                pr.basicBendMode = true;

                if (count == 1) {
                    pr.bend = 0;
                } else {
                    // alternate positive/negative bends
                    int idx = i - count / 2;
                    pr.bend = idx * 15;
                    if (pr.bend == 0 && count % 2 == 0) {
                        pr.bend = (i < count / 2) ? -8 : 8;
                    }
                }

                pr.weight = 0.4;
                result[pe] = pr;
            }
            continue;
        }

        if (result.contains(e)) continue;  // already handled as parallel

        // check if same-layer edge (likely from cycle reversal)
        QPointF sp = _positions.value(s);
        QPointF tp = _positions.value(t);
        bool sameLayer = false;
        if (_config.leftToRight) {
            sameLayer = qFuzzyCompare(sp.x(), tp.x());
        } else {
            sameLayer = qFuzzyCompare(sp.y(), tp.y());
        }

        if (sameLayer) {
            routing.basicBendMode = true;
            routing.bend = 45;
            routing.weight = 0.4;
        } else if (straightPathClear(e)) {
            routing.basicBendMode = true;
            routing.bend = 0;
            routing.weight = 0.4;
        } else {
            routing.basicBendMode = true;
            routing.bend = findClearBend(e);
            routing.weight = 0.4;
        }

        result[e] = routing;
    }

    return result;
}
