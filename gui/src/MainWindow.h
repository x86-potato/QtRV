#pragma once

#include <QMainWindow>
#include <Emulator.h>


#include <memory>

class Console;
class MemoryPanel;
class RegisterPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDocks();
    void setupCentralWidget();

    void updateConsole();
    void updatePanels();   // refresh registers, memory, console after any CPU change

    const std::string &textFromEditor() const;

    std::unique_ptr<Emulator> m_emulator;
    Console       *m_console       = nullptr;
    MemoryPanel   *m_memoryPanel   = nullptr;
    RegisterPanel *m_registerPanel = nullptr;
    bool           m_programLoaded = false;
};
