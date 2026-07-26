#pragma once

#include <QMainWindow>
#include <Emulator.h>


#include <memory>

class QTabWidget;
class CodeEditor;
class Console;
class MemoryPanel;
class RegisterPanel;
class PipelinePanel;
class DisplayWindow;

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
    void onRunClicked();
    void onStepClicked();
    void onRunTimerTick();


private:
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDocks();
    void setupCentralWidget();
    void onConfigureDisplay(); // opens the "Configure Bitmap Display" dialog

    QTimer *m_runTimer = nullptr;
    QAction *m_runAction = nullptr;
    QStringList m_sourceCache;

    int m_cpuSpeed = 100; // 1-100 (100 = Realtime)

    void updateConsole();
    void updatePanels();   // refresh registers, memory, console after any CPU change

    CodeEditor* currentEditor() const;
    CodeEditor* createEditorTab(const QString &title, const QString &filePath = "");
    bool saveEditor(CodeEditor *editor, bool saveAs);
    bool maybeSave(int tabIndex);

    QTabWidget *m_tabWidget = nullptr;

    // Multi-file / whole-directory compilation
    bool m_directoryMode = false;
    QString m_workingDirectory;
    std::vector<SourceUnit> gatherCompileUnits();
    CodeEditor* findEditorForPath(const QString &absPath) const;
    CodeEditor* openOrFocusFile(const QString &absPath); // opens from disk if no tab has it yet
    bool loadCurrentProgram(); // gathers compile units and loads them into the emulator; false on assemble error

    std::unique_ptr<Emulator> m_emulator;
    Console       *m_console       = nullptr;
    MemoryPanel   *m_memoryPanel   = nullptr;
    RegisterPanel *m_registerPanel = nullptr;
    PipelinePanel *m_pipelinePanel = nullptr;
    DisplayWindow *m_displayWindow = nullptr;
    bool           m_programLoaded = false;
};
