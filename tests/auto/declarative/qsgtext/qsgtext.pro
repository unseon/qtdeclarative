load(qttest_p4)
contains(QT_CONFIG,declarative): QT += declarative gui
QT += network
macx:CONFIG -= app_bundle

SOURCES += tst_qsgtext.cpp

INCLUDEPATH += ../shared/
HEADERS += ../shared/testhttpserver.h
SOURCES += ../shared/testhttpserver.cpp

symbian: {
    importFiles.files = data
    importFiles.path = .
    DEPLOYMENT += importFiles
} else {
    DEFINES += SRCDIR=\\\"$$PWD\\\"
}

CONFIG += parallel_test

QT += core-private gui-private v8-private declarative-private
QT += opengl-private
