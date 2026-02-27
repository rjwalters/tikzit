#include "layeredplacement.h"

#include <algorithm>
#include <cmath>
#include <QQueue>

LayeredPlacement::LayeredPlacement(Graph *graph, const LayoutConfig &config,
                                   const QSet<Node*> &lockedNodes)
    : _graph(graph), _config(config), _lockedNodes(lockedNodes)
{
}

void LayeredPlacement::buildAdjacency()
{
    _outEdges.clear();
    _inEdges.clear();
    for (Node *n : _graph->nodes()) {
        _outEdges[n] = QVector<Edge*>();
        _inEdges[n] = QVector<Edge*>();
    }
    for (Edge *e : _graph->edges()) {
        if (e->isSelfLoop()) continue;
        _outEdges[edgeSource(e)].append(e);
        _inEdges[edgeTarget(e)].append(e);
    }
}

Node* LayeredPlacement::edgeSource(Edge *e) const
{
    return _reversedEdges.contains(e) ? e->target() : e->source();
}

Node* LayeredPlacement::edgeTarget(Edge *e) const
{
    return _reversedEdges.contains(e) ? e->source() : e->target();
}

// DFS-based cycle removal: mark back edges as reversed
void LayeredPlacement::removeCycles()
{
    QSet<Node*> visited;
    QSet<Node*> onStack;

    // build initial adjacency (before reversal)
    for (Node *n : _graph->nodes()) {
        _outEdges[n] = QVector<Edge*>();
        _inEdges[n] = QVector<Edge*>();
    }
    for (Edge *e : _graph->edges()) {
        if (e->isSelfLoop()) continue;
        _outEdges[e->source()].append(e);
        _inEdges[e->target()].append(e);
    }

    for (Node *n : _graph->nodes()) {
        if (!visited.contains(n)) {
            dfs(n, visited, onStack);
        }
    }

    // rebuild adjacency with reversed edges accounted for
    buildAdjacency();
}

void LayeredPlacement::dfs(Node *node, QSet<Node*> &visited, QSet<Node*> &onStack)
{
    visited.insert(node);
    onStack.insert(node);

    for (Edge *e : _outEdges[node]) {
        Node *target = e->target(); // original target before reversal
        if (e->isSelfLoop()) continue;
        if (onStack.contains(target)) {
            _reversedEdges.insert(e);
        } else if (!visited.contains(target)) {
            dfs(target, visited, onStack);
        }
    }

    onStack.remove(node);
}

// Longest-path layer assignment on the DAG
void LayeredPlacement::assignLayers()
{
    QMap<Node*, int> memo;

    for (Node *n : _graph->nodes()) {
        longestPathFrom(n, memo);
    }

    // find max layer
    int maxLayer = 0;
    for (auto it = memo.constBegin(); it != memo.constEnd(); ++it) {
        maxLayer = qMax(maxLayer, it.value());
    }

    _layerAssignment = memo;

    // for locked nodes, snap to nearest layer based on x position
    if (!_lockedNodes.isEmpty() && maxLayer > 0) {
        // determine extent from unlocked layer assignments
        qreal minX = 1e18, maxX = -1e18;
        for (Node *n : _lockedNodes) {
            qreal x = _config.leftToRight ? n->point().x() : -n->point().y();
            minX = qMin(minX, x);
            maxX = qMax(maxX, x);
        }

        for (Node *n : _lockedNodes) {
            qreal x = _config.leftToRight ? n->point().x() : -n->point().y();
            int layer;
            if (maxLayer == 0) {
                layer = 0;
            } else {
                qreal layerSpan = _config.nodeMarginH;
                layer = qRound(x / layerSpan);
                layer = qBound(0, layer, maxLayer);
            }
            _layerAssignment[n] = layer;
        }
    }

    // build layer vectors
    _layers.clear();
    _layers.resize(maxLayer + 1);
    for (Node *n : _graph->nodes()) {
        int layer = _layerAssignment[n];
        _layers[layer].append(n);
    }
}

