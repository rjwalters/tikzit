#ifndef PATHITEM_H
#define PATHITEM_H

#include "path.h"

#include <QGraphicsItem>

class PathItem : public QGraphicsItem
{
public:
    PathItem(Path *path);
    void readPos();
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

    Path *path() const;

    QPainterPath painterPath() const;
    void setPainterPath(const QPainterPath &painterPath);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;

private:
    Path *_path;
    QPainterPath _painterPath;
    QPainterPath _shapePath;
    QRectF _boundingRect;
};

#endif // PATHITEM_H
