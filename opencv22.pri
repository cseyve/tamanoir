# TEST INSTALL DIRECTORY FOR SEARCH LOCAL FILES

#message("Testing dir = $$CV22_DIR For OpenCV >= 2.2")

exists( $$CV22_DIR/include/opencv2/opencv.hpp ) {
	message("OpenCV >= 2.2 found in search dir= $$CV22_DIR/include/opencv2")
	DEFINES += OPENCV_22 OPENCV2
	INCLUDEPATH += $$CV22_DIR/include/opencv2


	LIBS += -L$$CV22_DIR/lib
	LIBSDIR = $$CV22_DIR/lib

	CVINSTPATH = $$CV22_DIR
	CVINCPATH = $$CV22_DIR/include/opencv2

	# Dynamic libraries for OpenCV >= 2.2
	CV22_LIB = $$LIBSDIR/libopencv_core.$$LIBS_EXT
	message ( Testing CV lib = '$$CV22_LIB )
	exists( $$CV22_LIB ) {
		#message( " => Linking with -lcv ('$$CV_LIB' exists)")
		LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui
	}

	OPENCV_STATIC_LIBDIR = $$CV22_DIR/lib


	# FIXME : use the right .a files
	STATIC_LIBS += $$OPENCV_STATIC_LIBDIR/lib_cxcore.a


}

#message("Testing dir = $$CV22_DIR For OpenCV <= 2.1")
exists($$CV22_DIR/include/opencv/cv.hpp) {
	message("OpenCV <= 2.1 found in $$CV22_DIR")
	DEFINES += OPENCV_21 OPENCV2
	INCLUDEPATH += $$CV22_DIR/include/opencv
	CVINCPATH = $$CV22_DIR/include/opencv
	CVINSTPATH = $$CV22_DIR

	DIN_LIBS += -L$$CV22_DIR/lib
	LIBSDIR = $$CV22_DIR/lib

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

	# libtool --mode=link g++ -o Tamanoir .obj-simple/imgproc.o .obj-simple/imgutils.o .obj-simple/main.o .obj-simple/tamanoir.o .obj-simple/qimagedisplay.o .obj-simple/cv2tiff.o .obj-simple/moc_tamanoir.o .obj-simple/moc_qimagedisplay.o .obj-simple/qrc_tamanoir.o -L/usr/lib /usr/lib/libtiff.a /usr/local/lib/libcxcore.la /usr/local/lib/libcv.la /usr/local/lib/libcvaux.la /usr/local/lib/libhighgui.la -lQtGui -lQtCore -lpthread
	message("Adding openCV <=2.1 static libraries for release")
	DYN_LIBS += -lcvaux -lhighgui
	STATIC_LIBS += $$OPENCV_STATIC_LIBDIR/lib_cv.a \
		$$OPENCV_STATIC_LIBDIR/lib_cvaux.a \
		$$OPENCV_STATIC_LIBDIR/libtiff.a \
		$$OPENCV_STATIC_LIBDIR/lib_highgui.a
}


message("OpenCV tested for path $$CV22_DIR")