int LayeredPlacement::longestPathFrom(Node *node, QMap<Node*, int> &memo)
{
    if (memo.contains(node)) return memo[node];

    int longest = 0;
    for (Edge *e : _outEdges[node]) {
        Node *target = edgeTarget(e);
        if (target == node) continue;
        longest = qMax(longest, 1 + longestPathFrom(target, memo));
    }

    memo[node] = longest;
    return longest;
}

// Barycenter crossing minimization
void LayeredPlacement::minimizeCrossings()
{
    if (_layers.size() <= 1) return;

    int bestCrossings = INT_MAX;
    QVector<QVector<Node*>> bestLayers = _layers;

    for (int iter = 0; iter < _config.maxIterations; ++iter) {
        bool forward = (iter % 2 == 0);

        if (forward) {
            for (int i = 1; i < _layers.size(); ++i) {
                QVector<Node*> &layer = _layers[i];
                QVector<QPair<qreal, Node*>> scored;

                for (Node *n : layer) {
                    if (_lockedNodes.contains(n)) {
                        scored.append({(qreal)layer.indexOf(n), n});
                    } else {
                        qreal bc = barycenter(n, _layers[i - 1], true);
                        scored.append({bc, n});
                    }
                }

                std::sort(scored.begin(), scored.end(),
                          [](const QPair<qreal, Node*> &a, const QPair<qreal, Node*> &b) {
                              return a.first < b.first;
                          });

                layer.clear();
                for (auto &p : scored) layer.append(p.second);
            }
        } else {
            for (int i = _layers.size() - 2; i >= 0; --i) {
                QVector<Node*> &layer = _layers[i];
                QVector<QPair<qreal, Node*>> scored;

                for (Node *n : layer) {
                    if (_lockedNodes.contains(n)) {
                        scored.append({(qreal)layer.indexOf(n), n});
                    } else {
                        qreal bc = barycenter(n, _layers[i + 1], false);
                        scored.append({bc, n});
                    }
                }

                std::sort(scored.begin(), scored.end(),
                          [](const QPair<qreal, Node*> &a, const QPair<qreal, Node*> &b) {
                              return a.first < b.first;
                          });

                layer.clear();
                for (auto &p : scored) layer.append(p.second);
            }
        }

        // count crossings for this arrangement
        int crossings = 0;
        for (int i = 0; i < _layers.size() - 1; ++i) {
            const QVector<Node*> &top = _layers[i];
            const QVector<Node*> &bot = _layers[i + 1];

            QMap<Node*, int> topPos, botPos;
            for (int j = 0; j < top.size(); ++j) topPos[top[j]] = j;
            for (int j = 0; j < bot.size(); ++j) botPos[bot[j]] = j;

            QVector<QPair<int, int>> edgePairs;
            for (Node *n : top) {
                for (Edge *e : _outEdges[n]) {
                    Node *t = edgeTarget(e);
                    if (botPos.contains(t)) {
                        edgePairs.append({topPos[n], botPos[t]});
                    }
                }
            }

            // count inversions
            for (int a = 0; a < edgePairs.size(); ++a) {
                for (int b = a + 1; b < edgePairs.size(); ++b) {
                    if ((edgePairs[a].first - edgePairs[b].first) *
                        (edgePairs[a].second - edgePairs[b].second) < 0)
                        ++crossings;
                }
            }
        }

        if (crossings < bestCrossings) {
            bestCrossings = crossings;
            bestLayers = _layers;
        }

        if (crossings == 0) break;
    }

    _layers = bestLayers;
}

qreal LayeredPlacement::barycenter(Node *node, const QVector<Node*> &adjacentLayer, bool forward)
{
    qreal sum = 0.0;
    int count = 0;

    QMap<Node*, int> pos;
    for (int i = 0; i < adjacentLayer.size(); ++i) pos[adjacentLayer[i]] = i;

    if (forward) {
        // node is in a lower layer, neighbors in the upper (adjacentLayer)
        for (Edge *e : _inEdges[node]) {
            Node *src = edgeSource(e);
            if (pos.contains(src)) {
                sum += pos[src];
                ++count;
            }
        }
    } else {
        // node is in an upper layer, neighbors in the lower (adjacentLayer)
        for (Edge *e : _outEdges[node]) {
            Node *tgt = edgeTarget(e);
            if (pos.contains(tgt)) {
                sum += pos[tgt];
                ++count;
            }
        }
    }

    if (count == 0) return 0.0;
    return sum / count;
}

