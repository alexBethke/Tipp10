include(../tipp10.pro)
TARGET = tipp10-smoke
CONFIG -= app_bundle
QT += testlib
SOURCES -= main.cpp
ROOT = $$clean_path($$PWD/..)
for(source, SOURCES): ABS_SOURCES += $$ROOT/$$source
for(header, HEADERS): ABS_HEADERS += $$ROOT/$$header
SOURCES = $$ABS_SOURCES $$PWD/smoke.cpp
HEADERS = $$ABS_HEADERS
RESOURCES = $$ROOT/tipp10.qrc $$TIPP10_DATABASE_QRC
INCLUDEPATH += $$ROOT
