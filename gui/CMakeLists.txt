add_executable(QTRv
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
)

set_target_properties(QTRv PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_link_libraries(QTRv
    PRIVATE core
    PRIVATE Qt5::Widgets
)