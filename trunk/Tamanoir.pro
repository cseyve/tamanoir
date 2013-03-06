# #####################################################################
# TAMANOIR Qt PROJECT
# #####################################################################
TEMPLATE = app

# Use lowercase name for Linux
TARGET = tamanoir

# and an uppercase first letter for Mac & Windows
mac::TARGET = Tamanoir
win32::TARGET = Tamanoir


DEPENDPATH += . \
    inc \
    src \
    ui
INCLUDEPATH += . \
    inc \
    ui
OBJECTS_DIR = .obj-simple

DEFINES += SIMPLE_VIEW 

CONFIG += qt \
    warn_on

win32: {
    CONFIG += debug_and_release
    OBJECTS_DIR = obj
    MOC_DIR = moc
}
TRANSLATIONS = tamanoir_French.ts

# icon
# reference : file:///usr/share/qt4/doc/html/appicon.html
mac::ICON = icon/Tamanoir.icns
win32: {
	RC_FILE = icon/tamanoir.rc
	ICON = icon/tamanoir32.ico
}
linux-g++::ICON = icon/Tamanoir32.png
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4

# SOURCES -= paintwidget_win.cpp
# Input
HEADERS = inc/imgproc.h \
	inc/imgutils.h \
	inc/tamanoir.h \
	inc/qimagedisplay.h \
	inc/prefsdialog.h \
	inc/tamanoirwizard.h
FORMS = ui/tamanoir_simple.ui \
	ui/prefsdialog.ui \
	ui/tamanoirwizard.ui

SOURCES = src/imgproc.cpp \
	src/imgutils.cpp \
	src/main.cpp \
	src/tamanoir.cpp \
	src/qimagedisplay.cpp \
	src/prefsdialog.cpp \
	src/tamanoirwizard.cpp

linux-g++:TMAKE_CXXFLAGS += -W \
    -Wall \
    -g \
    -O2 \
    -fexceptions \
    -Wimplicit \
    -Wreturn-type \
    -Wunused \
    -Wswitch \
    -Wcomment \
    -Wuninitialized \
    -Wparentheses \
    -Wpointer-arith

# -Wshadow
linux-g++::DEFINES += LINUX
LIBS_EXT = so
macx::LIBS_EXT = dylib
linux-g++::LIBS_EXT = so
linux:LIBS_EXT = so
win32:LIBS_EXT = lib
message( "Installation directory = $(PWD) ")

LIBS = 
DYN_LIBS = 
STATIC_LIBS = 
OPENCV_STATIC_LIBDIR =

# opencv.pri check several directories to find where openCV is installed
include(opencv.pri)

unix: { 
    
    # Test if libtiff is installed ==============================
    exists( /usr/local/include/tiffio.h ) { 
        INCLUDEPATH += /usr/local/include
        DYN_LIBS += -L/usr/local/lib \
            -ltiff
        SOURCES += src/cv2tiff.cpp
        DEFINES += HAVE_LIBTIFF
        STATIC_LIBS += /usr/local/lib/libtiff.a
    }
    else { 
        exists( /opt/local/include/tiffio.h ) { 
            INCLUDEPATH += /opt/local/include
            DYN_LIBS += -L/opt/local/lib \
                -ltiff
            SOURCES += src/cv2tiff.cpp
            DEFINES += HAVE_LIBTIFF
            STATIC_LIBS += /opt/local/lib/libtiff.a
        }
        else { 
            exists( /sw/include/tiffio.h ) { 
                INCLUDEPATH += /sw/include
                DYN_LIBS += -L/sw/lib \
                    -ltiff
                SOURCES += src/cv2tiff.cpp
                DEFINES += HAVE_LIBTIFF
                STATIC_LIBS += /sw/lib/libtiff.a
            }
            else:exists( /usr/include/tiffio.h ) { 
                INCLUDEPATH += /usr/include
                DYN_LIBS += -ltiff
                SOURCES += src/cv2tiff.cpp
                DEFINES += HAVE_LIBTIFF
                STATIC_LIBS += /usr/lib/libtiff.a
            }
        }
    }
    
    INSTALL_DUSTHOOVER = $(DUSTHOOVER_DIR)
    message( Reading installation directory : '$$INSTALL_DUSTHOOVER')
    count( $$INSTALL_DUSTHOOVER , 0 ):message("Installation directory is undefined !! Installing in '/usr/local/tamanoir'.")
    else { 
        message( Installing in : $$INSTALL_DUSTHOOVER)
        DEFINES += BASE_DIRECTORY=$$INSTALL_DUSTHOOVER
        message( defines : $$DEFINES )
    }
}

