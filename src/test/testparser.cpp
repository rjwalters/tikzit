#include "testparser.h"
#include "graph.h"
#include "tikzassembler.h"

#include <QTest>
#include <QVector>
#include <cmath>

//void TestParser::initTestCase()
//{

//}

//void TestParser::cleanupTestCase()
//{

//}

void TestParser::parseEmptyGraph()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse("\\begin{tikzpicture}\n\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 0);
    QVERIFY(g->edges().size() == 0);
    delete g;
}

void TestParser::parseNodeGraph()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (node0) at (1.1, -2.2) {};\n"
    "  \\node (node1) at (3, 4) {test};\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);
    QVERIFY(g->edges().size() == 0);
    QVERIFY(g->nodes()[0]->name() == "node0");
    QVERIFY(g->nodes()[0]->label() == "");
    QVERIFY(g->nodes()[0]->point() == QPointF(1.1,-2.2));
    QVERIFY(g->nodes()[1]->name() == "node1");
    QVERIFY(g->nodes()[1]->label() == "test");
    QVERIFY(g->nodes()[1]->point() == QPointF(3,4));
    delete g;
}

void TestParser::parseEdgeGraph()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\begin{pgfonlayer}{nodelayer}\n"
    "    \\node [style=x, {foo++}] (0) at (-1, -1) {};\n"
    "    \\node [style=y] (1) at (0, 1) {};\n"
    "    \\node [style=z] (2) at (1, -1) {};\n"
    "  \\end{pgfonlayer}\n"
    "  \\begin{pgfonlayer}{edgelayer}\n"
    "    \\draw [style=a] (1.center) to (2);\n"
    "    \\draw [style=b, foo] (2) to (0.west);\n"
    "    \\draw [style=c] (0) to (1);\n"
    "  \\end{pgfonlayer}\n"
    "\\end{tikzpicture}\n");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 3);
    QVERIFY(g->edges().size() == 3);
    QVERIFY(g->nodes()[0]->data()->atom("foo++"));
    QVERIFY(g->edges()[0]->data()->property("style") == "a");
    QVERIFY(!g->edges()[0]->data()->atom("foo"));
    QVERIFY(g->edges()[1]->data()->property("style") == "b");
    QVERIFY(g->edges()[1]->data()->atom("foo"));
    QVERIFY(g->edges()[2]->data()->property("style") == "c");
    Node *en = g->edges()[0]->edgeNode();
    QVERIFY(en == 0);
    delete g;
}

void TestParser::parseEdgeNode()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\begin{pgfonlayer}{nodelayer}\n"
    "    \\node [style=none] (0) at (-1, 0) {};\n"
    "    \\node [style=none] (1) at (1, 0) {};\n"
    "  \\end{pgfonlayer}\n"
    "  \\begin{pgfonlayer}{edgelayer}\n"
    "    \\draw [style=diredge] (0.center) to node[foo, bar=baz baz]{test} (1.center);\n"
    "  \\end{pgfonlayer}\n"
    "\\end{tikzpicture}\n");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);
    QVERIFY(g->edges().size() == 1);
    Node *en = g->edges()[0]->edgeNode();
    QVERIFY(en != 0);
    QVERIFY(en->label() == "test");
    QVERIFY(en->data()->atom("foo"));
    QVERIFY(en->data()->property("bar") == "baz baz");
    delete g;
}

void TestParser::parseEdgeBends()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\begin{pgfonlayer}{nodelayer}\n"
    "    \\node [style=white] (0) at (-1, 0) {};\n"
    "    \\node [style=black] (1) at (1, 0) {};\n"
    "  \\end{pgfonlayer}\n"
    "  \\begin{pgfonlayer}{edgelayer}\n"
    "    \\draw [style=diredge,bend left] (0) to (1);\n"
    "    \\draw [style=diredge,bend right] (0) to (1);\n"
    "    \\draw [style=diredge,bend left=20] (0) to (1);\n"
    "    \\draw [style=diredge,bend right=80] (0) to (1);\n"
    "    \\draw [style=diredge,in=10,out=150,looseness=2] (0) to (1);\n"
    "  \\end{pgfonlayer}\n"
    "\\end{tikzpicture}\n");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);
    QVERIFY(g->edges().size() == 5);
    QVERIFY(g->edges()[0]->bend() == -30);
    QVERIFY(g->edges()[1]->bend() == 30);
    QVERIFY(g->edges()[2]->bend() == -20);
    QVERIFY(g->edges()[3]->bend() == 80);
    QVERIFY(g->edges()[4]->inAngle() == 10);
    QVERIFY(g->edges()[4]->outAngle() == 150);
    QVERIFY(g->edges()[4]->weight() == 2.0/2.5);
}

