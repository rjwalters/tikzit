#ifndef PREVIEWPANEL_H
#define PREVIEWPANEL_H

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QImage>
#include <QPdfDocument>

class PreviewPanel : public QObject
{
    Q_OBJECT
public:
    explicit PreviewPanel(QObject *parent = nullptr);
    ~PreviewPanel();

    void scheduleUpdate(const QString &tikzSource);
    void compileNow();

    QString preamble() const;
    void setPreamble(const QString &preamble);

    bool enabled() const;
    void setEnabled(bool enabled);

signals:
    void pdfReady(const QImage &image, const QRectF &tikzBBox);
    void compileFailed(const QString &errorMessage);
    void compileStarted();

private slots:
    void compile();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);

private:
    QString findPdfLatex();
    void writeTexFile(const QString &tikzSource);
    void renderPdf(const QString &pdfPath);

    QPdfDocument *_pdfDoc;
    QTimer *_debounceTimer;
    QProcess *_process;
    QTemporaryDir *_workingDir;
    QString _pendingSource;
    QString _pdfLatexPath;
    QString _preamble;
    bool _enabled;
};

#endif // PREVIEWPANEL_H
