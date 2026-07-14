#include "MainWindow.h"
#include "Console.h"
#include "MemoryPanel.h"
#include "RegisterPanel.h"

#include <stdexcept>
#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QAction>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QtRV");
    resize(1200, 800);

    m_emulator = std::make_unique<Emulator>();

    // Wire up the input dialog: called whenever the program executes syscall 5 or 8.
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
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDocks();
}

void MainWindow::setupCentralWidget()
{
    auto *editor = new QPlainTextEdit(this);
    editor->setPlaceholderText("Enter RISC-V assembly here...");
    editor->setFont(QFont("Courier New", 11));
    setCentralWidget(editor);
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New",  this, []{ /* TODO */ }, QKeySequence::New);
    fileMenu->addAction("&Open", this, []{ /* TODO */ }, QKeySequence::Open);
    fileMenu->addAction("&Save", this, []{ /* TODO */ }, QKeySequence::Save);
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
            m_emulator->step();
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
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar("Main");
    tb->setMovable(false);
    tb->addAction("Run",   this, [this]{
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
    });
    tb->addAction("Step",  this, [this]{
        try {
            if (!m_programLoaded) {
                m_emulator->loadProgram(textFromEditor(), []{});
                m_programLoaded = true;
                if (m_memoryPanel)
                    m_memoryPanel->setMemory(&m_emulator->memory(), m_emulator->textBase());
            }
            m_emulator->step();
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
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
}

void MainWindow::setupDocks()
{
    // Registers panel (right side)
    m_registerPanel = new RegisterPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_registerPanel);

    // Memory panel (right, tabbed with registers)
    m_memoryPanel = new MemoryPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_memoryPanel);
    tabifyDockWidget(m_registerPanel, m_memoryPanel);

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

    // Status bar
    statusBar()->showMessage(m_emulator->halted() ? "Halted" : "Running");
}

const std::string &MainWindow::textFromEditor() const
{
    auto *editor = qobject_cast<QPlainTextEdit *>(centralWidget());
    if (editor) {
        static std::string text; // avoid returning reference to temporary
        text = editor->toPlainText().toStdString();
        return text;
    }
    static std::string empty;
    return empty;
}

