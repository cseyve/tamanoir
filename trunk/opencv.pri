#this script try to locate opencv and set the right compilation options

# PG = opencv
# system(pkg-config --exists $$PG) {
#    message(opencv v$$system(pkg-config --modversion $$PG)found)
#    LIBS += $$system(pkg-config --libs $$PG)
#    INCLUDEPATH += $$system(pkg-config --cflags $$PG | sed s/-I//g)
# } else {
#   error($$PG "NOT FOUND => IT WILL NOT COMPILE")
#}

LIBSDIR =
LIBS_EXT = dylib


linux-g++: LIBS_EXT = so
unix: {
	##################### OPENCV >= 2.2 #####################
	# Test if OpenCV library is present
	exists( /usr/local/include/opencv2/core/core.hpp ) {
		message("OpenCV >= 2.2 found in /usr/local/include/opencv2/")
		DEFINES += OPENCV_22 OPENCV2
		INCLUDEPATH += /usr/local/include/opencv2

		CVINSTPATH = /usr/local
		CVINCPATH = /usr/local/include/opencv2

		LIBS += -L/usr/local/lib
		LIBSDIR = /usr/local/lib
	} else {
		exists( /usr/include/opencv2/core/core.hpp ) {
	                message("OpenCV >= 2.2 found in /usr/include/opencv2/")

			DEFINES += OPENCV_22 OPENCV2

			#message("OpenCV found in /usr/include.")
			CVINSTPATH = /usr
			CVINCPATH = /usr/include/opencv2
			INCLUDEPATH += /usr/include/opencv2

			LIBS += -L/usr/lib
			LIBSDIR = /usr/lib
		} else {
			##################### OPENCV <= 2.1 #####################
			# Test if OpenCV library is present
			exists( /usr/local/include/opencv/cv.hpp ) {
				DEFINES += OPENCV_21 OPENCV2
				#message("OpenCV found in /usr/local/include.")
				INCLUDEPATH += /usr/local/include/opencv

				CVINSTPATH = /usr/local
				CVINCPATH = /usr/local/include/opencv

				LIBS += -L/usr/local/lib
				LIBSDIR = /usr/local/lib
			} else {
				exists( /usr/include/opencv/cv.hpp )
				{
					DEFINES += OPENCV_21 OPENCV2
					#message("OpenCV found in /usr/include.")
					CVINSTPATH = /usr
					CVINCPATH = /usr/include/opencv
					INCLUDEPATH += /usr/include/opencv

					LIBS += -L/usr/lib
					LIBSDIR = /usr/lib
				} else {
					message ( "OpenCV NOT FOUND => IT WILL NOT COMPILE" )
				}
			}
		}
	}




	CV22_LIB = $$LIBSDIR/libopencv_core.$$LIBS_EXT
	message ( Testing CV lib = '$$CV22_LIB )
	exists( $$CV22_LIB ) {
		#message( " => Linking with -lcv ('$$CV_LIB' exists)")
		LIBS += -lopencv_core -lopencv_imgproc -lopencv_legacy -lopencv_highgui 
		# for Haar (needed for pluigns)
		LIBS += -lopencv_features2d -lopencv_video -lopencv_objdetect
	} else {
		# For Ubuntu 7.10 for example, the link option is -lcv instead of -lopencv
		CV_LIB = $$LIBSDIR/libcv.$$LIBS_EXT
		#message ( Testing CV lib = '$$CV_LIB' )
		exists( $$CV_LIB ) {
			#message( " => Linking with -lcv ('$$CV_LIB' exists)")
			LIBS += -lcxcore -lcv -lcvaux -lhighgui
		} else {
			# on MacOS X with OpenCV 1, we must also link with cxcore
			#message( Dynamic libraries : '$$LIBS_EXT' )
			CXCORE_LIB = $$CVINSTPATH/lib/libcxcore.$$LIBS_EXT
			#message ( Testing CxCore lib = '$$CXCORE_LIB' )
			exists( $$CXCORE_LIB ) {
				# message( " => Linking with CxCore in '$$CVINSTPATH' ")
				LIBS += -lcxcore
			}


			CV_LIB = $$LIBSDIR/libopencv.$$LIBS_EXT
			exists($$CV_LIB ) {

				#message( " => Linking with -lopencv ('$$CV_LIB' does not exist)")
				LIBS += -lopencv -lcvaux -lhighgui
			}
		}
	}

	# Subdirs for OpenCV 2.2
	COREHPP = $$CVINCPATH/core/core.hpp
	message ("Testing $$COREHPP")
	exists( $$COREHPP ) {
		message("OpenCV >= 2.2 found in $$COREHPP")
		INCLUDEPATH += $$CVINCPATH/core
		INCLUDEPATH += $$CVINCPATH/imgproc
		INCLUDEPATH += $$CVINCPATH/legacy
		INCLUDEPATH += $$CVINCPATH/calib3d
		INCLUDEPATH += $$CVINCPATH/contrib
		INCLUDEPATH += $$CVINCPATH/ml
		INCLUDEPATH += $$CVINCPATH/highgui
	}

        exists($$CVINCPATH/nonfree/features2d.hpp) {
                message("Using non free in recent OpenCV functions")
                INCLUDEPATH += $$CVINCPATH/nonfree
                DEFINES += OPENCV_NONFREE
                LIBS += -lopencv_nonfree
        }
	exists($$CVINCPATH/photo/photo.hpp) {
                message("Using photo in recent OpenCV functions")
                INCLUDEPATH += $$CVINCPATH/photo
                DEFINES += OPENCV_PHOTO
                LIBS += -lopencv_photo
        }


}

win32: {
	message("Win32 specific paths : OpenCV must be installed in C:\\Program Files\\OpenCV")
	DYN_LIBS += -L"C:\Program Files\OpenCV\lib" \
		-L"C:\Program Files\OpenCV\bin"

	exists("C:\Program Files\OpenCV\cxcore\include\cxcore.h") {
		message("Complete for opencv >=2.1")
		DEFINES += OPENCV2 OPENCV22
		INCLUDEPATH += "C:\Program Files\OpenCV\cxcore\include"
		INCLUDEPATH += "C:\Program Files\OpenCV\cv\include"
		INCLUDEPATH += "C:\Program Files\OpenCV\cvaux\include"
		INCLUDEPATH += "C:\Program Files\OpenCV\otherlibs\highgui"
		INCLUDEPATH += "C:\Program Files\OpenCV\otherlibs\highgui"
		exists("C:\Program Files\OpenCV\bin\cvaux110.dll") {
			DYN_LIBS += -lcxcore -lcv -lcvaux -lhighgui
		} else {

			DYN_LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui
		}

	} else {
		message("Complete for opencv <2.1")
		DEFINES += OPENCV2 OPENCV21
		INCLUDEPATH += "C:\Program Files\OpenCV\include"

	}
        LIBS += $$DYN_LIBS
}