void TestParser::parseBbox()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\path [use as bounding box] (-1.5,-1.5) rectangle (1.5,1.5);\n"
    "  \\begin{pgfonlayer}{nodelayer}\n"
    "    \\node [style=white dot] (0) at (-1, -1) {};\n"
    "    \\node [style=white dot] (1) at (0, 1) {};\n"
    "    \\node [style=white dot] (2) at (1, -1) {};\n"
    "  \\end{pgfonlayer}\n"
    "  \\begin{pgfonlayer}{edgelayer}\n"
    "    \\draw [style=diredge] (1) to (2);\n"
    "    \\draw [style=diredge] (2) to (0);\n"
    "    \\draw [style=diredge] (0) to (1);\n"
    "  \\end{pgfonlayer}\n"
    "\\end{tikzpicture}\n");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 3);
    QVERIFY(g->edges().size() == 3);
    QVERIFY(g->hasBbox());
    QVERIFY(g->bbox() == QRectF(QPointF(-1.5,-1.5), QPointF(1.5,1.5)));

    delete g;
}

void TestParser::parseNonAsciiLabel()
{
    // Test Chinese characters
    Graph *g1 = new Graph();
    TikzAssembler ga1(g1);
    bool res1 = ga1.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (n) at (0, 0) {" "\xe4\xbd\xa0\xe5\xa5\xbd" "};\n"
    "\\end{tikzpicture}");
    QVERIFY(res1);
    QVERIFY(g1->nodes().size() == 1);
    QVERIFY(g1->nodes()[0]->label() == QString::fromUtf8("\xe4\xbd\xa0\xe5\xa5\xbd"));
    // Verify roundtrip: tikz output should contain the UTF-8 label
    QVERIFY(g1->tikz().contains(QString::fromUtf8("\xe4\xbd\xa0\xe5\xa5\xbd")));
    delete g1;

    // Test accented Latin characters
    Graph *g2 = new Graph();
    TikzAssembler ga2(g2);
    bool res2 = ga2.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (n) at (0, 0) {na" "\xc3\xaf" "ve};\n"
    "\\end{tikzpicture}");
    QVERIFY(res2);
    QVERIFY(g2->nodes().size() == 1);
    QVERIFY(g2->nodes()[0]->label() == QString::fromUtf8("na\xc3\xafve"));
    delete g2;
}

void TestParser::parseEdgeMinusMinus()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (0, 0) {};\n"
    "  \\node (b) at (1, 1) {};\n"
    "  \\draw (a) -- (b);\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);
    QVERIFY(g->edges().size() == 1);
    QVERIFY(g->edges()[0]->source()->name() == "a");
    QVERIFY(g->edges()[0]->target()->name() == "b");
    delete g;
}

void TestParser::parseEdgeHVLine()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (0, 0) {};\n"
    "  \\node (b) at (1, 1) {};\n"
    "  \\draw (a) -| (b);\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);
    QVERIFY(g->edges().size() == 1);
    QVERIFY(g->edges()[0]->source()->name() == "a");
    QVERIFY(g->edges()[0]->target()->name() == "b");

    // Test |- too
    Graph *g2 = new Graph();
    TikzAssembler ga2(g2);
    bool res2 = ga2.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (0, 0) {};\n"
    "  \\node (b) at (1, 1) {};\n"
    "  \\draw (a) |- (b);\n"
    "\\end{tikzpicture}");
    QVERIFY(res2);
    QVERIFY(g2->nodes().size() == 2);
    QVERIFY(g2->edges().size() == 1);
    delete g;
    delete g2;
}

void TestParser::parsePlusPlusCoord()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (1, 2) {};\n"
    "  \\draw (a) -- ++(0.5, 0.5);\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);  // original + synthetic
    QVERIFY(g->edges().size() == 1);
    // The synthetic node should be at (1+0.5, 2+0.5) = (1.5, 2.5)
    Node *target = g->edges()[0]->target();
    QVERIFY(std::abs(target->point().x() - 1.5) < 0.01);
    QVERIFY(std::abs(target->point().y() - 2.5) < 0.01);
    delete g;
}

