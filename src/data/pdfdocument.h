#ifndef PDFDOCUMENT_H
#define PDFDOCUMENT_H

#include <QObject>
#include <QString>
#include <QLabel>
#include <QSize>
#include <QImage>

class PdfDocument : public QObject
{
    Q_OBJECT
public:
    explicit PdfDocument(QString file, QObject *parent = nullptr);
    void renderTo(QLabel *label, QRect rect);
    bool isValid();
    bool exportImage(QString file, const char *format, QSize outputSize=QSize());
    bool exportPdf(QString file);
    void copyImageToClipboard(QSize outputSize=QSize());
    QImage asImage(QSize outputSize=QSize());
    QSize size();
private:
    QString _file;
    bool _valid;
};

#endif // PDFDOCUMENT_H
