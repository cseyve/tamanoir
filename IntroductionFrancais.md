**Tamanoir est un outil de dépoussiérage de scans par traitement d'images**

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Intro.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Intro.png)

# Introduction #


---


La plupart des scanners à plat ne fournissent pas de dépoussiérage (ICE) optique. Les modèles moyenne gamme (Epson 4490, 4990, v700 ...) fournissent un dépoussiérage logiciel, ou Digital ICE, qui n'est de plus pas accessible avec n'importe que logiciel (en particulier sous Linux) mais surtout génère fréquemment des artéfacts sur les images produites.

L'objectif de Tamanoir est de fournir un outil de pré-traitement des photos avec une interface utilisateur simple et efficace. L'objectif est d'enlever 90% des poussières sur le scan brut avec Tamanoir, rapidement, de manière à effectuer le plus gros du travail avec le moins de manipulations possibles.

Tamanoir détecte les petites poussières dans l'image, et essaie de trouver une zone de couleur et texture identique dans le voisinage des poussières détectées, puis les propose à l'utilisateur qui valide ou non la correction.

Environ 80 à 90 % des poussières sont détectées sur des films avec peu de grain (ex: diapo 100 ISO). Les 10% restants ainsi que les fibres, rayures... pourront être enlevés dans un logiciel tiers (Gimp, Photoshop, Krita, Lightroom...).

Merci de lire le [ModeDEmploiTamanoir](ModeDEmploiTamanoir.md)

### Paquets ###

Candidats à la seconde version (_release candidate_) :
  * Linux Debian Lenny / Ubuntu >8.04 package : http://tamanoir.googlecode.com/files/tamanoir_1.svn20100308-1_i386.debtamanoir_0.svn20090407-1_i386.deb (380 ko, lié dynamiquement à Qt4 et OpenCV)
  * MacOS X : http://tamanoir.googlecode.com/files/Tamanoir-MacOSX-svn20100309.zip (Universal binary, 16 Mo avec Qt4.6 et OpenCV)
  * Windows 32bit : http://tamanoir.googlecode.com/files/Tamanoir-Win32-svn20100314.zip (5.4 Mo avec Qt4.6 et OpenCV)

### Statut ###

Version : beta.

Portabilité : Linux, MacOS X (Intel) et Windows.

### Dépendances ###

Tamanoir est écrit en C/C++, et utilise les bibliothèques :

- OpenCV pour les fonctions de lecture/écriture et le traitemnt d'images, et libtiff pour le support des TIFF en 16 bits.

- Qt4 pour l'interface utilisateur portable (Linux, MacOS X, Windows).

# Compilation #

Documentation en anglais :
  * pour MacOS X : voir [MacOSXBuild](MacOSXBuild.md)
  * pour Linux : voir [LinuxBuild](LinuxBuild.md)
  * pour Windows : voir [WindowsBuild](WindowsBuild.md)