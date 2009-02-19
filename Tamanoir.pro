# #####################################################################
# TAMANOIR Qt PROJECT
# #####################################################################
TEMPLATE = app
TARGET = Tamanoir
DEPENDPATH += . \
    inc \
    src \
    ui
INCLUDEPATH += . \
    inc \
    ui
OBJECTS_DIR = .obj-simple
DEFINES += SIMPLE_VIEW \
    QT3_SUPPORT
TRANSLATIONS = tamanoir_fr.ts

# icon
# reference : file:///usr/share/qt4/doc/html/appicon.html
mac::ICON = icon/Tamanoir.icns
win32::RC_FILE = icon/tamanoir.rc
linux-g++::ICON = icon/Tamanoir32.png
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4

# Input
HEADERS = inc/imgproc.h \
    inc/imgutils.h \
    inc/tamanoir.h \
    inc/qimagedisplay.h
FORMS = ui/tamanoir_simple.ui
SOURCES = src/imgproc.cpp \
    src/imgutils.cpp \
    src/main.cpp \
    src/tamanoir.cpp \
    src/qimagedisplay.cpp
linux-g++:TMAKE_CXXFLAGS += -Wall \
    -O2 \
    -g \
    -fexceptions \
    -Wimplicit \
    -Wreturn-type \
    -Wunused \
    -Wswitch \
    -Wcomment \
    -Wuninitialized \
    -Wparentheses \
    -Wpointer-arith
linux-g++:DEFINES += LINUX
LIBS_EXT = dylib
linux-g++:LIBS_EXT = so
win32:LIBS_EXT = lib
message( "Installation directory = $(PWD) ")
LIBS = 
DYN_LIBS = 
STATIC_LIBS = 
win32: { 
    message("Win32 specific paths : OpenCV must be installed in C:\Program Files\OpenCV")
    INCLUDEPATH += "C:\Program Files\OpenCV\cxcore\include"
    INCLUDEPATH += "C:\Program Files\OpenCV\cv\include"
    INCLUDEPATH += "C:\Program Files\OpenCV\cvaux\include"
    INCLUDEPATH += "C:\Program Files\OpenCV\otherlibs\highgui"
    DYN_LIBS += -L"C:\Program Files\OpenCV\lib" \
        -L"C:\Program Files\OpenCV\bin" \
        -lcxcore \
        -lcv
}
unix: { 
    # Test if libtiff is installed
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
                DYN_LIBS += -L/usr/lib \
                    -ltiff
                SOURCES += src/cv2tiff.cpp
                DEFINES += HAVE_LIBTIFF
                STATIC_LIBS += /usr/lib/libtiff.a
            }
        }
    }
    
    # Test if OpenCV library is present
    OPENCV_STATIC_LIBDIR = 
    exists( /opt/local/include/opencv/cv.hpp) { 
        message("OpenCV found in /opt/local/include/opencv.")
        INCLUDEPATH += /opt/local/include/opencv
        CVINSTPATH = /opt/local
        CVINCPATH = /opt/local/include/opencv
        DYN_LIBS += -L/opt/local/lib
        OPENCV_STATIC_LIBDIR = /opt/local/lib
    }
    else { 
        exists( /usr/local/include/opencv/cv.hpp ) { 
            message("OpenCV found in /usr/local/include.")
            INCLUDEPATH += /usr/local/include/opencv
            CVINSTPATH = /usr/local
            CVINCPATH = /usr/local/include/opencv
            DYN_LIBS += -L/usr/local/lib
            OPENCV_STATIC_LIBDIR = /usr/local/lib
        }
        else { 
            exists( /usr/include/opencv/cv.hpp )
             { 
                message("OpenCV found in /usr/include.")
                CVINSTPATH = /usr
                CVINCPATH = /usr/include/opencv
                INCLUDEPATH += /usr/include/opencv
                DYN_LIBS += -L/usr/lib
                OPENCV_STATIC_LIBDIR = /usr/local/lib
            }
            else { 
                exists( /sw/include/opencv/cv.hpp )
                 { 
                    message("OpenCV found in /usr/include.")
                    CVINSTPATH = /usr
                    CVINCPATH = /usr/include/opencv
                    INCLUDEPATH += /usr/include/opencv
                    DYN_LIBS += -L/sw/lib
                    OPENCV_STATIC_LIBDIR = /sw/lib
                }
                else:message ( "OpenCV NOT FOUND => IT WILL NOT COMPILE" )
            }
        }
    }
    
    # on MacOS X with OpenCV 1, we must also link with cxcore
    message( Dynamic libraries extension : '$$LIBS_EXT' )
    CXCORE_LIB = $$CVINSTPATH/lib/libcxcore.$$LIBS_EXT
    message ( Testing CxCore lib = '$$CXCORE_LIB' )
    exists( $$CXCORE_LIB ) { 
        message( " => Linking with CxCore in '$$CVINSTPATH' ")
        DYN_LIBS += -lcxcore
        STATIC_LIBS += $$OPENCV_STATIC_LIBDIR/lib_cxcore.a
    }
    
    # For Ubuntu 7.10 for example, the link option is -lcv instead of -lopencv
    CV_LIB = $$CVINSTPATH/lib/libcv.$$LIBS_EXT
    message ( Testing CV lib = '$$CV_LIB' )
    exists( $$CV_LIB ) { 
        message( " => Linking with -lcv ('$$CV_LIB' exists)")
        DYN_LIBS += -lcv
        STATIC_LIBS += $$OPENCV_STATIC_LIBDIR/lib_cxcore.a
    }
    else { 
        message( " => Linking with -lopencv ('$$CV_LIB' does not exist)")
        DYN_LIBS += -lopencv
        STATIC_LIBS += $$OPENCV_STATIC_LIBDIR/libopencv.a
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
DYN_LIBS += -lcvaux \
    -lhighgui
STATIC_LIBS += $$OPENCV_STATIC_LIBDIR/lib_cv.a \
    $$OPENCV_STATIC_LIBDIR/lib_cvaux.a \
    $$OPENCV_STATIC_LIBDIR/lib_highgui.a
BUILD_STATIC = $$(BUILD_STATIC)
contains(BUILD_STATIC, true) { 
    message("Building static version of binary :")
    
    # Test for building releases
    LIBS += $$STATIC_LIBS
}
else { 
    message("Building dynamic libraries version of binary :")
    
    # Dynamic libraries version
    LIBS += $$DYN_LIBS
}
OTHER_FILES += build_mac_bundle.sh \
    build_mac_dmg.py

# CONFIG(debug, debug|release) {
macx:message("MacOS X specific options =================================================")

# TARGET = $$join(TARGET,,,_debug)
# DEFINES += "TRANSLATION_DIR=\"Tamanoir.app/Contents/\""
linux-g++ { 
    message("Linux specific options =================================================")
    DEFINES += "TRANSLATION_DIR=/usr/share/tamanoir"
}
win32:TARGET = $$join(TARGET,,d)

# }
CONFIG += qt \
    warn_on \
    debug \
    release \
    build_all

# # INSTALATION
# target.path = /usr/local/tamanoir
# INSTALLS += target
# FINAL CONFIGURATION ==================================================
message( "")
message( "")
message( "FINAL CONFIGURATION ================================================== ")
message( "Configuration : ")
message( " defs : $$DEFINES ")
message( " libs : $$LIBS ")
message( "FINAL CONFIGURATION ================================================== ")
message( "")
message( "")
RESOURCES += tamanoir.qrc
