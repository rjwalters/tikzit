#include "layouteval.h"

#include <cmath>
#include <QLineF>

qreal LayoutEval::cross2D(QPointF a, QPointF b)
{
    return a.x() * b.y() - a.y() * b.x();
}

bool LayoutEval::segmentsIntersect(QPointF p1, QPointF p2, QPointF p3, QPointF p4)
{
    QPointF d1 = p2 - p1;
    QPointF d2 = p4 - p3;
    QPointF d3 = p3 - p1;

    qreal denom = cross2D(d1, d2);
    if (qFuzzyIsNull(denom)) return false;

    qreal t = cross2D(d3, d2) / denom;
    qreal u = cross2D(d3, d1) / denom;

    return (t > 0.0 && t < 1.0 && u > 0.0 && u < 1.0);
}

int LayoutEval::countCrossings(Graph *graph, const QMap<Node*, QPointF> &positions)
{
    const auto &edges = graph->edges();
    int crossings = 0;

    for (int i = 0; i < edges.size(); ++i) {
        Edge *e1 = edges[i];
        if (e1->isSelfLoop()) continue;
        QPointF s1 = positions.value(e1->source());
        QPointF t1 = positions.value(e1->target());

        for (int j = i + 1; j < edges.size(); ++j) {
            Edge *e2 = edges[j];
            if (e2->isSelfLoop()) continue;

            // skip edges sharing an endpoint
            if (e1->source() == e2->source() || e1->source() == e2->target() ||
                e1->target() == e2->source() || e1->target() == e2->target())
                continue;

            QPointF s2 = positions.value(e2->source());
            QPointF t2 = positions.value(e2->target());

            if (segmentsIntersect(s1, t1, s2, t2))
                ++crossings;
        }
    }

    return crossings;
}

int LayoutEval::countOverlaps(Graph *graph, const QMap<Node*, QPointF> &positions, qreal margin)
{
    const auto &nodes = graph->nodes();
    int overlaps = 0;
    qreal halfW = margin / 2.0;

    for (int i = 0; i < nodes.size(); ++i) {
        QPointF p1 = positions.value(nodes[i]);
        QRectF r1(p1.x() - halfW, p1.y() - halfW, margin, margin);

        for (int j = i + 1; j < nodes.size(); ++j) {
            QPointF p2 = positions.value(nodes[j]);
            QRectF r2(p2.x() - halfW, p2.y() - halfW, margin, margin);

            if (r1.intersects(r2))
                ++overlaps;
        }
    }

    return overlaps;
}

qreal LayoutEval::computeWirelength(Graph *graph, const QMap<Node*, QPointF> &positions)
{
    qreal total = 0.0;
    for (Edge *e : graph->edges()) {
        if (e->isSelfLoop()) continue;
        QPointF s = positions.value(e->source());
        QPointF t = positions.value(e->target());
        total += QLineF(s, t).length();
    }
    return total;
}

qreal LayoutEval::computeArea(const QMap<Node*, QPointF> &positions)
{
    if (positions.isEmpty()) return 0.0;

    qreal minX = 1e18, maxX = -1e18;
    qreal minY = 1e18, maxY = -1e18;

    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        QPointF p = it.value();
        minX = qMin(minX, p.x());
        maxX = qMax(maxX, p.x());
        minY = qMin(minY, p.y());
        maxY = qMax(maxY, p.y());
    }

    return (maxX - minX) * (maxY - minY);
}

qreal LayoutEval::weightedScore(int crossings, int overlaps, qreal area, qreal wirelength)
{
    return crossings * 1e6 + overlaps * 1e4 + area + wirelength * 0.1;
}
