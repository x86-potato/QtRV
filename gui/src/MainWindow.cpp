#include "MainWindow.h"
#include "Console.h"
#include "MemoryPanel.h"
#include "RegisterPanel.h"
#include "CodeEditor.h"
#include "PipelinePanel.h"
#include "DisplayWindow.h"

#include <stdexcept>
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QToolButton>
#include <QWidgetAction>
#include <QSlider>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QToolButton>
#include <QFileInfo>
#include <QLabel>
#include <QAction>
#include <QStyleFactory>
#include <QInputDialog>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QtMips");
    resize(1200, 800);

    m_emulator = std::make_unique<Emulator>();

    m_emulator->setInputCallback([this](const std::string& prompt) -> std::string {
        // A blocking read syscall stalls MipsCPU::tick() itself -- and thus
        // the whole run()/step() call -- before control ever returns to the
        // GUI event loop. Without this, anything written to memory (e.g. a
        // bitmap fill) earlier in the same run is invisible for as long as
        // the program is waiting on input, since nothing has told the
        // panels/display to repaint yet.
        updatePanels();

        bool ok = false;
        QString result = QInputDialog::getText(
            this,
            "Program Input",
            QString::fromStdString(prompt),
            QLineEdit::Normal, "", &ok);

        if (!ok) {
            // Cancelling the prompt stops the program, rather than silently
            // feeding it an empty/zero value -- otherwise a retry-on-bad-
            // input loop (e.g. "re-ask until the number is in range") would
            // just re-prompt forever with no way to break out, since a
            // blocking run() call can't be interrupted any other way.
            m_emulator->halt();
            return "";
        }
        return result.toStdString();
    });

    m_emulator->setSleepCallback([this](uint32_t ms) {
        // Refresh first so a "reveal, pause, then hide" sequence (e.g. a
        // memory game flipping two cards) actually shows the revealed state
        // during the pause, instead of a stale view from before this
        // syscall ran. Then pump a local event loop for the requested
        // duration -- a plain blocking sleep here would also freeze Qt's
        // event loop, so nothing could repaint until the "instant" pause
        // was already over, defeating the point of waiting at all.
        updatePanels();

        QEventLoop loop;
        QTimer::singleShot(static_cast<int>(ms), &loop, &QEventLoop::quit);
        loop.exec();
    });

    //start execution timer for running the emulator in real-time
    m_runTimer = new QTimer(this);
    connect(m_runTimer, &QTimer::timeout, this, &MainWindow::onRunTimerTick);


    setupCentralWidget();
    
    setupDocks();       
    
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
}

