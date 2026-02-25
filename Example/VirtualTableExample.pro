# Qt项目文件
QT += core gui concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = VirtualTableExample
TEMPLATE = app

# 包含路径
INCLUDEPATH += \
    $$PWD \
    $$PWD/../VirtualTable

# 源文件
SOURCES += \
    $$PWD/main.cpp \
    $$PWD/MainWindow.cpp \
    $$PWD/../VirtualTable/VirtualTableView.cpp \
    $$PWD/../VirtualTable/VirtualTableModel.cpp \
    $$PWD/../VirtualTable/SampleDataSource.cpp \
    $$PWD/../VirtualTable/CsvDataSource.cpp


# 头文件
HEADERS += \
    $$PWD/MainWindow.h \
    $$PWD/../VirtualTable/VirtualTableView.h \
    $$PWD/../VirtualTable/VirtualTableModel.h \
    $$PWD/../VirtualTable/DataSource.h \
    $$PWD/../VirtualTable/SampleDataSource.h \
    $$PWD/../VirtualTable/CsvDataSource.h

# 编译标志
QMAKE_CXXFLAGS += -std=c++17
msvc {
    QMAKE_CFLAGS += /utf-8
    QMAKE_CXXFLAGS += /utf-8
}
