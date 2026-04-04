cmake_minimum_required(VERSION 3.16)
project(SonoStega VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

set(SOURCES
    src/main.cpp
    src/core/io/WavFile.cpp
    src/core/steganography/LsbEmbedder.cpp
    src/core/analysis/QualityAnalyzer.cpp
    src/ui/MainWindow.cpp
    src/ui/embed/EmbedWidget.cpp
    src/ui/extract/ExtractWidget.cpp
    src/ui/analysis/AnalysisWidget.cpp
)

set(HEADERS
    src/core/io/WavFile.h
    src/core/steganography/LsbEmbedder.h
    src/core/analysis/QualityAnalyzer.h
    src/ui/MainWindow.h
    src/ui/embed/EmbedWidget.h
    src/ui/extract/ExtractWidget.h
    src/ui/analysis/AnalysisWidget.h
)

qt_add_executable(SonoStega
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
)

target_include_directories(SonoStega PRIVATE src)

target_link_libraries(SonoStega PRIVATE
    Qt6::Widgets
    Qt6::Charts
)

if(WIN32)
    set_target_properties(SonoStega PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