void MainWindow::setupCentralWidget()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    
    setCentralWidget(m_tabWidget);
    
    onFileNew(); // Open default empty file
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New",  this, &MainWindow::onFileNew, QKeySequence::New);
    fileMenu->addAction("&Open", this, &MainWindow::onFileOpen, QKeySequence::Open);
    fileMenu->addAction("&Save", this, [this]{ onFileSave(); }, QKeySequence::Save);
    fileMenu->addAction("Save &As...", this, [this]{ onFileSaveAs(); }, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("&Quit", qApp, &QApplication::quit, QKeySequence::Quit);

    QMenu *runMenu = menuBar()->addMenu("&Run");
    // Wire up to our new slots
    runMenu->addAction("&Assemble && Run", this, &MainWindow::onRunClicked, QKeySequence("F5"));
    runMenu->addAction("&Step", this, &MainWindow::onStepClicked, QKeySequence("F10"));
    runMenu->addAction("&Reset", this, [this]{
        if (m_runTimer->isActive()) m_runTimer->stop();
        if (m_runAction) m_runAction->setText("Run");
        m_emulator->reset();
        m_programLoaded = false;
        updatePanels();
    });

    QMenu *viewMenu = menuBar()->addMenu("&View");
    if (m_registerPanel) viewMenu->addAction(m_registerPanel->toggleViewAction());
    if (m_memoryPanel)   viewMenu->addAction(m_memoryPanel->toggleViewAction());
    if (m_console)       viewMenu->addAction(m_console->toggleViewAction());
    if (m_pipelinePanel) viewMenu->addAction(m_pipelinePanel->toggleViewAction());

    // Devices — structured as one submenu per device so more can be added later.
    QMenu *devicesMenu = menuBar()->addMenu("&Devices");
    QMenu *bitmapMenu = devicesMenu->addMenu("Bitmap Display");
    bitmapMenu->addAction("Show Window", this, [this] {
        if (!m_displayWindow) return;
        m_displayWindow->refresh();
        m_displayWindow->show();
        m_displayWindow->raise();
        m_displayWindow->activateWindow();
    });
    bitmapMenu->addAction("Configure...", this, &MainWindow::onConfigureDisplay);
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar("Main");
    tb->setMovable(false);
    
    // Wire up the Run/Step/Reset actions to our slots
    m_runAction = tb->addAction("Run", this, &MainWindow::onRunClicked);
    tb->addAction("Step",  this, &MainWindow::onStepClicked);
    tb->addAction("Reset", this, [this]{
        if (m_runTimer->isActive()) m_runTimer->stop();
        m_runAction->setText("Run");
        m_emulator->reset();
        m_programLoaded = false;
        updatePanels();
    });

    tb->addSeparator();

    // --- Build the Speed Dropdown Slider ---
    QToolButton *speedBtn = new QToolButton(this);
    speedBtn->setText("Speed: Max");
    speedBtn->setPopupMode(QToolButton::InstantPopup);
    speedBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);

    QMenu *speedMenu = new QMenu(this);
    QWidgetAction *sliderAction = new QWidgetAction(speedMenu);
    QWidget *sliderWidget = new QWidget(speedMenu);
    
    auto *vbox = new QVBoxLayout(sliderWidget);
    vbox->setContentsMargins(15, 10, 15, 10);
    
    QLabel *speedLabel = new QLabel("Realtime (Max)", sliderWidget);
    QSlider *slider = new QSlider(Qt::Horizontal, sliderWidget);
    slider->setRange(1, 100);
    slider->setValue(100);
    slider->setMinimumWidth(150);

    // Update speed logic when slider moves
    connect(slider, &QSlider::valueChanged, this, [this, speedLabel, speedBtn](int val) {
        m_cpuSpeed = val;
        
        QString unit = m_emulator->m_pipelineMode ? "Hz" : "IPS"; // Ticks vs Instructions
        
        if (val == 100) {
            speedLabel->setText("Realtime (Max)");
            speedBtn->setText("Speed: Max");
        } else {
            speedLabel->setText(QString("%1 %2").arg(val).arg(unit));
            speedBtn->setText(QString("Speed: %1").arg(val));
        }
        
        if (m_runTimer && m_runTimer->isActive()) {
            m_runTimer->setInterval(1000 / m_cpuSpeed);
        }
    });

    vbox->addWidget(speedLabel);
    vbox->addWidget(slider);
    sliderAction->setDefaultWidget(sliderWidget);
    speedMenu->addAction(sliderAction);
    speedBtn->setMenu(speedMenu);
    
    tb->addWidget(speedBtn);
    // ----------------------------------------

    tb->addSeparator();

    if (m_memoryPanel) {
        QAction* toggleMemory = m_memoryPanel->toggleViewAction();
        toggleMemory->setText("Toggle Memory");
        tb->addAction(toggleMemory);
    }

    QAction *modeAction = new QAction("Mode: Standard", this);
    modeAction->setCheckable(true);
    connect(modeAction, &QAction::toggled, this, [this, modeAction](bool checked) {
        m_emulator->m_pipelineMode = checked;
        modeAction->setText(checked ? "Mode: Pipeline" : "Mode: Standard");
        
        // Reset the emulator when switching architectures to avoid corrupted state
        m_emulator->reset();
        m_programLoaded = false;
        updatePanels();
    });
    tb->addAction(modeAction);
    tb->addSeparator();

    // --- Single File / Whole Directory compile toggle ---
    QAction *selectDirAction = new QAction("Select Folder…", this);
    selectDirAction->setEnabled(false);

    QAction *dirModeAction = new QAction("Compile: Single File", this);
    dirModeAction->setCheckable(true);

    connect(dirModeAction, &QAction::toggled, this, [this, dirModeAction, selectDirAction](bool checked) {
        if (checked) {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Working Directory");
            if (dir.isEmpty()) {
                // User cancelled: don't enter directory mode.
                dirModeAction->blockSignals(true);
                dirModeAction->setChecked(false);
                dirModeAction->blockSignals(false);
                return;
            }
            m_workingDirectory = dir;
            m_directoryMode = true;
            dirModeAction->setText("Compile: Directory");
            selectDirAction->setEnabled(true);
        } else {
            m_directoryMode = false;
            dirModeAction->setText("Compile: Single File");
            selectDirAction->setEnabled(false);
        }

        // Compiling context changed; force a reassemble on the next Run/Step.
        m_runTimer->stop();
        m_runAction->setText("Run");
        m_emulator->reset();
        m_programLoaded = false;
        updatePanels();
    });

    connect(selectDirAction, &QAction::triggered, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Working Directory", m_workingDirectory);
        if (dir.isEmpty()) return;

        m_workingDirectory = dir;
        m_runTimer->stop();
        m_runAction->setText("Run");
        m_emulator->reset();
        m_programLoaded = false;
        updatePanels();
    });

    tb->addAction(dirModeAction);
    tb->addAction(selectDirAction);
    tb->addSeparator();
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
}
void MainWindow::setupDocks()
{
    // Registers panel (left side)
    m_registerPanel = new RegisterPanel(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_registerPanel);

    // Memory panel (right, hidden by default)
    m_memoryPanel = new MemoryPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_memoryPanel);
    m_memoryPanel->hide(); // <--- Added: start with memory collapsed/hidden

    // Console panel (bottom)
    m_console = new Console(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_console);

    // Pipeline panel (right, hidden by default)
    m_pipelinePanel = new PipelinePanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_pipelinePanel);
    if (m_console) {
        tabifyDockWidget(m_console, m_pipelinePanel);
    }
    m_pipelinePanel->hide(); // start with pipeline collapsed/hidden

    // Bitmap display device — its own top-level window, hidden until opened
    // from the Devices menu.
    m_displayWindow = new DisplayWindow(&m_emulator->memory(), this);
    m_displayWindow->hide();
}