# libtool --mode=link g++ -o Tamanoir .obj-simple/imgproc.o .obj-simple/imgutils.o .obj-simple/main.o .obj-simple/tamanoir.o .obj-simple/qimagedisplay.o .obj-simple/cv2tiff.o .obj-simple/moc_tamanoir.o .obj-simple/moc_qimagedisplay.o .obj-simple/qrc_tamanoir.o -L/usr/lib /usr/lib/libtiff.a /usr/local/lib/libcxcore.la /usr/local/lib/libcv.la /usr/local/lib/libcvaux.la /usr/local/lib/libhighgui.la -lQtGui -lQtCore -lpthread

# $$OPENCV_STATIC_LIBDIR/libjpeg.a
# Build static if linked statically with a patched version of OpenCV for 16bit TIFF pictures
# ./configure --with-ffmpeg=no --with-tiff=yes --with-v4l=no --with-v4l2=no --with-gtk=no
BUILD_STATIC = $$(BUILD_STATIC)

# contains(BUILD_STATIC, true)
macx:contains(BUILD_STATIC, true) { 
    message("Building static version of binary : Universal=x86 ppc")
    CONFIG += x86 \
        ppc
    
    # Test for building releases
    LIBS += $$STATIC_LIBS
}
else { 
    message("Building dynamic libraries version of binary :")
    CONFIG += debug
    
    # Dynamic libraries version
    LIBS += $$DYN_LIBS
}
OTHER_FILES += build_mac_bundle.sh \
    build_mac_dmg.py \
    docs/Tamanoir-FR_AnnonceForums.txt \
    qss/tamanoir-Gray.qss \
    opencv.pri
macx: { 
    message("MacOS X specific options =================================================")
    
    # for Qt4.6, we need to add ApplicationServices for the "Open With" with finder
    LIBS += -framework \
        ApplicationServices
}

# TARGET = $$join(TARGET,,,_debug)
# DEFINES += "TRANSLATION_DIR=\"Tamanoir.app/Contents/\""
linux-g++ { 
    message("Linux specific options =================================================")
    DEFINES += "TRANSLATION_DIR=/usr/share/tamanoir"
}
win32:TARGET = $$join(TARGET,,d)

# }
unix::DEFINES += VERSION_YY="`date +%Y`" \
    VERSION_MM="`date +%m | sed 's/0//'`" \
    VERSION_DD="`date +%d | sed 's/0//'`" \
    __LINUX__
win32:DEFINES += VERSION_YY="2010" \
	VERSION_MM="03" \
    VERSION_DD="13"

# linux-g++::CONFIG += debug_and_release
# commented to build debug version on macx
# debug \
# release \
# # INSTALLATION
# target.path = /usr/local/tamanoir
# INSTALLS += target
# FINAL CONFIGURATION ==================================================
message( "")
message( "")
message( "FINAL CONFIGURATION ================================================== ")
message( "Configuration : ")
message( " config : $$CONFIG ")
message( " defs : $$DEFINES ")
message( " libs : $$LIBS ")
message( "FINAL CONFIGURATION ================================================== ")
message( "")
message( "")
RESOURCES += tamanoir.qrc
