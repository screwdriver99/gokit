cmake_minimum_required(VERSION 3.5)

project(gokit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

qt_add_executable(gokit
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(gokit
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    "-framework Foundation"
    "-framework CoreBluetooth")

target_include_directories(gokit
    PRIVATE "../ultragui/include"
    PRIVATE "../blewrapper/include")

target_link_directories(gokit
    PRIVATE "../ultragui/lib"
    PRIVATE "../blewrapper/lib")

target_link_libraries(gokit
    PRIVATE ultragui-d
    PRIVATE blewrappermacos-d)

set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.gokit)

set_target_properties(gokit PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_INFO_PLIST Info.plist.in
)

include(GNUInstallDirs)
install(TARGETS gokit
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(gokit)

