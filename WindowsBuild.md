# Introduction #

This page explains how to build Tamanoir on Windows.


# Details #

Building Tamanoir on Windows.

You need :
> - OpenCV : to load/save images, to manipulate some image buffers (libs cxcore, highgui)
> - Qt4 for Windows
> - an IDE/... : you should try QtCreator, because the installer package provides Qt4, MinGW compiler and the IDE.


---

## Qt4 + QtCreator ##

Easiest way : download
Download : http://.qt.nokia.com/ download LGPL
Then install


---

## OpenCV ##

### OpenCV **without** 16bit TIFF support ###


### OpenCV **with** 16bit TIFF support ###
Download patched version for 16bit support : http://cseyve.free.fr/Vision/download/opencv-tiff16bit.tar.bz2

I don't now how to compile it for Windows.


---

## LibTIFF for Windows ##

Download setup version
http://gnuwin32.sourceforge.net/packages/tiff-win32.htm


---

## Tamanoir ##

Download snapshot :
```
# Non-members may check out a read-only working copy anonymously over HTTP.
svn checkout http://tamanoir.googlecode.com/svn/trunk/ tamanoir-read-only
```

Compile Tamanoir :
  * with QtCreator : open the Tamanoir.pro project file, then build and run.