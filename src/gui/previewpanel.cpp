#include "previewpanel.h"
#include "tikzit.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>
#include <QSettings>
#include <QPdfDocument>

PreviewPanel::PreviewPanel(QObject *parent)
    : QObject(parent),
      _process(nullptr),
      _workingDir(nullptr),
      _enabled(false)
{
    _pdfDoc = new QPdfDocument(this);

    _debounceTimer = new QTimer(this);
    _debounceTimer->setSingleShot(true);
    _debounceTimer->setInterval(500);
    connect(_debounceTimer, &QTimer::timeout, this, &PreviewPanel::compile);

    QSettings settings("tikzit", "tikzit");
    _preamble = settings.value("preview-preamble", "").toString();
}

PreviewPanel::~PreviewPanel()
{
    if (_process) {
        _process->disconnect(this);
        _process->kill();
        _process->waitForFinished(1000);
    }
    delete _workingDir;
}

void PreviewPanel::scheduleUpdate(const QString &tikzSource)
{
    _pendingSource = tikzSource;
    if (_enabled)
        _debounceTimer->start();
}

void PreviewPanel::compileNow()
{
    _debounceTimer->stop();
    compile();
}

bool PreviewPanel::enabled() const
{
    return _enabled;
}

void PreviewPanel::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (_enabled && !_pendingSource.trimmed().isEmpty())
        _debounceTimer->start();
}

QString PreviewPanel::preamble() const
{
    return _preamble;
}

void PreviewPanel::setPreamble(const QString &preamble)
{
    _preamble = preamble;
    QSettings settings("tikzit", "tikzit");
    settings.setValue("preview-preamble", _preamble);
}

QString PreviewPanel::findPdfLatex()
{
    if (!_pdfLatexPath.isEmpty())
        return _pdfLatexPath;

    QSettings settings("tikzit", "tikzit");

    if (settings.value("auto-detect-pdflatex", true).toBool()) {
        _pdfLatexPath = QStandardPaths::findExecutable("pdflatex");
        if (_pdfLatexPath.isEmpty()) {
            QStringList texDirs;
            texDirs << "/Library/TeX/texbin"
                    << "/usr/texbin"
                    << "/usr/local/bin"
                    << "/sw/bin";
            _pdfLatexPath = QStandardPaths::findExecutable("pdflatex", texDirs);
        }
    } else {
        _pdfLatexPath = settings.value("pdflatex-path", "/usr/bin/pdflatex").toString();
    }

    return _pdfLatexPath;
}

void PreviewPanel::writeTexFile(const QString &tikzSource)
{
    // Copy tikzit.sty to working dir
    QString styDst = _workingDir->path() + "/tikzit.sty";
    QFile::remove(styDst);
    QFile::copy(":/tex/sample/tikzit.sty", styDst);

    // Copy style file if available
    QString styleInclude;
    if (!tikzit->styleFile().isEmpty() && QFile::exists(tikzit->styleFilePath())) {
        QString dst = _workingDir->path() + "/" + tikzit->styleFile();
        QFile::remove(dst);
        QFile::copy(tikzit->styleFilePath(), dst);
        styleInclude = "\\input{" + tikzit->styleFile() + "}\n";

        // Also copy .tikzdefs if it exists
        QFileInfo fi(tikzit->styleFilePath());
        QString defFile = fi.baseName() + ".tikzdefs";
        QString defFilePath = fi.absolutePath() + "/" + defFile;
        if (QFile::exists(defFilePath)) {
            QString defDst = _workingDir->path() + "/" + defFile;
            QFile::remove(defDst);
            QFile::copy(defFilePath, defDst);
            styleInclude += "\\input{" + defFile + "}\n";
        }
    }

    QFile f(_workingDir->path() + "/preview.tex");
    f.open(QIODevice::WriteOnly);
    QTextStream tex(&f);
    tex << "\\documentclass{article}\n";
    tex << "\\usepackage[margin=1pt]{geometry}\n";
    tex << "\\pagestyle{empty}\n";
    tex << "\\PassOptionsToPackage{svgnames,dvipsnames}{xcolor}\n";
    tex << "\\usepackage{tikzit}\n";
    tex << "\\usetikzlibrary{arrows.meta,calc,fit,positioning}\n";
    tex << "\\tikzset{every picture/.style={tikzfig}}\n";
    tex << styleInclude;
    if (!_preamble.isEmpty()) {
        tex << _preamble << "\n";
    }
    tex << "\\begin{document}\n\n";

    tex << tikzSource << "\n";

    tex << "\n\n\\end{document}\n";
    f.close();
}

