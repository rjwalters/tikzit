#ifndef TESTPARSER_H
#define TESTPARSER_H

#include <QObject>

class TestParser : public QObject
{
    Q_OBJECT
private slots:
    void parseEmptyGraph();
    void parseNodeGraph();
    void parseEdgeGraph();
    void parseEdgeNode();
    void parseEdgeBends();
    void parseBbox();
    void parseNonAsciiLabel();
    void parseEdgeMinusMinus();
    void parseEdgeHVLine();
    void parsePlusPlusCoord();
    void parseCoordCalc();
    void parseNodeNoAt();
    void parseNodeNoName();
    void parseScope();
    void parseShiftedCoordinate();
    void parseNodeRelativePosition();
};

#endif // TESTPARSER_H
