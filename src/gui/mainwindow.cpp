#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "mainmenu.h"
#include "tikzassembler.h"
#include "toolpalette.h"
#include "tikzit.h"
#include "util.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QList>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextEdit>
#include <QTextBlock>
#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>
#include <QRegularExpression>

int MainWindow::_numWindows = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QSettings settings("tikzit", "tikzit");
    _windowId = _numWindows;
    _numWindows++;
    ui->setupUi(this);

    setWindowIcon(QIcon(":/images/tikzit.png"));

    setAttribute(Qt::WA_DeleteOnClose, true);
    _tikzDocument = new TikzDocument(this);

    _toolPalette = new ToolPalette(ui->toolbarContainer);
    
    // Create a vertical layout for the toolbar container
    QVBoxLayout *toolbarLayout = new QVBoxLayout(ui->toolbarContainer);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->addWidget(_toolPalette);
    toolbarLayout->addStretch();
    
    _toolPalette->setMovable(false);
    
    _stylePalette = new StylePalette(this);

    _tikzScene = new TikzScene(_tikzDocument, _toolPalette, _stylePalette, this);
    ui->tikzView->setScene(_tikzScene);

    // Preview panel is a hidden background compilation engine
    _previewPanel = new PreviewPanel(this);
    connect(_previewPanel, &PreviewPanel::pdfReady,
            this, &MainWindow::onPreviewReady);
    connect(_previewPanel, &PreviewPanel::compileFailed,
            this, &MainWindow::onPreviewFailed);

    // TODO: check if each window should have a menu
    _menu = new MainMenu();
    _menu->setParent(this);
    setMenuBar(_menu);

    // initially, the source view should be collapsed
    QList<int> sz = ui->splitter->sizes();
    sz[0] = sz[0] + sz[1];
    sz[1] = 0;
    ui->splitter->setSizes(sz);

    _tikzDocument->refreshTikz();

    connect(_tikzDocument->undoStack(), SIGNAL(cleanChanged(bool)), this, SLOT(updateFileName()));

    setFont();

    QVariant state = settings.value(QString("windowState-main-qt") + qVersion());
    if (state.isValid()) {
        restoreState(state.toByteArray(), 2);
    } else {
        addDockWidget(Qt::RightDockWidgetArea, _stylePalette);
        resizeDocks({_stylePalette}, {130}, Qt::Horizontal);
    }

    // Add dock toggle actions to the View menu. This must be called after
    // restoreState/addDockWidget so that createPopupMenu() sees the
    // registered dock widgets (fixes: style palette cannot be reopened).
    _menu->addDocks(createPopupMenu());
}

MainWindow::~MainWindow()
{
    tikzit->removeWindow(this);
    delete ui;
}

void MainWindow::setFont()
{
    QSettings settings("tikzit", "tikzit");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    ui->tikzSource->setTabStopDistance(20.0);
#else
    ui->tikzSource->setTabStopWidth(20);
#endif

    QFont font("Courier New", 12);
    if (settings.contains("source-font")) {
        font.fromString(settings.value("source-font").toString());
    }
    ui->tikzSource->setFont(font);
}

void MainWindow::restorePosition()
{
    QSettings settings("tikzit", "tikzit");
    QVariant geom = settings.value(QString("geometry-main-qt") + qVersion());

    if (geom.isValid()) {
        restoreGeometry(geom.toByteArray());
    }


}

void MainWindow::open(QString fileName)
{
    _tikzDocument->open(fileName);

    //ui->tikzSource->setText(_tikzDocument->tikz());


    if (_tikzDocument->parseSuccess()) {
        _tikzScene->setTikzDocument(_tikzDocument);
        updateFileName();
    } else {
        QString msg = "Cannot read TiKZ source.";
        QString details = _tikzDocument->parseError();
        if (!details.isEmpty()) {
            msg += "\n\n" + details;
        }
        QMessageBox::warning(this, "Parse failed", msg);
    }

    // Store original file content for preview patching (the graph model
    // roundtrip is lossy, so we patch the original source with updated
    // node positions instead of regenerating from scratch)
    QFile previewFile(fileName);
    if (previewFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&previewFile);
        _originalTikzSource = in.readAll();
        _previewPanel->scheduleUpdate(_originalTikzSource);
        previewFile.close();
    }
}

QSplitter *MainWindow::splitter() const {
    return ui->splitter;
}