void MainWindow::onConfigureDisplay()
{
    if (!m_displayWindow) return;

    static constexpr uint32_t DEFAULT_BASE   = 0x10040000u;
    static constexpr int      DEFAULT_WIDTH  = 64;
    static constexpr int      DEFAULT_HEIGHT = 32;

    auto formatAddr = [](uint32_t addr) {
        return QString("0x%1").arg(addr, 8, 16, QChar('0'));
    };

    QDialog dialog(this);
    dialog.setWindowTitle("Configure Bitmap Display");

    auto *addressEdit = new QLineEdit(formatAddr(m_displayWindow->baseAddress()), &dialog);

    auto *widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(1, 2048);
    widthSpin->setValue(m_displayWindow->widthPixels());

    auto *heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(1, 2048);
    heightSpin->setValue(m_displayWindow->heightPixels());

    auto *form = new QFormLayout();
    form->addRow("Base Address:", addressEdit);
    form->addRow("Width (px):", widthSpin);
    form->addRow("Height (px):", heightSpin);

    auto *resetBtn = new QPushButton("Reset to Default", &dialog);
    connect(resetBtn, &QPushButton::clicked, &dialog, [=] {
        addressEdit->setText(formatAddr(DEFAULT_BASE));
        widthSpin->setValue(DEFAULT_WIDTH);
        heightSpin->setValue(DEFAULT_HEIGHT);
    });
    form->addRow(resetBtn);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    auto *vbox = new QVBoxLayout(&dialog);
    vbox->addLayout(form);
    vbox->addWidget(buttons);

    uint32_t parsedAddr = m_displayWindow->baseAddress();

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        bool ok = false;
        // base 0 auto-detects a "0x" prefix, same convention used elsewhere (e.g. immediates)
        uint32_t addr = static_cast<uint32_t>(addressEdit->text().trimmed().toULong(&ok, 0));
        if (!ok) {
            QMessageBox::warning(&dialog, "Invalid Address",
                "Base address must be a decimal or 0x-prefixed hex integer.");
            return;
        }
        parsedAddr = addr;
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        m_displayWindow->configure(parsedAddr, widthSpin->value(), heightSpin->value());
    }
}

void MainWindow::updateConsole()
{
    if (m_console)
        m_console->println(m_emulator->m_buffer.m_data);
}

