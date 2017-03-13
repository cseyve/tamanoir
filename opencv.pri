#this script try to locate opencv and set the right compilation options

# PG = opencv
# system(pkg-config --exists $$PG) {
#    message(opencv $$system(pkg-config --modversion $$PG)found)
#    LIBS += $$system(pkg-config --libs $$PG)
#    INCLUDEPATH += $$system(pkg-config --cflags $$PG | sed s/-I//g)
# } else {
#   error($$PG "NOT FOUND => IT WILL NOT COMPILE")
#}

LIBSDIR =
LIBS_EXT = dylib

PKG=opencv
system('pkg-config --exists $$PKG') {
	PKGVERSION =  $$system(pkg-config $$PKG --modversion)
	LIBS += $$system(pkg-config --libs $$PKG)

	CVINC = $$system('pkg-config --cflags $$PKG | sed "s/\\-I//g"')
	INCLUDEPATH += $$CVINC
	message(OpenCV $$PKGVERSION found with pkg-config: CVINC=$$CVINC => INCLUDEPATH=$$INCLUDEPATH)
	# Try to activate the OPENVCV_22 tag 
	system('pkg-config --exists $$PKG "opencv >= 2.2.0"') {
		message("    OpenCV >= 2.2, activate exceptions")
		DEFINES += OPENCV_22 OPENCV2
	}
	system('pkg-config --exists $$PKG "opencv >= 2.4.0"') {
		message("    OpenCV >= 2.4, activate photo")
		DEFINES += OPENCV_24_PHOTO
	}
} else {

	BASEPATH=/usr
	linux-arm* {
		message("Opencv.pri : compilation for ARM")
		BASEPATH=/opt/eldk-5.3/armv7a-hf/sysroots/armv7ahf-vfp-neon-linux-gnueabi/usr
	}

	linux* {
		 LIBS_EXT = so
	}

	unix: {
		QMAKE_LIBDIR_QT -= /usr/lib

		##################### OPENCV >= 2.2 #####################
		# Test if OpenCV library is present
		exists( $$BASEPATH/local/include/opencv2/core/core.hpp ) {
			message("OpenCV >= 2.2 found in $$BASEPATH/local/include/opencv2/")
			DEFINES += OPENCV_22 OPENCV2
			INCLUDEPATH += $$BASEPATH/local/include/opencv2
			# add one level higher because opencv.hpp includes subdirs like "opencv2/core/core.hpp"
			INCLUDEPATH += $$BASEPATH/local/include

			CVINSTPATH = $$BASEPATH/local
			CVINCPATH = $$BASEPATH/local/include/opencv2

			LIBS += -L$$BASEPATH/local/lib
			LIBSDIR = $$BASEPATH/local/lib
		} else {
			exists( $$BASEPATH/include/opencv2/core/core.hpp ) {
						message("OpenCV >= 2.2 found in $$BASEPATH/include/opencv2/")

				DEFINES += OPENCV_22 OPENCV2

				#message("OpenCV found in $$BASEPATH/include.")
				CVINSTPATH = $$BASEPATH
				CVINCPATH = $$BASEPATH/include/opencv2
				INCLUDEPATH += $$BASEPATH/include/opencv2
						# add one level higher because opencv.hpp includes subdirs like "opencv2/core/core.hpp"
				INCLUDEPATH += $$BASEPATH/include

				LIBS += -L$$BASEPATH/lib
				LIBSDIR = $$BASEPATH/lib
			} else {
						# for MacPorts
						exists( /opt/local/include/opencv2/core/core.hpp ) {
								message("OpenCV >= 2.2 found in /opt/local/include/opencv2/")

								DEFINES += OPENCV_22 OPENCV2

								#message("OpenCV found in $$BASEPATH/include.")
								CVINSTPATH = /opt/local/
								CVINCPATH = /opt/local/include/opencv2
								INCLUDEPATH += /opt/local/include/opencv2
								INCLUDEPATH += /opt/local/include/

								LIBS += -L/opt/local/lib
								LIBSDIR = /opt/local/lib
						} else {
							##################### OPENCV <= 2.1 #####################
				# Test if OpenCV library is present
				exists( $$BASEPATH/local/include/opencv/cv.hpp ) {
					DEFINES += OPENCV_21 OPENCV2
					#message("OpenCV found in $$BASEPATH/local/include.")
					INCLUDEPATH += $$BASEPATH/local/include/opencv

					CVINSTPATH = $$BASEPATH/local
					CVINCPATH = $$BASEPATH/local/include/opencv

					LIBS += -L$$BASEPATH/local/lib
					LIBSDIR = $$BASEPATH/local/lib
				} else {
									exists( $$BASEPATH/include/opencv/cv.hpp ) {
						DEFINES += OPENCV_21 OPENCV2
						#message("OpenCV found in $$BASEPATH/include.")
						CVINSTPATH = $$BASEPATH
						CVINCPATH = $$BASEPATH/include/opencv
						INCLUDEPATH += $$BASEPATH/include/opencv

						LIBS += -L$$BASEPATH/lib
						LIBSDIR = $$BASEPATH/lib
					} else {
						message ( "OpenCV NOT FOUND => IT WILL NOT COMPILE" )
					}
				}
			}
		}

	}


		CV22_LIB = $$LIBSDIR/libopencv_core.$$LIBS_EXT
		message ( Testing CV lib = $$CV22_LIB )
		exists( $$CV22_LIB ) {
			message( " => Linking with -lopencv_* ('$$CV_LIB' exists)")
			LIBS += -lopencv_core -lopencv_imgproc -lopencv_legacy -lopencv_highgui
			# for Haar (needed for pluigns)
			LIBS += -lopencv_features2d -lopencv_video -lopencv_objdetect
			DEFINES += HAS_HIGHGUI
		} else {
			# For Ubuntu 7.10 for example, the link option is -lcv instead of -lopencv
			CV_LIB = $$LIBSDIR/libcv.$$LIBS_EXT
			#message ( Testing CV lib = '$$CV_LIB' )
			exists( $$CV_LIB ) {
				#message( " => Linking with -lcv ('$$CV_LIB' exists)")
				LIBS += -lcxcore -lcv -lcvaux -lhighgui
				DEFINES += HAS_HIGHGUI
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
					DEFINES += HAS_HIGHGUI
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

} # else of pkg-config
