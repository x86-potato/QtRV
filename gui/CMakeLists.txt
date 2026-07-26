add_executable(QtMips
    src/main.cpp
    src/MainWindow.cpp
    src/Console.cpp
    src/Console.h
    src/MemoryPanel.cpp
    src/MemoryPanel.h
    src/RegisterPanel.cpp
    src/RegisterPanel.h
    src/CodeEditor.cpp
    src/CodeEditor.h
    src/PipelinePanel.h
    src/PipelinePanel.cpp
    src/AsmHighlighter.h
    src/AsmHighlighter.cpp
    src/DisplayWindow.h
    src/DisplayWindow.cpp
)

set_target_properties(QtMips PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_link_libraries(QtMips
    PRIVATE core
    PRIVATE Qt5::Widgets
)