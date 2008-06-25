######################################################################
# TAMANOIR Qt PROJECT
######################################################################

TEMPLATE = app
TARGET = Tamanoir 
DEPENDPATH += . inc src ui
INCLUDEPATH += . inc ui

DEFINES += QT3_SUPPORT

# Input
HEADERS += inc/imgproc.h \
		inc/imgutils.h \
		inc/tamanoir.h \
		inc/qimagedisplay.h

FORMS = ui/tamanoir.ui

SOURCES += src/imgproc.cpp \
		src/imgutils.cpp \
		src/main.cpp \
		src/tamanoir.cpp \
		src/qimagedisplay.cpp

linux-g++:TMAKE_CXXFLAGS += -Wall -O2 -g \
	-fexceptions -Wimplicit -Wreturn-type \
	-Wunused -Wswitch -Wcomment -Wuninitialized -Wparentheses  \
	-Wpointer-arith 

linux-g++:DEFINES += LINUX


LIBS_EXT = dylib
linux-g++:LIBS_EXT = so

message( "Installation directory  = $(PWD) ")

LIBS =

unix: {
	# Test if libtiff is installed
	exists( /opt/local/include/tiffio.h ) {
		INCLUDEPATH += /opt/local/include
		LIBS += -L/opt/local/lib
		SOURCES += src/cv2tiff.cpp
		DEFINES += HAVE_LIBTIFF
	} else {
		exists( /usr/local/include/tiffio.h ) {
			INCLUDEPATH += /usr/local/include
			LIBS += -L/usr/local/lib
			SOURCES += src/cv2tiff.cpp
			DEFINES += HAVE_LIBTIFF
		}
	}
	
	# Test if OpenCV library is present
	exists( /opt/local/include/opencv/cv.hpp) {
		message("OpenCV found in /opt/local/include/opencv.")
		INCLUDEPATH += /opt/local/include/opencv
		
		CVINSTPATH = /opt/local
		CVINCPATH = /opt/local/include/opencv
		
		LIBS += -L/opt/local/lib 
	} else {
		exists( /usr/local/include/opencv/cv.hpp ) {
			message("OpenCV found in /usr/local/include.")
			INCLUDEPATH += /usr/local/include/opencv
			
			CVINSTPATH = /usr/local
			CVINCPATH = /usr/local/include/opencv
			
			LIBS += -L/usr/local/lib 
			
		} else {
			exists( /usr/include/opencv/cv.hpp ) 
			{
				message("OpenCV found in /usr/include.")
				CVINSTPATH = /usr
				CVINCPATH = /usr/include/opencv
				INCLUDEPATH += /usr/include/opencv
				
				LIBS += -L/usr/lib
			} else {
				message ( "OpenCV NOT FOUND => IT WILL NOT COMPILE" )
				
				
			}		
		}
	}

	# on MacOS X with OpenCV 1, we must also link with cxcore
	message( Dynamic libraries extension : '$$LIBS_EXT' ) 
	CXCORE_LIB = $$CVINSTPATH/lib/libcxcore.$$LIBS_EXT
	message ( Testing CxCore lib = '$$CXCORE_LIB' )
	exists( $$CXCORE_LIB ) {
		message( " => Linking with CxCore in '$$CVINSTPATH' ")
		LIBS += -lcxcore
	}
	
	# For Ubuntu 7.10 for example, the link option is -lcv instead of -lopencv
	CV_LIB = $$CVINSTPATH/lib/libcv.$$LIBS_EXT
	message ( Testing CV lib = '$$CV_LIB' )
	exists( $$CV_LIB ) {
		message( " => Linking with -lcv ('$$CV_LIB' exists)")
		LIBS += -lcv
	} else {
		message( " => Linking with -lopencv ('$$CV_LIB' does not exist)")
		LIBS += -lopencv
	}

	INSTALL_DUSTHOOVER = $(DUSTHOOVER_DIR)
	message( Reading installation directory : '$$INSTALL_DUSTHOOVER')
	
	
	count( $$INSTALL_DUSTHOOVER , 0 ) {
		message("Installation directory is undefined !! Installing in '/usr/local/tamanoir'.")
	}
	else {
		message( Installing in : $$INSTALL_DUSTHOOVER)
		DEFINES += BASE_DIRECTORY=$$INSTALL_DUSTHOOVER
		message( defines : $$DEFINES )
	}
}

LIBS += -lcvaux -lhighgui -ltiff

INCLUDEPATH	+= .
