#!/bin/bash
echo
echo "GENERATE Translation packages ...................."

echo
echo "BUILDING Tamanoir for MacOS X : please build first with QtCreator....................."
echo
#rm -fr Tamanoir.app
#BUILD_STATIC=true /Developer/Tools/Qt/qmake Tamanoir.pro || exit 0
#make clean && make || exit 0

cp tamanoir_fr.qm Tamanoir.app/Contents/MacOS/tamanoir_French.qm

#./build_mac_bundle.sh Tamanoir-simple.app/ Tamanoir-simple.app/Contents/MacOS/Tamanoir-simple
echo
echo "PACKAGING........................................."
./build_mac_bundle.sh Tamanoir.app/ Tamanoir.app/Contents/MacOS/Tamanoir


#DEBUG ./build_mac_bundle.sh Tamanoir-simple.app/ Tamanoir-simple.app/Contents/MacOS/Tamanoir-simple
#./build_mac_bundle.sh Tamanoir.app/ Tamanoir.app/Contents/MacOS/Tamanoir

echo
echo "COMPRESSING......................................."
zip -r Tamanoir-MacOSX-svn`date +%Y%m%d`.zip Tamanoir.app
echo
echo "Done"

#exit 0


echo
echo "Listing library dependencies"

APP=Tamanoir

# First make .app directory either with Xcode or command line qmake
# /opt/local/bin/qmake-mac && make



#otool -L Tamanoir.app/Contents/MacOS/Tamanoir
 
# We must get at least QtGui, OpenCV and Tiff libraries 

 
mkdir $APP.app/Contents/Frameworks


# Copy QtCore, QtGui
qtcore=`otool -L $APP.app/Contents/MacOS/$APP | grep QtCore | awk '{print $1}'`

qtcoredir=`otool -L $APP.app/Contents/MacOS/$APP | grep QtCore | awk '{print $1}' | cut -d "/" -f1 `

firstslash=`otool -L $APP.app/Contents/MacOS/$APP | grep QtCore | awk '{print $1}' | cut -c 1`

echo "First char: '$firstslash'"

if [ "$firstslash" = "/" ]; then 
    echo "In custom directory: '$qtcore' : NOT SUPPORTED. Abort"
    qtfrmwk=
    exit 0
else
    echo "In standard directory"
    qtfrmwk=/Library/Frameworks
fi

echo "Qt Framework is '$qtfrmwk'"
echo
echo "QtCore Path = '$qtcore' => copy in Contents"
cp -R $qtfrmwk/$qtcoredir $APP.app/Contents/Frameworks

qtgui=`otool -L $APP.app/Contents/MacOS/$APP | grep QtGui | awk '{print $1}'`
qtguidir=`otool -L $APP.app/Contents/MacOS/$APP | grep QtGui | awk '{print $1}' | cut -d "/" -f1`
echo "QtGui Path = '$qtgui' => copy in Contents"
cp -R $qtfrmwk/$qtguidir $APP.app/Contents/Frameworks

# Copy OpenCV
cvpath=`otool -L $APP.app/Contents/MacOS/$APP | grep cxcore | awk '{print $1}'`
cvpath=`pwd -L $cvpath`
echo "CVPath='$cvpath'"
cxcore=`otool -L $APP.app/Contents/MacOS/$APP | grep cxcore | awk '{print $1}'`
cxcoreslashh=`echo $cxcore | cut -d "/" -f5` 
cv=`otool -L $APP.app/Contents/MacOS/$APP | grep "cv\." | awk '{print $1}'`
cvslash=`otool -L $APP.app/Contents/MacOS/$APP | grep "cv\." | awk '{print $1}' | cut -d "/" -f5`

cvaux=`otool -L $APP.app/Contents/MacOS/$APP | grep "cvaux" | awk '{print $1}'`
highgui=`otool -L $APP.app/Contents/MacOS/$APP | grep highgui | awk '{print $1}'`

echo "OpenCV Libs: "
echo "    + CxCore='$cxcore' => '$cxcoreslash'"
echo "    + Cv='$cv' => '$cvslash'"
echo "    + Cvaux='$cvaux'"
echo "    + HighGUI='$highgui'"
echo

cp -R /usr/local/lib/libcxcore* $APP.app/Contents/FrameWorks/
cp -R /usr/local/lib/libcv* $APP.app/Contents/Frameworks
cp -R /usr/local/lib/libhighgui* $APP.app/Contents/Frameworks

# Copy libtiff
tiff=`otool -L $APP.app/Contents/MacOS/$APP | grep tiff | awk '{print $1}'`
echo "TIFF Libs: '$tiff'"
cp -R $tiff $APP.app/Contents/Frameworks


echo 
echo "Finally : list of the contents of directory $APP.app/Contents/Frameworks/"
ls $APP.app/Contents/Frameworks

echo
echo "Remapping Frameworks... -id"
echo " + remapping libcv..."
echo install_name_tool -id @executable_path/../Frameworks/$cvslash $APP.app//Contents/Frameworks/$cvslash
install_name_tool -id @executable_path/../Frameworks/$cvslash $APP.app//Contents/Frameworks/$cvslash
echo " + change..."
echo install_name_tool -change /usr/local/lib \
         @executable_path/../Frameworks \
         $APP.app/Contents/Frameworks/libcxcore.2.dylib
echo install_name_tool -change /usr/local/lib \
         @executable_path/../Frameworks \
         $APP.app/Contents/Frameworks/libcv.2.dylib
echo install_name_tool -change /usr/local/lib \
         @executable_path/../Frameworks \
         $APP.app/Contents/Frameworks/libcvaux.2.dylib
echo install_name_tool -change /usr/local/lib \
         @executable_path/../Frameworks \
         $APP.app/Contents/Frameworks/libhighgui.2.dylib


echo "Finally, otool says : "
otool -L $APP.app/Contents/MacOS/$APP


#install_name_tool -id @executable_path/../Frameworks/$cv \
#		$APP.app/Contents/Frameworks/$cvslash
exit 0
install_name_tool -id @executable_path/../Frameworks/$qtcore \
                $APP.app/Contents/Frameworks/$qtcore
install_name_tool -id @executable_path/../Frameworks/$qtgui \
                $APP.app/Contents/Frameworks/$qtgui

echo "Remapping Frameworks... -change"
install_name_tool -change \
                $qtfrmwk/$qtcore \
                @executable_path/../Frameworks/$qtcore \
                $APP.app/Contents/MacOS/$APP
install_name_tool -change \
                $qtfrmwk/$qtgui \
                @executable_path/../Frameworks/$qtgui \
                $APP.app/Contents/MacOS/$APP

install_name_tool -change $qtfrmwk/$qtcore \
                 @executable_path/../Frameworks/$qtcore \
                 $APP.app/Contents/Frameworks/$qtgui

echo "Finally, otool says : "
otool -L $APP.app/Contents/MacOS/$APP

