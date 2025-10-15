QT += core gui widgets concurrent
TARGET = HDFVisualizer
TEMPLATE = app

CONFIG += c++17
QMAKE_CXXFLAGS += -m64 -utf-8
DEFINES += _CRT_SECURE_NO_WARNINGS

# 头文件路径
INCLUDEPATH += D:/gdal-3.7.1-install/include
INCLUDEPATH += D:/hdf4-2.16.2-dynamic/include
INCLUDEPATH += D:/proj-9.2.1-install/include

# 库文件路径
LIBS += -L"D:/hdf4-2.16.2-dynamic/lib" -lhdf -lmfhdf
LIBS += -L"D:/proj-9.2.1-install/lib" -lproj
LIBS += -L"D:/gdal-3.7.1-install/lib" -lgdal
LIBS += -lws2_32

SOURCES += \
    geoutils.cpp \
    main.cpp \
    mainwindow.cpp \
    rasterdata.cpp \
    viewcontroller.cpp \
    viewrenderer.cpp \
    viewwidget.cpp

HEADERS += \
    geoutils.h \
    mainwindow.h \
    rasterdata.h \
    viewcontroller.h \
    viewrenderer.h \
    viewwidget.h

FORMS += \
    mainwindow.ui
