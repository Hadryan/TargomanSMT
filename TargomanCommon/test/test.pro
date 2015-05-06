################################################################################
# Copyright © 2012-2015, Targoman.com
#
# Published under the terms of TCRL(Targoman Community Research License)
# You can find a copy of the license file with distributed source or
# download it from http://targoman.com/License.txt
#
################################################################################
BasePath = ".."

# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
HEADERS += \
    sampleAgent.hpp

# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
SOURCES += \
    main.cpp

################################################################################
#                       DO NOT CHANGE ANYTHING BELOW                           #
################################################################################
ConfigFile = $$BasePath/Configs.pri
!exists($$ConfigFile){
error("**** libsrc: Unable to find Configuration file $$ConfigFile ****")
}
include ($$ConfigFile)

TEMPLATE = app
TARGET = test_$$ProjectName
DESTDIR = $$BaseTestBinFolder
OBJECTS_DIR = $$BaseBuildFolder/$$TARGET/obj
MOC_DIR = $$BaseBuildFolder/$$TARGET/moc
INCLUDEPATH += $$BasePath/libsrc \
               $$BasePath/libsrc/lib$$ProjectName
QMAKE_LIBDIR += $$BaseLibraryFolder
LIBS += -l$$ProjectName
