#!/bin/bash

echo
echo "BUILDING Tamanoir for MacOS X....................."
echo


./build_mac_bundle.sh Tamanoir-simple.app/ Tamanoir-simple.app/Contents/MacOS/Tamanoir-simple
./build_mac_bundle.sh Tamanoir.app/ Tamanoir.app/Contents/MacOS/Tamanoir

exit 0


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
cxcore=`otool -L $APP.app/Contents/MacOS/$APP | grep cxcore | awk '{print $1}'`
cv=`otool -L $APP.app/Contents/MacOS/$APP | grep cv. | awk '{print $1}'`
highgui=`otool -L $APP.app/Contents/MacOS/$APP | grep highgui | awk '{print $1}'`

echo "OpenCV Libs: "
echo "    + CxCore='$cxcore'"
echo "    + Cv='$cv'"
echo "    + HighGUI='$highgui'"
echo

cp -R $cxcore $APP.app/Contents/Frameworks
cp -R $cv $APP.app/Contents/Frameworks
cp -R $highgui $APP.app/Contents/Frameworks

# Copy libtiff
tiff=`otool -L $APP.app/Contents/MacOS/$APP | grep tiff | awk '{print $1}'`
echo "TIFF Libs: '$tiff'"
cp -R $tiff $APP.app/Contents/Frameworks


echo 
echo "Finally : list of the contents of directory $APP.app/Contents/Frameworks/"
ls $APP.app/Contents/Frameworks

echo
echo "Remapping Frameworks... -id"
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

otool -L $APP.app/Contents/MacOS/$APP

