# #####################################################################
# Automatically generated by qmake (2.01a) Mon Sep 1 03:56:07 2008
# #####################################################################
TEMPLATE = app

# Input
HEADERS += gameloop.h \
    connecttoserverdialog.h \
    mainwindow.h \
    serverconnection.h \
    common.h \
    joingamedialog.h \
    logwidget.h \
    chatwidget.h \
    card.h \
    opponentwidget.h \
    game.h \
    cardwidget.h \
    creategamedialog.h \
    playerwidget.h \
    playercharacterwidget.h \
    cardpilewidget.h \
    deckwidget.h \
    cardpocket.h \
    localplayerwidget.h \
    cardlist.h \
    gameevent.h \
    cardmovementevent.h \
    gameeventqueue.h \
    gameeventhandler.h \
    gamecontextchangeevent.h \
    gamesyncevent.h \
    lifepointschangeevent.h \
    cardactionswidget.h \
    cardwidgetfactory.h \
    gameobjectclickhandler.h \
    playerdiedevent.h \
    playerevent.h
FORMS += connecttoserverdialog.ui \
    mainwindow.ui \
    joingamedialog.ui \
    logwidget.ui \
    chatwidget.ui \
    opponentwidget.ui \
    creategamedialog.ui \
    localplayerwidget.ui
SOURCES += gameloop.cpp \
    main.cpp \
    connecttoserverdialog.cpp \
    mainwindow.cpp \
    serverconnection.cpp \
    common.cpp \
    joingamedialog.cpp \
    logwidget.cpp \
    chatwidget.cpp \
    card.cpp \
    opponentwidget.cpp \
    game.cpp \
    cardwidget.cpp \
    creategamedialog.cpp \
    playerwidget.cpp \
    playercharacterwidget.cpp \
    cardpilewidget.cpp \
    deckwidget.cpp \
    localplayerwidget.cpp \
    cardlist.cpp \
    gameevent.cpp \
    cardmovementevent.cpp \
    gameeventqueue.cpp \
    gameeventhandler.cpp \
    gamecontextchangeevent.cpp \
    gamesyncevent.cpp \
    lifepointschangeevent.cpp \
    cardactionswidget.cpp \
    cardwidgetfactory.cpp \
    gameobjectclickhandler.cpp \
    cardpocket.cpp \
    playerdiedevent.cpp \
    playerevent.cpp
QT += network \
    xml
RESOURCES += client.qrc
INCLUDEPATH += ../common
LIBS += ../common/libcommon.a
TARGETDEPS += ../common/libcommon.a
CONFIG -= release
CONFIG += debug
QMAKE_CXXFLAGS_DEBUG += -Wall
