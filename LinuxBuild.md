# Introduction #

This page explains how to build Tamanoir on Linux.


# Details #

Building Tamanoir on Linux.

You need :
  * OpenCV : to load/save images, to manipulate some image buffers (libs cxcore, highgui)
  * libTIFF for 16bit TIFF support
  * Qt4 for Linux
  * Tamanoir



---

## Libtiff ##

Download : http://www.libtiff.org/
Compile and Install :
```
./configure ; make ; sudo make install
```

Or simply use your distribution package :
```
apt-get install libtiff-dev
```


---

## OpenCV ##

Install a development version of OpenCV with your package manager, for example:
```
apt-get install libhighgui-dev
```


---

## Qt4 ##

Download : http://trolltech.com/downloads/opensource
Install Qt4. For example:
```
apt-get install qt4-dev
```


---

## Tamanoir ##

Download snapshot :
```
# Non-members may check out a read-only working copy anonymously over HTTP.
svn checkout http://tamanoir.googlecode.com/svn/trunk/ tamanoir-read-only
```

Compile Tamanoir :
```
cd Tamanoir/
qmake Tamanoir.pro
```

or for Debian 4 Etch with both Qt3 and Qt4:
```
qmake-qt4 Tamanoir.pro
```

Then build :
```
make
./tamanoir
```



---

# Additional notes to rebuild OpenCV #


---

## libpng ##

Download : http://libpng.sourceforge.net/ - [ftp://ftp.simplesystems.org/pub/png/](ftp://ftp.simplesystems.org/pub/png/)
Compile and Install :
./configure ; make ; sudo make install


---

## OpenCV ##

### OpenCV **without** 16bit TIFF support ###

Install a development version of OpenCV with your package manager, for example:
```
apt-get install libhighgui-dev
```

### OpenCV **with** 16bit TIFF support ###
Download patched version for 16bit support : http://cseyve.free.fr/Vision/download/opencv-tiff16bit.tar.bz2

We must compile without ImageIO to use the libtiff for 16bit support :
```
./configure --with-libtiff=yes --enable-libtiff=yes --with-imageio=no
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

Then build :
```
make
make install
```


---

# Debian package #

Tamanoir use the "native Debian packagin" model, where the debian/ foler and its control files are maintained in the upstream repository. Instructions to build a Debian package should work on any Debian-based distribution (Ubuntu for instance):
```
$ svn checkout http://tamanoir.googlecode.com/svn/trunk/ tamanoir
$ cd tamanoir
$ ./build_debian.sh
```

The package is built in _/tmp_