void TestParser::parseCoordCalc()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (1, 2) {};\n"
    "  \\node (b) at (3, 4) {};\n"
    "  \\draw (a) -- (a -| b);\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 3);  // a, b, synthetic
    QVERIFY(g->edges().size() == 1);
    // (a -| b) should be (b.x, a.y) = (3, 2)
    Node *target = g->edges()[0]->target();
    QVERIFY(std::abs(target->point().x() - 3.0) < 0.01);
    QVERIFY(std::abs(target->point().y() - 2.0) < 0.01);

    // Test |- variant
    Graph *g2 = new Graph();
    TikzAssembler ga2(g2);
    bool res2 = ga2.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (1, 2) {};\n"
    "  \\node (b) at (3, 4) {};\n"
    "  \\draw (a) -- (a |- b);\n"
    "\\end{tikzpicture}");
    QVERIFY(res2);
    // (a |- b) should be (a.x, b.y) = (1, 4)
    Node *target2 = g2->edges()[0]->target();
    QVERIFY(std::abs(target2->point().x() - 1.0) < 0.01);
    QVERIFY(std::abs(target2->point().y() - 4.0) < 0.01);
    delete g;
    delete g2;
}

void TestParser::parseNodeNoAt()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (1, 0) {A};\n"
    "  \\node [right=1 of a] (b) {B};\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);
    QVERIFY(g->nodes()[1]->name() == "b");
    QVERIFY(g->nodes()[1]->label() == "B");
    // After resolveRelativePositions, b should be at (2, 0) (right=1 of a at (1,0))
    QVERIFY(std::abs(g->nodes()[1]->point().x() - 2.0) < 0.01);
    QVERIFY(std::abs(g->nodes()[1]->point().y() - 0.0) < 0.01);
    delete g;
}

void TestParser::parseNodeNoName()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node [style=foo] at (1, 2) {hello};\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 1);
    QVERIFY(g->nodes()[0]->label() == "hello");
    QVERIFY(g->nodes()[0]->point() == QPointF(1, 2));
    // Should have an auto-generated name
    QVERIFY(!g->nodes()[0]->name().isEmpty());
    delete g;
}

void TestParser::parseScope()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\begin{scope}[xshift=1cm]\n"
    "    \\node (a) at (0, 0) {A};\n"
    "  \\end{scope}\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 1);
    QVERIFY(g->nodes()[0]->name() == "a");
    delete g;
}

void TestParser::parseShiftedCoordinate()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (2, 3) {A};\n"
    "  \\node (b) at ([xshift=1]a) {B};\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 2);
    QVERIFY(g->nodes()[1]->name() == "b");
    // xshift=1 (no unit, treated as cm) from a at (2,3) → (3, 3)
    QVERIFY(std::abs(g->nodes()[1]->point().x() - 3.0) < 0.01);
    QVERIFY(std::abs(g->nodes()[1]->point().y() - 3.0) < 0.01);
    delete g;
}

void TestParser::parseNodeRelativePosition()
{
    Graph *g = new Graph();
    TikzAssembler ga(g);
    bool res = ga.parse(
    "\\begin{tikzpicture}\n"
    "  \\node (a) at (0, 0) {A};\n"
    "  \\node [right=2 of a] (b) {B};\n"
    "  \\node [above=1 of b] (c) {C};\n"
    "  \\node [left=1 of a] (d) {D};\n"
    "  \\node [below=0.5 of a] (e) {E};\n"
    "\\end{tikzpicture}");
    QVERIFY(res);
    QVERIFY(g->nodes().size() == 5);

    // b: right=2 of a(0,0) → (2, 0)
    QVERIFY(std::abs(g->nodes()[1]->point().x() - 2.0) < 0.01);
    QVERIFY(std::abs(g->nodes()[1]->point().y() - 0.0) < 0.01);

    // c: above=1 of b(2,0) → (2, 1)
    QVERIFY(std::abs(g->nodes()[2]->point().x() - 2.0) < 0.01);
    QVERIFY(std::abs(g->nodes()[2]->point().y() - 1.0) < 0.01);

    // d: left=1 of a(0,0) → (-1, 0)
    QVERIFY(std::abs(g->nodes()[3]->point().x() - (-1.0)) < 0.01);
    QVERIFY(std::abs(g->nodes()[3]->point().y() - 0.0) < 0.01);

    // e: below=0.5 of a(0,0) → (0, -0.5)
    QVERIFY(std::abs(g->nodes()[4]->point().x() - 0.0) < 0.01);
    QVERIFY(std::abs(g->nodes()[4]->point().y() - (-0.5)) < 0.01);

    delete g;
}

