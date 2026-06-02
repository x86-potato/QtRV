#include "MainWindow.h"
#include "Console.h"
#include "MemoryPanel.h"
#include "RegisterPanel.h"


#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QAction>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QtRV");
    resize(1200, 800);

    m_emulator = std::make_unique<Emulator>();

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
    runMenu->addAction("&Assemble && Run", this, [this]{ m_emulator->loadProgram(textFromEditor(), [this]{updateConsole();}); }, QKeySequence("F5"));
    runMenu->addAction("&Step",            this, [this]{  }, QKeySequence("F10"));
    runMenu->addAction("&Reset",           this, [this]{ m_emulator->reset(); });
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar("Main");
    tb->setMovable(false);
    tb->addAction("Run",   this, [this]{ m_emulator->loadProgram(textFromEditor(), [this]{ updateConsole(); }); });
    tb->addAction("Step",  this, [this]{  });
    tb->addAction("Reset", this, [this]{ m_emulator->reset(); });
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

