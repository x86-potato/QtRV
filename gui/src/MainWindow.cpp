#include "MainWindow.h"
#include "Console.h"
#include "MemoryPanel.h"
#include "RegisterPanel.h"
#include "CodeEditor.h"

#include <stdexcept>
#include <QApplication>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QtRV");
    resize(1200, 800);

    m_emulator = std::make_unique<Emulator>();

    m_emulator->setInputCallback([this](const std::string& prompt) -> std::string {
        bool ok = false;
        QString result = QInputDialog::getText(
            this,
            "Program Input",
            QString::fromStdString(prompt),
            QLineEdit::Normal, "", &ok);
        return ok ? result.toStdString() : "";
    });

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
    runMenu->addAction("&Assemble && Run", this, [this]{
        try {
            m_emulator->loadProgram(textFromEditor(), []{});
            m_programLoaded = true;
            if (m_memoryPanel)
                m_memoryPanel->setMemory(&m_emulator->memory(), m_emulator->textBase());
            m_emulator->run([]{});
        } catch (const std::exception &e) {
            m_emulator->m_buffer.m_data += std::string("\n[Error] ") + e.what();
        }
        updatePanels();
    }, QKeySequence("F5"));
    runMenu->addAction("&Step", this, [this]{
        try {
            if (!m_programLoaded) {
                m_emulator->loadProgram(textFromEditor(), []{});
                m_programLoaded = true;
                if (m_memoryPanel)
                    m_memoryPanel->setMemory(&m_emulator->memory(), m_emulator->textBase());
            }
            else
            {
                m_emulator->step();
            }

        } catch (const std::exception &e) {
            m_emulator->m_buffer.m_data += std::string("\n[Error] ") + e.what();
        }
        updatePanels();
    }, QKeySequence("F10"));
    runMenu->addAction("&Reset", this, [this]{
        m_emulator->reset();
        m_programLoaded = false;
        updatePanels();
    });

    // Added: View Menu to toggle panels on and off
    QMenu *viewMenu = menuBar()->addMenu("&View");
    if (m_registerPanel) viewMenu->addAction(m_registerPanel->toggleViewAction());
    if (m_memoryPanel)   viewMenu->addAction(m_memoryPanel->toggleViewAction());
    if (m_console)       viewMenu->addAction(m_console->toggleViewAction());
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar("Main");
    tb->setMovable(false);
    
    tb->addAction("Run",   this, [this]{
        try {
            if (!m_programLoaded || m_emulator->isFinished()) {                m_emulator->loadProgram(textFromEditor(), []{});
                m_programLoaded = true;
                if (m_memoryPanel)
                    m_memoryPanel->setMemory(&m_emulator->memory(), m_emulator->textBase());
            }
            
            m_emulator->run([]{});
        } catch (const std::exception &e) {
            m_emulator->m_buffer.m_data += std::string("\n[Error] ") + e.what();
        }
        updatePanels();
    });
    
    tb->addAction("Step",  this, [this]{
        try {
            if (!m_programLoaded || m_emulator->isFinished()) {
                m_emulator->loadProgram(textFromEditor(), []{});
                m_programLoaded = true;
                if (m_memoryPanel)
                    m_memoryPanel->setMemory(&m_emulator->memory(), m_emulator->textBase());
            }
            else
            {
                m_emulator->step();
            }
        } catch (const std::exception &e) {
            m_emulator->m_buffer.m_data += std::string("\n[Error] ") + e.what();
        }
        updatePanels();
    });
    
    tb->addAction("Reset", this, [this]{
        m_emulator->reset();
        m_programLoaded = false;
        updatePanels();
    });

    tb->addSeparator();

    // Add a checkable button to the toolbar to quickly toggle the Memory Panel
    if (m_memoryPanel) {
        QAction* toggleMemory = m_memoryPanel->toggleViewAction();
        toggleMemory->setText("Toggle Memory");
        tb->addAction(toggleMemory);
    }
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
    
    auto *editor = currentEditor();
    if (editor) {
        // Only highlight if loaded AND NOT completely finished
        if (m_programLoaded && !m_emulator->isFinished()) {
            uint32_t currentPC = m_emulator->pc();
            
            if (m_emulator->m_PCtoLineMap.find(currentPC) != m_emulator->m_PCtoLineMap.end()) {
                uint32_t zeroIndexedLine = m_emulator->m_PCtoLineMap[currentPC] - 1; 
                editor->setExecutionLine(zeroIndexedLine);
            } else {
                editor->setExecutionLine(-1);
            }
        } else {
            // Program is reset or finished, clear the green highlight
            editor->setExecutionLine(-1);
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

const std::string &MainWindow::textFromEditor() const
{
    static std::string text; // avoid returning reference to temporary
    CodeEditor *editor = currentEditor();
    if (editor) {
        text = editor->toPlainText().toStdString();
        return text;
    }
    static std::string empty;
    return empty;
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

    // Reset emulator state when switching/creating files
    m_programLoaded = false;
    updatePanels();
    
    return editor;
}

void MainWindow::onFileNew()
{
    createEditorTab("Untitled");
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