void MainWindow::updatePanels()
{
    // Console — show full buffer content
    if (m_console) {
        m_console->clear();
        if (!m_emulator->m_buffer.m_data.empty())
            m_console->print(m_emulator->m_buffer.m_data);
    }

    // Registers
    if (m_registerPanel)
        m_registerPanel->setRegisters(m_emulator->registers(), m_emulator->pc());

    // Memory — jump to text base only on initial load, refresh in place otherwise
    if (m_memoryPanel) {
        if (!m_programLoaded)
            m_memoryPanel->setMemory(&m_emulator->memory(), 0);
        else
            m_memoryPanel->refresh();
    }

    // Bitmap display — only bother polling memory while the window is actually visible
    if (m_displayWindow && m_displayWindow->isVisible())
        m_displayWindow->refresh();

    auto *editor = currentEditor();
    if (m_pipelinePanel && editor) {
        // Split editor text into lines so PipelinePanel can look them up by line number
        m_pipelinePanel->updatePipeline(m_emulator->m_pipelineHistory, 
            m_emulator->m_globalCycle, 
            m_sourceCache);
    }
    
    // Clear the execution highlight on every open tab first, then (if
    // applicable) re-apply it to whichever one is currently executing. This
    // ensures a Reset/finish always clears every tab, not just the active
    // one, and that stepping across files never leaves a stale highlight
    // behind in the file execution just left.
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(i)))
            ed->setExecutionLine(-1);
    }

    // Only highlight if loaded AND NOT completely finished
    if (m_programLoaded && !m_emulator->isFinished()) {
        uint32_t currentPC = m_emulator->pc();
        auto it = m_emulator->m_PCtoLineMap.find(currentPC);

        if (it != m_emulator->m_PCtoLineMap.end()) {
            uint32_t blobLine = it->second;

            // Translate the blob-relative line back to which file it came
            // from, so execution can be shown in the correct tab even when
            // multiple files were compiled together.
            QString filePath = QString::fromStdString(m_emulator->fileForBlobLine(blobLine));
            uint32_t localLine = m_emulator->localLineForBlobLine(blobLine);

            CodeEditor *target = filePath.isEmpty() ? editor : openOrFocusFile(filePath);
            if (target)
                target->setExecutionLine(static_cast<int>(localLine) - 1);
        }
    }

    // Status bar text
    if (!m_programLoaded) {
        statusBar()->showMessage("Ready");
    } else if (m_emulator->isFinished()) {
        statusBar()->showMessage("Program Terminated");
    } else if (m_emulator->isBreakpoint()) {
        statusBar()->showMessage("Paused at Breakpoint");
    } else {
        statusBar()->showMessage("Running");
    }
}

CodeEditor* MainWindow::currentEditor() const
{
    if (!m_tabWidget) return nullptr;
    return qobject_cast<CodeEditor*>(m_tabWidget->currentWidget());
}

CodeEditor* MainWindow::createEditorTab(const QString &title, const QString &filePath)
{
    CodeEditor *editor = new CodeEditor(this, m_emulator.get());
    editor->setPlaceholderText("Enter code here...");
    editor->setFont(QFont("Courier New", 11));
    editor->setTabStopDistance(QFontMetricsF(editor->font()).horizontalAdvance(' ') * 4);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFilePath(filePath);

    // Watch for typing so we can add an asterisk to the tab name
    connect(editor->document(), &QTextDocument::modificationChanged, 
            this, &MainWindow::onEditorModificationChanged);

    int index = m_tabWidget->addTab(editor, title);
    m_tabWidget->setCurrentIndex(index);
    
    // Create a completely flat tool button to replace the default 3D button
    auto *closeBtn = new QToolButton(m_tabWidget->tabBar());
    closeBtn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeBtn->setAutoRaise(true); // Removes 3D borders, frame, and background
    closeBtn->setCursor(Qt::ArrowCursor);

    // Wire the custom close button to trigger the tab close request
    connect(closeBtn, &QToolButton::clicked, this, [this, editor]() {
        int idx = m_tabWidget->indexOf(editor);
        if (idx != -1) {
            onTabCloseRequested(idx);
        }
    });

    // Attach our flat button to the right side of the tab
    m_tabWidget->tabBar()->setTabButton(index, QTabBar::RightSide, closeBtn);

    return editor;
}

