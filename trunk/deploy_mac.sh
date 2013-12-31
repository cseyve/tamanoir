#!/bin/bash
rm -fr Tamanoir.app
/Developer/Tools/Qt/qmake Tamanoir.pro -r -spec macx-g++ CONFIG+=release
make clean && make || exit
/Developer/Tools/Qt/lrelease Tamanoir_French.ts
SVNVERSION=$(svnversion -n . | sed s#\:#-#g)
echo "Generating version = $SVNVERSION"
sed s#VERSION#"$SVNVERSION"#g Info.plist > Tamanoir.app/Contents/Info.plist

echo "Generate .app"
cp Tamanoir_French.qm Agora.app/Contents/MacOS/
cp /Developer/Applications/Qt/translations/qt_fr.qm Tamanoir.app/Contents/MacOS/
cp -r doc Tamanoir.app/Contents/MacOS/

/Developer/Tools/Qt/macdeployqt Tamanoir.app -dmg
mv Tamanoir.dmg Tamanoir-MacOSX-build"$SVNVERSION".dmg

#/Developer/Tools/Qt/macdeployqt Tamanoir.app
#zip -r Tamanoir-MacOSX-build"$SVNVERSION".zip Tamanoir.app
echo
echo "done."


