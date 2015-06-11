**Tamanoir is an image processing tool to remove small dust from picture scans.**

---

_This introduction in French/Cette introduction en français: [IntroductionFrancais](IntroductionFrancais.md)_

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Intro.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Intro.png)

Most of the flatbed scanners do not provide optical ICE, but only digital ICE, which is very slow, sometimes with weird artefacts on picture, and not available with every scanning software (espescially when running Linux).

The aim of Tamanoir is to pre-process images with a simple and fast user interface to remove most of the small dusts. It detects the small objects on the picture, then tries to propose a correction by cloning another part of image in the neighbour of the dust. The user can skip or apply the correction.

With this processing, 90% of the dusts are removed.

Then you can correct the remaining dusts and fibers on another image manipulation tool (Gimp, Lightroom, Krita...).

Please read HowToUseTamanoir

####  ####

New release candidates :
  * Linux Debian 7 Wheezy package: [https://tamanoir.googlecode.com/files/tamanoir_1.svn20130305-1_i386.deb]
  * Linux Debian Lenny / Ubuntu >8.04 package: http://tamanoir.googlecode.com/files/tamanoir_1.svn20100308-1_i386.debtamanoir_0.svn20090407-1_i386.deb (380 kB, dynamically linked to Qt4 and OpenCV)
  * MacOS X : http://tamanoir.googlecode.com/files/Tamanoir-MacOSX-svn20100309.zip (Universal binary, 16 MB with Qt4.6 and OpenCV)
  * Windows 32bit : http://tamanoir.googlecode.com/files/Tamanoir-Win32-svn20100314.zip (5.4 MB with Qt4.6 and OpenCV)

####  ####

### Status ###

Version: beta.

Portability: Linux, MacOS X and Windows.


### Dependencies ###

Tamanoir is written in C/C++, and uses libraries :

- OpenCV for image import/export, and libtiff to support 16bit TIFF compressed export, and above all, image processing functions.

- Qt4 for portable GUI (Linux, MacOS X, Windows).

See [MacOSXBuild](MacOSXBuild.md)
See [LinuxBuild](LinuxBuild.md)