CodeEditor* MainWindow::findEditorForPath(const QString &key) const
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(i));
        if (ed && ed->breakpointKey() == key)
            return ed;
    }
    return nullptr;
}

CodeEditor* MainWindow::openOrFocusFile(const QString &absPath)
{
    CodeEditor *existing = findEditorForPath(absPath);
    if (existing) {
        m_tabWidget->setCurrentWidget(existing);
        return existing;
    }

    // Only real on-disk paths can be opened fresh; a synthetic "untitled://"
    // key with no matching tab means that scratch buffer was closed.
    if (!absPath.startsWith("untitled://")) {
        QFile file(absPath);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&file);
            QString content = in.readAll();

            QFileInfo fileInfo(absPath);
            CodeEditor *editor = createEditorTab(fileInfo.fileName(), absPath);
            editor->setPlainText(content);
            editor->document()->setModified(false);
            return editor;
        }
    }
    return nullptr;
}

std::vector<SourceUnit> MainWindow::gatherCompileUnits()
{
    std::vector<SourceUnit> units;

    if (!m_directoryMode) {
        CodeEditor *editor = currentEditor();
        if (editor) {
            SourceUnit u;
            u.path = editor->breakpointKey().toStdString();
            u.text = editor->toPlainText().toStdString();
            units.push_back(std::move(u));
        }
        return units;
    }

    QDir dir(m_workingDirectory);
    QStringList fileNames = dir.entryList(QStringList() << "*.asm" << "*.s", QDir::Files, QDir::Name);

    for (const QString &fileName : fileNames) {
        QString absPath = QFileInfo(dir, fileName).absoluteFilePath();

        SourceUnit u;
        u.path = absPath.toStdString();

        CodeEditor *open = findEditorForPath(absPath);
        if (open) {
            // Use the live buffer so unsaved edits are included.
            u.text = open->toPlainText().toStdString();
        } else {
            QFile file(absPath);
            if (!file.open(QFile::ReadOnly | QFile::Text))
                throw std::runtime_error("Cannot read file: " + absPath.toStdString());
            QTextStream in(&file);
            u.text = in.readAll().toStdString();
        }
        units.push_back(std::move(u));
    }

    if (units.empty())
        throw std::runtime_error("No .asm/.s files found in " + m_workingDirectory.toStdString());

    return units;
}

bool MainWindow::loadCurrentProgram()
{
    auto units = gatherCompileUnits();
    m_emulator->loadProgramMulti(units, []{});
    m_sourceCache = QString::fromStdString(m_emulator->m_sourceBlob).split('\n');
    m_programLoaded = true;
    if (m_memoryPanel)
        m_memoryPanel->setMemory(&m_emulator->memory(), m_emulator->textBase());
    return true;
}

void MainWindow::onFileNew()
{
    createEditorTab("Untitled");

    // Reset emulator state when switching/creating files
    m_programLoaded = false;
    updatePanels();
}

void MainWindow::onFileOpen()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", "Assembly Files (*.s *.asm);;All Files (*)");
    if (fileName.isEmpty()) return;

    // If file is already open in a tab, just switch to it
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *ed = qobject_cast<CodeEditor*>(m_tabWidget->widget(i));
        if (ed && ed->filePath() == fileName) {
            m_tabWidget->setCurrentIndex(i);
            return;
        }
    }

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, "Error", "Cannot read file: " + file.errorString());
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();

    QFileInfo fileInfo(fileName);
    CodeEditor *editor = createEditorTab(fileInfo.fileName(), fileName);
    editor->setPlainText(content);
    editor->document()->setModified(false);

    // Reset emulator state when switching/creating files
    m_programLoaded = false;
    updatePanels();
}

bool MainWindow::onFileSave()
{
    return saveEditor(currentEditor(), false);
}

bool MainWindow::onFileSaveAs()
{
    return saveEditor(currentEditor(), true);
}