void LayeredPlacement::findConnectedComponents(QVector<QVector<Node*>> &components)
{
    QSet<Node*> visited;
    for (Node *n : _graph->nodes()) {
        if (visited.contains(n)) continue;

        QVector<Node*> component;
        QQueue<Node*> queue;
        queue.enqueue(n);
        visited.insert(n);

        while (!queue.isEmpty()) {
            Node *curr = queue.dequeue();
            component.append(curr);

            for (Edge *e : _outEdges[curr]) {
                Node *next = edgeTarget(e);
                if (!visited.contains(next)) {
                    visited.insert(next);
                    queue.enqueue(next);
                }
            }
            for (Edge *e : _inEdges[curr]) {
                Node *next = edgeSource(e);
                if (!visited.contains(next)) {
                    visited.insert(next);
                    queue.enqueue(next);
                }
            }
        }

        components.append(component);
    }
}

void LayeredPlacement::assignCoordinates()
{
    for (int i = 0; i < _layers.size(); ++i) {
        const QVector<Node*> &layer = _layers[i];
        qreal layerSize = layer.size();
        qreal offset = -(layerSize - 1.0) / 2.0 * _config.nodeMarginV;

        for (int j = 0; j < layer.size(); ++j) {
            Node *n = layer[j];

            if (_lockedNodes.contains(n)) {
                _positions[n] = n->point();
                continue;
            }

            qreal primary = i * _config.nodeMarginH;
            qreal secondary = offset + j * _config.nodeMarginV;

            if (_config.leftToRight) {
                _positions[n] = QPointF(primary, -secondary);
            } else {
                _positions[n] = QPointF(secondary, -primary);
            }
        }
    }
}

QMap<Node*, QPointF> LayeredPlacement::run()
{
    const auto &nodes = _graph->nodes();
    const auto &edges = _graph->edges();

    // single node: place at origin
    if (nodes.size() <= 1) {
        for (Node *n : nodes) {
            if (_lockedNodes.contains(n))
                _positions[n] = n->point();
            else
                _positions[n] = QPointF(0, 0);
        }
        return _positions;
    }

    // no edges: grid layout
    if (edges.isEmpty()) {
        int cols = qMax(1, (int)ceil(sqrt((double)nodes.size())));
        for (int i = 0; i < nodes.size(); ++i) {
            Node *n = nodes[i];
            if (_lockedNodes.contains(n)) {
                _positions[n] = n->point();
            } else {
                int row = i / cols;
                int col = i % cols;
                _positions[n] = QPointF(col * _config.nodeMarginH, -row * _config.nodeMarginV);
            }
        }
        return _positions;
    }

    // all locked: keep positions
    if (_lockedNodes.size() == nodes.size()) {
        for (Node *n : nodes) {
            _positions[n] = n->point();
        }
        return _positions;
    }

    removeCycles();
    assignLayers();
    minimizeCrossings();
    assignCoordinates();

    // handle disconnected components: offset them
    QVector<QVector<Node*>> components;
    findConnectedComponents(components);

    if (components.size() > 1) {
        qreal xOffset = 0.0;
        for (auto &comp : components) {
            // find bounding box of this component
            qreal minX = 1e18, maxX = -1e18;
            for (Node *n : comp) {
                qreal x = _positions[n].x();
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
            }

            qreal shift = xOffset - minX;
            for (Node *n : comp) {
                if (!_lockedNodes.contains(n)) {
                    _positions[n].setX(_positions[n].x() + shift);
                }
            }

            xOffset = maxX + shift + _config.nodeMarginH * 2;
        }
    }

    return _positions;
}
