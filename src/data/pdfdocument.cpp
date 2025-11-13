#include "pdfdocument.h"

#include <QFile>
#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QPainter>

PdfDocument::PdfDocument(QString file, QObject *parent) : QObject(parent)
{
    _file = file;
    _valid = QFile::exists(file);
}

void PdfDocument::renderTo(QLabel *label, QRect rect)
{
    // Show a blank white rectangle instead of PDF content
    QPixmap pm(rect.width() - 20, rect.height() - 20);
    pm.fill(Qt::white);
    label->setPixmap(pm);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("QLabel {background-color: white}");
}

bool PdfDocument::isValid()
{
    return _valid;
}

bool PdfDocument::exportImage(QString file, const char *format, QSize outputSize)
{
    // Create a simple white image
    QImage img(outputSize.width() > 0 ? outputSize.width() : 800,
               outputSize.height() > 0 ? outputSize.height() : 600,
               QImage::Format_ARGB32);
    img.fill(Qt::white);
    return img.save(file, format);
}

bool PdfDocument::exportPdf(QString file)
{
    // Just copy the original file if it exists
    if (_valid) {
        QFile src(_file);
        QFile dst(file);
        if (src.open(QIODevice::ReadOnly) && dst.open(QIODevice::WriteOnly)) {
            dst.write(src.readAll());
            src.close();
            dst.close();
            return true;
        }
    }
    return false;
}

void PdfDocument::copyImageToClipboard(QSize outputSize)
{
    QImage img(outputSize.width() > 0 ? outputSize.width() : 800,
               outputSize.height() > 0 ? outputSize.height() : 600,
               QImage::Format_ARGB32);
    img.fill(Qt::white);
    QApplication::clipboard()->setImage(img, QClipboard::Clipboard);
}

QImage PdfDocument::asImage(QSize outputSize)
{
    if (outputSize.isNull()) {
        outputSize = QSize(800, 600);
    }
    QImage img(outputSize.width(), outputSize.height(), QImage::Format_ARGB32);
    img.fill(Qt::white);
    return img;
}

QSize PdfDocument::size()
{
    return QSize(800, 600); // Return a default size
}


