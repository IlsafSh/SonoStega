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
    src/core/WavFile.cpp
    src/core/LsbEmbedder.cpp
    src/core/QualityAnalyzer.cpp
    src/ui/MainWindow.cpp
)

set(HEADERS
    src/core/WavFile.h
    src/core/LsbEmbedder.h
    src/core/QualityAnalyzer.h
    src/ui/MainWindow.h
)

qt_add_executable(SonoStega
    ${SOURCES}
    ${HEADERS}
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