void PreviewPanel::compile()
{
    if (!_enabled)
        return;

    if (_pendingSource.trimmed().isEmpty())
        return;

    QString pdflatex = findPdfLatex();
    if (pdflatex.isEmpty()) {
        emit compileFailed("pdflatex not found");
        return;
    }

    // Disconnect and kill any running process
    if (_process) {
        _process->disconnect(this);
        if (_process->state() == QProcess::Running) {
            _process->kill();
            _process->waitForFinished(1000);
        }
        _process->deleteLater();
        _process = nullptr;
    }

    // Create fresh temp dir
    delete _workingDir;
    _workingDir = new QTemporaryDir();
    if (!_workingDir->isValid()) {
        emit compileFailed("Cannot create temp directory");
        return;
    }

    writeTexFile(_pendingSource);

    emit compileStarted();

    _process = new QProcess(this);
    _process->setProcessChannelMode(QProcess::MergedChannels);
    _process->setWorkingDirectory(_workingDir->path());

    connect(_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PreviewPanel::processFinished);
    connect(_process, &QProcess::errorOccurred,
            this, &PreviewPanel::processError);

    _process->start(pdflatex, QStringList()
                    << "-interaction=nonstopmode"
                    << "-halt-on-error"
                    << "preview.tex");
}

void PreviewPanel::renderPdf(const QString &pdfPath)
{
    _pdfDoc->close();
    auto status = _pdfDoc->load(pdfPath);
    if (status != QPdfDocument::Error::None) {
        emit compileFailed("Failed to load PDF");
        return;
    }

    if (_pdfDoc->pageCount() < 1) {
        emit compileFailed("PDF has no pages");
        return;
    }

    // Render at high DPI for quality
    QSizeF pageSize = _pdfDoc->pagePointSize(0); // in points (1/72 inch)
    // Render at 4x scale for crisp display
    qreal renderScale = 4.0;
    QSize renderSize(pageSize.width() * renderScale, pageSize.height() * renderScale);
    QImage image = _pdfDoc->render(0, renderSize);

    if (image.isNull()) {
        emit compileFailed("Failed to render PDF page");
        return;
    }

    // The tikzBBox is computed from the graph model in the scene,
    // so we just pass the page size here for reference.
    QRectF pageRect(0, 0, pageSize.width(), pageSize.height());
    emit pdfReady(image, pageRect);
}

void PreviewPanel::processFinished(int exitCode, QProcess::ExitStatus)
{
    if (exitCode == 0) {
        QString pdfPath = _workingDir->path() + "/preview.pdf";
        if (QFile::exists(pdfPath)) {
            renderPdf(pdfPath);
        } else {
            emit compileFailed("PDF not generated");
        }
    } else {
        QByteArray outputData = _process->readAllStandardOutput();
        QString output = QString::fromUtf8(outputData);
        QString errorMsg = "Compilation failed";
        for (const QString &line : output.split('\n')) {
            if (line.startsWith('!')) {
                errorMsg = line.mid(2).trimmed();
                if (errorMsg.length() > 60)
                    errorMsg = errorMsg.left(57) + "...";
                break;
            }
        }
        emit compileFailed(errorMsg);
    }
}

void PreviewPanel::processError(QProcess::ProcessError)
{
    emit compileFailed("Failed to start pdflatex");
}
