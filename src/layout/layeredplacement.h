#ifndef LAYEREDPLACEMENT_H
#define LAYEREDPLACEMENT_H

#include "graph.h"
#include "layoutconfig.h"

#include <QMap>
#include <QSet>
#include <QVector>
#include <QPointF>

class LayeredPlacement
{
public:
    LayeredPlacement(Graph *graph, const LayoutConfig &config,
                     const QSet<Node*> &lockedNodes);

    QMap<Node*, QPointF> run();

private:
    void removeCycles();
    void assignLayers();
    void minimizeCrossings();
    void assignCoordinates();

    void dfs(Node *node, QSet<Node*> &visited, QSet<Node*> &onStack);
    int longestPathFrom(Node *node, QMap<Node*, int> &memo);
    qreal barycenter(Node *node, const QVector<Node*> &adjacentLayer, bool forward);
    void findConnectedComponents(QVector<QVector<Node*>> &components);

    Graph *_graph;
    LayoutConfig _config;
    QSet<Node*> _lockedNodes;
    QSet<Edge*> _reversedEdges;

    QMap<Node*, int> _layerAssignment;
    QVector<QVector<Node*>> _layers;
    QMap<Node*, QPointF> _positions;

    // adjacency helpers
    QMap<Node*, QVector<Edge*>> _outEdges;
    QMap<Node*, QVector<Edge*>> _inEdges;

    void buildAdjacency();
    Node* edgeSource(Edge *e) const;
    Node* edgeTarget(Edge *e) const;
};

#endif // LAYEREDPLACEMENT_H
