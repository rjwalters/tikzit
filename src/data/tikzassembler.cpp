/*
    TikZiT - a GUI diagram editor for TikZ
    Copyright (C) 2018 Aleks Kissinger

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "tikzassembler.h"

#include "tikzparserdefs.h"
#include "tikzparser.parser.hpp"
#include "tikzlexer.h"

#include <QRegularExpression>
#include <QSet>

int yyparse(void *scanner);

TikzAssembler::TikzAssembler(Graph *graph, QObject *parent) :
    QObject(parent), _graph(graph), _tikzStyles(0)
{
    yylex_init(&scanner);
    yyset_extra(this, scanner);
    _currentEdgeData = nullptr;
    _currentPath = nullptr;
    _syntheticNodeCount = 0;
}

TikzAssembler::TikzAssembler(TikzStyles *tikzStyles, QObject *parent) :
    QObject(parent), _graph(0), _tikzStyles(tikzStyles)
{
    yylex_init(&scanner);
    yyset_extra(this, scanner);
    _currentEdgeData = nullptr;
    _currentPath = nullptr;
    _syntheticNodeCount = 0;
}

void TikzAssembler::addNodeToMap(Node *n) { _nodeMap.insert(n->name(), n); }
Node *TikzAssembler::nodeWithName(QString name) { return _nodeMap[name]; }

void TikzAssembler::reportError(const QString &msg, int line)
{
    _errorMessage += QString("Line %1: %2\n").arg(line).arg(msg);
}

QString TikzAssembler::errorMessage() const
{
    return _errorMessage;
}

bool TikzAssembler::parse(const QString &tikz)
{
    yy_scan_string(tikz.toUtf8().data(), scanner);
    int result = yyparse(scanner);

    if (result == 0) {
        if (_graph) resolveRelativePositions();
        return true;
    }
    else return false;
}

Graph *TikzAssembler::graph() const
{
    return _graph;
}

TikzStyles *TikzAssembler::tikzStyles() const
{
    return _tikzStyles;
}

bool TikzAssembler::isGraph() const
{
    return _graph != 0;
}

bool TikzAssembler::isTikzStyles() const
{
    return _tikzStyles != 0;
}

Node *TikzAssembler::currentEdgeSource() const
{
    return _currentEdgeSource;
}

void TikzAssembler::setCurrentEdgeSource(Node *currentEdgeSource)
{
    _currentEdgeSource = currentEdgeSource;
}

Node *TikzAssembler::currentPathSource() const
{
    if (_currentPath && _currentPath->length() > 0) {
        return _currentPath->edges()[0]->source();
    } else {
        return nullptr;
    }
}

GraphElementData *TikzAssembler::currentEdgeData() const
{
    return _currentEdgeData;
}

void TikzAssembler::setCurrentEdgeData(GraphElementData *currentEdgeData)
{
    _currentEdgeData = currentEdgeData;
}

QString TikzAssembler::currentEdgeSourceAnchor() const
{
    return _currentEdgeSourceAnchor;
}

void TikzAssembler::setCurrentEdgeSourceAnchor(const QString &currentEdgeSourceAnchor)
{
    _currentEdgeSourceAnchor = currentEdgeSourceAnchor;
}

void TikzAssembler::addEdge(Edge *e)
{
    if (!_currentPath) _currentPath = new Path();
    _currentPath->addEdge(e);
    _graph->addEdge(e);
}

void TikzAssembler::finishCurrentPath()
{
    if (_currentEdgeData) {
        GraphElementData *d = _currentEdgeData;
        _currentEdgeData = nullptr;
        delete d;
    }

    if (_currentPath) {
        if (_currentPath->length() < 2) {
            _currentPath->removeEdges();
            Path *p = _currentPath;
            _currentPath = nullptr;
            delete p;
        } else {
            _graph->addPath(_currentPath);
            _currentPath = nullptr;
        }
    }
}

QPointF TikzAssembler::currentDrawPos() const
{
    return _currentDrawPos;
}

void TikzAssembler::setCurrentDrawPos(const QPointF &pos)
{
    _currentDrawPos = pos;
}

Node *TikzAssembler::createSyntheticNode(QPointF pos)
{
    Node *n = new Node();
    n->setPoint(pos);
    QString name = QString("__synthetic_%1").arg(_syntheticNodeCount++);
    n->setName(name);
    n->setLabel(QString());
    _graph->addNode(n);
    addNodeToMap(n);
    return n;
}

QPointF TikzAssembler::anchorOffset(Node * /*n*/, const QString &anchor)
{
    // Approximate offsets for common anchors (in cm).
    // TikZiT doesn't know actual node dimensions, so use small fixed offsets.
    if (anchor == "north") return QPointF(0, 0.2);
    if (anchor == "south") return QPointF(0, -0.2);
    if (anchor == "east") return QPointF(0.2, 0);
    if (anchor == "west") return QPointF(-0.2, 0);
    if (anchor == "north east") return QPointF(0.15, 0.15);
    if (anchor == "north west") return QPointF(-0.15, 0.15);
    if (anchor == "south east") return QPointF(0.15, -0.15);
    if (anchor == "south west") return QPointF(-0.15, -0.15);
    if (anchor == "center") return QPointF(0, 0);
    return QPointF(0, 0);
}

