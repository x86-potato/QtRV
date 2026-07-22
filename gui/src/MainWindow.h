#pragma once

#include <QMainWindow>
#include <Emulator.h>


#include <memory>

class QTabWidget;
class CodeEditor;
class Console;
class MemoryPanel;
class RegisterPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onFileNew();
    void onFileOpen();
    bool onFileSave();
    bool onFileSaveAs();
    void onTabCloseRequested(int index);
    void onEditorModificationChanged(bool modified);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDocks();
    void setupCentralWidget();

    void updateConsole();
    void updatePanels();   // refresh registers, memory, console after any CPU change

    CodeEditor* currentEditor() const;
    CodeEditor* createEditorTab(const QString &title, const QString &filePath = "");
    bool saveEditor(CodeEditor *editor, bool saveAs);
    bool maybeSave(int tabIndex);

    QTabWidget *m_tabWidget = nullptr;

    const std::string &textFromEditor() const;

    std::unique_ptr<Emulator> m_emulator;
    Console       *m_console       = nullptr;
    MemoryPanel   *m_memoryPanel   = nullptr;
    RegisterPanel *m_registerPanel = nullptr;
    bool           m_programLoaded = false;
};
