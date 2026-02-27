#ifndef LAYOUTEVAL_H
#define LAYOUTEVAL_H

#include "graph.h"

#include <QMap>
#include <QPointF>
#include <QRectF>

class LayoutEval
{
public:
    static int countCrossings(Graph *graph, const QMap<Node*, QPointF> &positions);
    static int countOverlaps(Graph *graph, const QMap<Node*, QPointF> &positions, qreal margin);
    static qreal computeWirelength(Graph *graph, const QMap<Node*, QPointF> &positions);
    static qreal computeArea(const QMap<Node*, QPointF> &positions);
    static qreal weightedScore(int crossings, int overlaps, qreal area, qreal wirelength);

private:
    static bool segmentsIntersect(QPointF p1, QPointF p2, QPointF p3, QPointF p4);
    static qreal cross2D(QPointF a, QPointF b);
};

#endif // LAYOUTEVAL_H