PreviewPanel *MainWindow::previewPanel() const {
    return _previewPanel;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // store qt version in window geometry keys to avoid strange behaviour w/ multiple Qt's on one system
    QSettings settings("tikzit", "tikzit");
    settings.setValue(QString("geometry-main-qt") + qVersion(), saveGeometry());
    settings.setValue(QString("windowState-main-qt") + qVersion(), saveState(2));

    if (!_tikzDocument->isClean()) {
        QString nm = _tikzDocument->shortName();
        if (nm.isEmpty()) nm = "untitled";
        QMessageBox::StandardButton resBtn = QMessageBox::question(
                    this, "Save Changes",
                    "Do you wish to save changes to " + nm + "?",
                    QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
                    QMessageBox::Yes);

        if (resBtn == QMessageBox::Yes && _tikzDocument->save()) {
            event->accept();
        } else if (resBtn == QMessageBox::No) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        tikzit->setActiveWindow(this);
        tikzit->setDialogStatus(false);
        //tikzit->stylePalette()->raise();
    }
    QMainWindow::changeEvent(event);
}

MainMenu *MainWindow::menu() const
{
    return _menu;
}

StylePalette *MainWindow::stylePalette() const
{
    return _stylePalette;
}

QString MainWindow::tikzSource()
{
    return ui->tikzSource->toPlainText();
}

void MainWindow::setSourceLine(int line)
{
    QTextCursor cursor(ui->tikzSource->document()->findBlockByLineNumber(line));
    cursor.movePosition(QTextCursor::EndOfLine);
    //ui->tikzSource->moveCursor(QTextCursor::End);
    ui->tikzSource->setTextCursor(cursor);
    ui->tikzSource->setFocus();
}

void MainWindow::updateFileName()
{
    QString nm = _tikzDocument->shortName();
    if (nm.isEmpty()) nm = "untitled";
    if (!_tikzDocument->isClean()) nm += "*";
    setWindowTitle(nm + " - TikZiT");
}

QString MainWindow::patchedPreviewSource()
{
    // Patch the original tikz source with current node positions from the
    // graph model.  For each node, find its \node command in the original
    // source by name and insert/replace the "at (x, y)" clause.  This
    // preserves the original file's style definitions, custom colors, draw
    // commands, etc. while reflecting position changes from the canvas.

    // Build name→position map from current graph
    QMap<QString, QPointF> positions;
    for (Node *n : _tikzDocument->graph()->nodes()) {
        if (!n->name().isEmpty() && !n->name().startsWith("__synthetic"))
            positions[n->name()] = n->point();
    }

    if (positions.isEmpty())
        return _originalTikzSource;

    QString result = _originalTikzSource;

    // For each known node, find \node...(name) and ensure it has at (x, y)
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        QString name = QRegularExpression::escape(it.key());
        QPointF pos = it.value();
        QString atStr = QString("at (%1, %2)")
            .arg(floatToString(pos.x()))
            .arg(floatToString(pos.y()));

        // Match: \node ... (name) [optional "at (x, y)"]
        // The \node can have options in [...] before the name.
        // We capture everything up to and including (name), then
        // optionally match an existing "at (...)" clause.
        QRegularExpression re(
            "(\\\\node\\b[^;]*?\\(" + name + "\\))"  // group 1: \node...(name)
            "(\\s*at\\s*\\([^)]*\\))?"                // group 2: optional existing at clause
        );

        QRegularExpressionMatch m = re.match(result);
        if (m.hasMatch()) {
            // Replace the matched region with group1 + new at clause
            QString replacement = m.captured(1) + " " + atStr;
            result.replace(m.capturedStart(), m.capturedLength(), replacement);
        }
    }

    return result;
}

void MainWindow::refreshTikz()
{
    // don't emit textChanged() when we update the tikz
    ui->tikzSource->blockSignals(true);
    ui->tikzSource->setText(_tikzDocument->tikz());
    ui->tikzSource->blockSignals(false);

    // If we have original file source, patch it with current node positions
    // so the preview preserves styles/colors from the original file.
    // Otherwise fall back to the regenerated tikz.
    if (!_originalTikzSource.isEmpty()) {
        _previewPanel->scheduleUpdate(patchedPreviewSource());
    } else {
        _previewPanel->scheduleUpdate(_tikzDocument->tikz());
    }
}

ToolPalette *MainWindow::toolPalette() const
{
    return _toolPalette;
}

TikzDocument *MainWindow::tikzDocument() const
{
    return _tikzDocument;
}

TikzScene *MainWindow::tikzScene() const
{
    return _tikzScene;
}

int MainWindow::windowId() const
{
    return _windowId;
}

TikzView *MainWindow::tikzView() const
{
    return ui->tikzView;
}

void MainWindow::on_tikzSource_textChanged()
{
    if (_tikzScene->enabled()) _tikzScene->setEnabled(false);

    // User is editing source directly — the original file source is
    // no longer the authority, so clear it and preview the editor content.
    _originalTikzSource.clear();
    _previewPanel->scheduleUpdate(ui->tikzSource->toPlainText());
}

void MainWindow::onPreviewReady(const QImage &image, const QRectF &tikzBBox)
{
    _tikzScene->setPreviewBackground(image, tikzBBox);
}

void MainWindow::onPreviewFailed(const QString &errorMessage)
{
    qDebug() << "Preview compilation failed:" << errorMessage;
}