bool MainWindow::saveEditor(CodeEditor *editor, bool saveAs)
{
    if (!editor) return false;

    QString fileName = editor->filePath();
    if (fileName.isEmpty() || saveAs) {
        fileName = QFileDialog::getSaveFileName(this, "Save File", "", "Assembly Files (*.s *.asm);;All Files (*)");
        if (fileName.isEmpty()) return false;
    }

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, "Error", "Cannot write file: " + file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    
    editor->setFilePath(fileName);
    editor->document()->setModified(false);
    
    QFileInfo fileInfo(fileName);
    int index = m_tabWidget->indexOf(editor);
    if (index != -1) m_tabWidget->setTabText(index, fileInfo.fileName());

    return true;
}

bool MainWindow::maybeSave(int tabIndex)
{
    CodeEditor *editor = qobject_cast<CodeEditor*>(m_tabWidget->widget(tabIndex));
    if (!editor || !editor->document()->isModified()) return true;

    m_tabWidget->setCurrentIndex(tabIndex);
    QMessageBox::StandardButton ret = QMessageBox::warning(this, "Unsaved Changes",
        "The document has been modified.\nDo you want to save your changes?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save) return onFileSave();
    if (ret == QMessageBox::Cancel) return false;
    return true; // user chose Discard
}

void MainWindow::onTabCloseRequested(int index)
{
    if (maybeSave(index)) {
        QWidget *widget = m_tabWidget->widget(index);
        m_tabWidget->removeTab(index);
        widget->deleteLater();

        // Keep at least one empty tab open
        if (m_tabWidget->count() == 0) {
            onFileNew(); 
        }
    }
}

void MainWindow::onEditorModificationChanged(bool /*modified*/)
{
    // Add or remove an asterisk on the tab title if the document is unsaved
    CodeEditor *editor = qobject_cast<CodeEditor*>(sender()->parent());
    if (!editor) return;
    
    int index = m_tabWidget->indexOf(editor);
    if (index == -1) return;

    QString title = editor->filePath().isEmpty() ? "Untitled" : QFileInfo(editor->filePath()).fileName();
    if (editor->document()->isModified()) {
        title += "*";
    }
    m_tabWidget->setTabText(index, title);
}

void MainWindow::onRunClicked()
{
    // If it's already running slowly via timer, pause it.
    if (m_runTimer->isActive()) {
        m_runTimer->stop();
        m_runAction->setText("Run");
        updatePanels();
        return;
    }

    try {
        if (!m_programLoaded || m_emulator->isFinished()) {
            loadCurrentProgram();
        }

        if (m_cpuSpeed == 100) {
            // MAX SPEED: Run blocking loop
            m_emulator->run([]{});
            updatePanels();
        } else {
            // SLOW SPEED: Start the tick timer and change button to "Pause"
            m_runAction->setText("Pause");
            m_runTimer->start(1000 / m_cpuSpeed);
        }
    } catch (const std::exception &e) {
        std::string msg = m_emulator->annotateFileLines(e.what());
        m_emulator->m_buffer.m_data += "\n[Error] " + msg;
        updatePanels();
        QMessageBox::warning(this, "Runtime Exception", QString::fromStdString(msg));
    }
}

void MainWindow::onStepClicked()
{
    // If timer is running, pause it first
    if (m_runTimer->isActive()) {
        m_runTimer->stop();
        m_runAction->setText("Run");
    }

    try {
        if (!m_programLoaded || m_emulator->isFinished()) {
            loadCurrentProgram();
        } else {
            m_emulator->step();
        }
    } catch (const std::exception &e) {
        std::string msg = m_emulator->annotateFileLines(e.what());
        m_emulator->m_buffer.m_data += "\n[Error] " + msg;
        updatePanels();
        QMessageBox::warning(this, "Runtime Exception", QString::fromStdString(msg));
        return;
    }
    updatePanels();
}

void MainWindow::onRunTimerTick()
{
    try {
        m_emulator->step();
        updatePanels();
        
        // Stop the timer if the program finished or hit a breakpoint
        if (m_emulator->halted()) {
            m_runTimer->stop();
            m_runAction->setText("Run");
        }
    } catch (const std::exception &e) {
        m_runTimer->stop();
        m_runAction->setText("Run");
        std::string msg = m_emulator->annotateFileLines(e.what());
        m_emulator->m_buffer.m_data += "\n[Error] " + msg;
        updatePanels();
        QMessageBox::warning(this, "Runtime Exception", QString::fromStdString(msg));
    }
}