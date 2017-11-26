QT += core
QT += gui

CONFIG += c++11

TARGET = dual1080p
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

#SOURCES += main.cpp \
SOURCES += \
    iniparser.c \
    dictionary.c \
    clog.c \
    main.cpp \
    configdata.cpp \
    rtmp_h264.c \
    compose.c \
    audio.c \
    stream_distribute.c \
    container.c \
    sqlite-amalgamation-3210000/sqlite3.c \
    disk_manage.c \
    osd.c \
    tmp_fun.c \
    text2bitmap.c \
    utils.c \
    utf2unicode.c

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += ff_lib/include mpp/include

LIBS += -L/home/ln/hisi/dual1080p/ff_lib/lib \
        -lavformat \
        -lavcodec \
        -lswresample \
        -lavdevice \
        -lavfilter \
        -lavutil \
        -L/home/ln/hisi/dual1080p/mpp/lib \
        -lmpi \
        -lhdmi \
        -lVoiceEngine \
        -lupvqe \
        -ldnvqe \
        -ljpeg \
        -ldl \
        -lrt \
        -L/home/ln/hisi/dual1080p \
        -lfreetype \
        -lz

INCLUDEPATH += sqlite-amalgamation-3210000


HEADERS += \
    streamblock.h \
    rtmp_h264.h \
    iniparser.h \
    dictionary.h \
    debug.h \
    clog.h \
    configdata.h \
    compose.h \
    stream_distribute.h \
    container.h \
    disk_manage.h \
    sqlite-amalgamation-3210000/sqlite3.h \
    sqlite-amalgamation-3210000/sqlite3ext.h \
    osd.h \
    audio.h \
    text2bitmap.h \
    common.h \
    utils.h \
    utf2unicode.h
