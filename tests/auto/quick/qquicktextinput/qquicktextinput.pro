CONFIG += testcase
TARGET = tst_qquicktextinput
macx:CONFIG -= app_bundle

SOURCES += tst_qquicktextinput.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private v8-private qml-private quick-private testlib

win32:CONFIG += insignificant_test # QTBUG-24790
