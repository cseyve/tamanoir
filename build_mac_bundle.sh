#!/bin/sh
# Configuration
# Reference : http://www.siteduzero.com/forum-83-234070-p1-un-bundle-mac-pour-vos-applications-qt4.html
LIBRARY="QtCore QtGui "

# On verifie et on donne le mode d'emploi
if [ $# -ne 2 ]
   then
   echo "USAGE : $0 /chemin/du/bundle /chemin/executable"
   exit 1
fi

# Ne pas oublier le / final
BUNDLE_FRAMEWORK_PWD="$1/Contents/Frameworks/"
SYSTEM_FRAMEWORK_PWD="/Library/Frameworks/"

echo "rm -fr $BUNDLE_FRAMEWORK_PWD"
rm -fr $BUNDLE_FRAMEWORK_PWD

echo "mkdir $BUNDLE_FRAMEWORK_PWD"
mkdir -p $BUNDLE_FRAMEWORK_PWD

for lib in $LIBRARY
do
   # Copie du Framework dans le bundle
   echo "cp -r ${SYSTEM_FRAMEWORK_PWD}${lib}.framework $BUNDLE_FRAMEWORK_PWD"
   cp -R ${SYSTEM_FRAMEWORK_PWD}${lib}.framework $BUNDLE_FRAMEWORK_PWD

   # Change le chemin absolu de chaque librairie par une chemin relatif au Bundle.
   # Chaque librairie sait où elle est.
   echo "install_name_tool -id @executable_path/../Frameworks/${lib}.framework/Versions/4.0/${lib} ${BUNDLE_FRAMEWORK_PWD}/${lib}.framework/Versions/4.0/${lib}"
   install_name_tool -id @executable_path/../Frameworks/${lib}.framework/Versions/4.0/${lib} ${BUNDLE_FRAMEWORK_PWD}/${lib}.framework/Versions/4.0/${lib}

   # Change le chemin absolu de chaque librairie dans l'executable
    # L'executable sais où sont ses librairies
   echo "install_name_tool -change ${lib}.framework/Versions/4/${lib} @executable_path/../Frameworks/${lib}.framework/Versions/4.0/${lib} $2"
   install_name_tool -change ${lib}.framework/Versions/4/${lib} @executable_path/../Frameworks/${lib}.framework/Versions/4.0/${lib} $2

done

# On change le chemin de chaque librairie dans chaque autre succeptible de l'utiliser
for lib_destination in $LIBRARY
do
    for lib_source in $LIBRARY
    do
        echo "install_name_tool -change ${lib_source}.framework/Versions/4/${lib_source} @executable_path/../Frameworks/${lib_source}.framework/Versions/4.0/${lib_source} ${BUNDLE_FRAMEWORK_PWD}${lib_destination}.framework/Versions/4.0/${lib_destination}"
        install_name_tool -change ${lib_source}.framework/Versions/4/${lib_source} @executable_path/../Frameworks/${lib_source}.framework/Versions/4.0/${lib_source} \
        ${BUNDLE_FRAMEWORK_PWD}${lib_destination}.framework/Versions/4.0/${lib_destination}
    done
done