QPointF TikzAssembler::resolveCoordCalc(const QString &refA, const QString &anchorA,
                                         const QString &refB, const QString &anchorB, bool hv)
{
    Node *a = nodeWithName(refA);
    Node *b = nodeWithName(refB);
    QPointF posA = a ? a->point() + anchorOffset(a, anchorA) : QPointF(0, 0);
    QPointF posB = b ? b->point() + anchorOffset(b, anchorB) : QPointF(0, 0);

    if (hv) {
        // (a -| b) means x from b, y from a
        return QPointF(posB.x(), posA.y());
    } else {
        // (a |- b) means x from a, y from b
        return QPointF(posA.x(), posB.y());
    }
}

void TikzAssembler::resolveRelativePositions()
{
    if (!_graph) return;

    // Resolve nodes that use relative positioning (right=X of Y, etc.)
    // Use iterative resolution since nodes may depend on other relatively-positioned nodes.
    QSet<Node*> unresolved;

    for (Node *n : _graph->nodes()) {
        if (!n->data()) continue;

        // Check if this node has a relative positioning property
        bool hasRelative = false;
        QStringList directions = {"right", "left", "above", "below",
                                  "above right", "above left", "below right", "below left"};
        for (const QString &dir : directions) {
            if (n->data()->hasProperty(dir)) {
                hasRelative = true;
                break;
            }
        }
        if (hasRelative) {
            unresolved.insert(n);
        }
    }

    // Iteratively resolve (simple topological ordering via iteration)
    int maxIter = unresolved.size() + 1;
    while (!unresolved.isEmpty() && maxIter > 0) {
        maxIter--;
        QSet<Node*> resolved;

        for (Node *n : unresolved) {
            QStringList directions = {"right", "left", "above", "below",
                                      "above right", "above left", "below right", "below left"};
            for (const QString &dir : directions) {
                if (!n->data()->hasProperty(dir)) continue;

                QString val = n->data()->property(dir);
                // Parse "Xpt of Y" or "X of Y" or just "of Y" (default distance)
                QRegularExpression re("^\\s*(?:([\\d.]+\\s*(?:pt|cm|mm|in|em|ex)?)\\s+)?of\\s+(.+)$");
                QRegularExpressionMatch m = re.match(val);
                if (!m.hasMatch()) continue;

                QString distStr = m.captured(1);
                QString refName = m.captured(2).trimmed();
                Node *ref = nodeWithName(refName);

                if (!ref) continue;
                if (unresolved.contains(ref)) continue; // dependency not yet resolved

                double dist = distStr.isEmpty() ? 1.0 : parseDimension(distStr);

                QPointF refPos = ref->point();
                QPointF offset(0, 0);

                if (dir == "right") offset = QPointF(dist, 0);
                else if (dir == "left") offset = QPointF(-dist, 0);
                else if (dir == "above") offset = QPointF(0, dist);
                else if (dir == "below") offset = QPointF(0, -dist);
                else if (dir == "above right") offset = QPointF(dist, dist);
                else if (dir == "above left") offset = QPointF(-dist, dist);
                else if (dir == "below right") offset = QPointF(dist, -dist);
                else if (dir == "below left") offset = QPointF(-dist, -dist);

                // Apply xshift/yshift if present
                if (n->data()->hasProperty("xshift")) {
                    offset.setX(offset.x() + parseDimension(n->data()->property("xshift")));
                }
                if (n->data()->hasProperty("yshift")) {
                    offset.setY(offset.y() + parseDimension(n->data()->property("yshift")));
                }

                n->setPoint(refPos + offset);
                resolved.insert(n);
                break;
            }
        }

        unresolved -= resolved;
        if (resolved.isEmpty()) break; // no progress, stop
    }

    // Any remaining unresolved nodes fall back to (0,0) — already their default
}

