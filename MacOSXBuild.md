# Introduction #

This page explains how to build Tamanoir on Mac OS X.


# Required #

Building Tamanoir on Mac OS X.

You need :
  * OpenCV : to load/save images, to manipulate some image buffers (libs cxcore, highgui)
  * libTIFF for 16bit TIFF support
  * Qt4 for Mac
  * XCode or QtCreator for development GUI
  * Tamanoir


---

## OpenCV + libtiff ##

Install a development version of OpenCV with Fink or MacPorts
```
sudo port install opencv libtiff-dev
```
or
```
sudo fink install opencv libtiff-dev
```

Project page: http://www.intel.com/technology/computing/opencv/


---

## Qt4 ##

Download : http://www.qtsoftware.com/downloads Choose GPL/LGPL version.
Install Qt4 Framework or SDK (for QtCreator).


---

## Tamanoir ##

Download snapshot :
```
# Non-members may check out a read-only working copy anonymously over HTTP.
svn checkout http://tamanoir.googlecode.com/svn/trunk/ tamanoir-read-only
```

2 methods : with command lines within a terminal or with an IDE (XCode or QtCreator)
  * Command line : compile Tamanoir : within a terminal :
```
cd tamanoir/
/Developer/Tools/Qt/qmake Tamanoir.pro 
```

  * XCode : launch the command line above to create a xcode project
> > Then you can choose your IDE :
    * open the _Tamanoir.xcodeproj_ with XCode then build, run...


  * QtCreator use _Tamanoir.pro_ to create project, and build with the build command.


After building, you can copy the Tamanoir (_Tamanoir.app_) in your "Application" directory in Finder.


---


## Releasing MacOS X package ##

This is still poorly done :
  * Build with QtCreator to link with Universal binary libraries
  * launch the _build\_macosx\_pkg.sh_ to create bundle and an archive (ZIP) of the application. It still has QtGui\_debug inside, so the archive is huge (140 MB !)


---


---


# Additional notes and build information for a few components used by Tamanoir #

This section is only for developers's information, it's not anymore needed for building Tamanoir or enabling some feature.

All the information for building is above.


---


## MacOS Notes ##

Using the "Open with" of Finder, or of any other "external applications" (Adobe Lightroom...).

We have to overload the QApplication to handle MacOS X Carbon events.
Example : http://www.mail-archive.com/lyx-devel@lists.lyx.org/msg94792.html



---

## Libtiff ##

Download : http://www.libtiff.org/
Compile and Install :
```
./configure  CFLAGS="-O -arch i386 -arch ppc" CXXFLAGS="-O -arch i386 -arch ppc"  LDFLAGS="-arch i386 -arch ppc" --disable-dependency-tracking 
ake ; sudo make install
```


---

## libpng ##

Download : http://libpng.sourceforge.net/ - [ftp://ftp.simplesystems.org/pub/png/](ftp://ftp.simplesystems.org/pub/png/)
Compile and Install :
```
./configure  CFLAGS="-O -arch i386 -arch ppc" CXXFLAGS="-O -arch i386 -arch ppc"  LDFLAGS="-arch i386 -arch ppc" --disable-dependency-tracking
make ; sudo make install
```


---

## OpenCV ##

### OpenCV **with** 16bit TIFF support ###
Download patched version for 16bit support : http://cseyve.free.fr/Vision/download/opencv-tiff16bit.tar.bz2

We must compile without ImageIO to use the libtiff for 16bit support :
```
./configure CFLAGS="-O -g -arch i386 -arch ppc" CXXFLAGS="-O -arch i386 -arch ppc"  LDFLAGS="-arch i386 -arch ppc" --disable-dependency-tracking --with-libtiff=yes --enable-libtiff=yes --with-imageio=no
```
We must obtain this kind of configuration :
```
Image I/O Use ImageIO / Mac OS X: no
Use libjpeg: yes
Use zlib: yes
Use libpng: yes
Use libtiff: yes
Use libjasper: no
Use libIlmImf: no
```


Universal binary package : http://www.ient.rwth-aachen.de/cms/software/opencv/

```
./configure CFLAGS="-arch i386 -arch ppc" CXXFLAGS="-arch i386 -arch ppc" --with-libtiff=yes --enable-libtiff=yes --with-imageio=no --with-gtk=no --with-ffmpeg=no --enable-apps=no --enable-static=yes
```

OpenCV wiki for more information : http://opencv.willowgarage.com/wiki/Mac_OS_X_OpenCV_Port

Reference for configuring : _Building Universal Binaries from "configure"-based Open Source Projects_
http://developer.apple.com/technotes/tn2005/tn2137.html#TNTAG2


Or with MacPorts paths:
```
CPATH="/opt/local/include" LIBRARY_PATH="/opt/local/lib" ./configure --with-libtiff=yes --enable-libtiff=yes --with-imageio=no
```

For this configuration on a G5 :
```
General configuration ================================================
    Compiler:                 g++
    CXXFLAGS:                 -Wall -fno-rtti -pipe -O3 -g -mcpu=G3 -mtune=G5 -fomit-frame-pointer 

    Install path:             /usr/local

HighGUI configuration ================================================

    Windowing system --------------
    Use Carbon / Mac OS X:    yes
    Use gtk+ 2.x:             no
    Use gthread:              no

    Image I/O ---------------------
    Use ImageIO / Mac OS X:   no
    Use libjpeg:              yes
    Use zlib:                 yes
    Use libpng:               yes
    Use libtiff:              yes
    Use libjasper:            yes
    Use libIlmImf:            no

    Video I/O ---------------------
    Use QuickTime / Mac OS X: yes
    Use xine:                 no
    Use ffmpeg:               no
    Use dc1394 & raw1394:     no
    Use v4l:                  no
    Use v4l2:                 no

Wrappers for other languages =========================================
    SWIG                      
    Python                    yes

Additional build settings ============================================
    Build demo apps           yes
```


---

## Building packages ##

Reference : http://www.siteduzero.com/forum-83-234070-p1-un-bundle-mac-pour-vos-applications-qt4.html

(Required : Python : http://www.python.org/download/)