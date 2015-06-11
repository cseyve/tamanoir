# Introduction #

This page explains how to use the Tamanoir user interface to correct your picture scans.

It presents the interface components, the usefull shortcuts and a few hints for an efficient use of Tamanoir/

The user manual is separated in 2 parts :
  * a **Step by step tutorial**
  * details of each functions or options

# Interface #

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Global.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Global.png)

Tamanoir's GUI is divided in several sections:
  * **Detailed views of dust and correction** on top
  * **Load/save buttons** on bottom-left
  * **Global picture display** centered on bottom
  * **Action buttons** to correct, skip correction, ...
  * **Options:** film type, correction accelerations and options

Every display and button show a tooltip when the mouse stays upon it.



---


# Typical use case / Step by step tutorial #

Tamanoir has been designed to avoid too many and too long operations to corret the dusts on scans. With 2 main concepts :
  1. the software detects automatically the dusts, then propose a correction to the user. It corrects a dust by a copy/paste (or clone) operation, while giving the choise to the user ether to accept (apply) or refuse the correction.
  1. it gives to the user another tool to manually correct the remaining dusts and fibers : the clone tool.

The typical use case of Tamanoir is :
> $ we open a picture : select the file you want to clean :
![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Open.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Open.png)
    * Wait for the end of the loading and pre-processing. The progress is shown under the "Save" button
![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Save.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Save.png)

  * Check if the options match the film type (pisitive/negative) and the scan resolution
![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Options.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Options.png)

  * The first pass in semi-auto mode : the software detects dusts and propose corrections. The user uses the buttons or the shortcuts to apply ('A' for Apply) or to go  to next proposal (right arrow key).
> At very proposal, the displays present the original image on the left and the corrected image on the right, at 100% scale (1:1)
> ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Details.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Details.png)
> The user can :
    * **Modify the proposed correction** by changing either the position of copy source (green rectangle) or the destination (end of the arrow) or the size (with the mouse wheel or by resiznig the rectangle). The corrected image is refreshed in real time.
> > ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Actions.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Actions.png)
    * **Apply (validate) or refuse the correction**. The acceptation is made with the "Apply" button or the 'A' key. ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-A.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-A.jpg)
> > The user can skip this proposal with the "Skip" button or the right arrow key.    ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Right.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Right.jpg)
> > If the user skipped the propsal too fast, he can go back with the "<" button or the left arrow key. ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Left.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Left.jpg)


> At anytime, the user can mark a region by pression the 'M' key ni order to come back later in second pass.

> Once this pass is terminated, **most of the dusts have been removed**. Nevertheless a few dusts have probably been missed (not detected or with no proposal). The user has to correct them manually. Anyway, the software is unable to correct fbers on image regions with details, because they are too big and a single copy/paste proposal does not fit.
> So there has to be a second pass in manual mode.

  * The second pass is done n manual mode : the user presses the "Home" key    ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-home.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-home.jpg) to set the region of interest on top-left corner of image. He corrects the remaining dusts with the clone tool (use 'C' key to switch the state of the "Clone" button). Then he goes to next block by pressing the "Page down" key.  ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-PageDown.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-PageDown.jpg)
> There it goes until the bottom-right corner of the picture.

  * The user save the image by pressing the "Save" button or the "Ctrl+S" shortcut. The corrected image use the same file name as the original, and the original is backuped width the "-copy._ext_" extention.



---


# Details #


---


# Load / Save #

  * **Load:** Open a dialog to choose a file, then pre-process the picture with current options/settings, and goes to first detected dust

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-OpenFile.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-OpenFile.png)

  * **Save:** save current image with the **same name than original**. A backup is stored with a _-copy_ extension by default for the moment. This backup may be an option only in the future versions.


# Global view / Main display #

The large image display shows the grayscaled version of input image, with inertion of small rectangles :
  * **Blue rectangle** where the dust has just been detected. This dust is displayed in blue in global view, and a crop of the neighbouring is shown in detail views (at scale 1:1).
  * **Green rectangles** where the proposed corrections have been accepted by the user ("Apply" button).
  * **Yellow rectangles** where the proposed corrections have been refused by the user ("Skip" button).
  * **Red rectangles** where the user marked a region.
![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-GlobalView.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-GlobalView.png)

# Detail views #

There are 2 image displays for detailled view of current dust :
  * Top-left is a crop of **original image** around the detected dust. The proposed correction is represented by a green rectangle for the source of cloning, and the destination of the cloning is linked to source by a green line. **You can move the position of the source with the mouse**.
  * Top-right is the **corrected image**. If you move the source of the cloning on the original crop display, this display is refreshed immediatly. If the mouse goes over this display, the dust is shown in red.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Details.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Details.png)

# Action buttons : Auto/Skip/Apply #

There are 3 kind of actions :
  * **Skip button:** make the process to go to next detected dust **without applying the correction** on original image. The user refuses the proposed correction.
  * **Apply button:** apply the proposed correction then goes to next dust.
  * **Back button "<":** go back one dust which has been skipped. It's usefull when we skip many dusts very fast, and skip by error a good proposal.
  * **Rewind button "|<":** restart dust detection from top-left of image. You will have to refuse all the dusts you have already cancelled.
  * **Auto button:** process all the image fast. It only correct trivial dust, like in _"Trust mode"_.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Actions.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Actions.png)


# Film settings : type and resolution #

Film options are :
  * **Film type:** the choices are :
    * _Undefined_: the dusts are detected if they are lighter or darked than the background
    * _Positive_: for scans from positive images, slides or digital camera pictures. The dusts are only detected if they are darked than the background
    * _Negative_: for scans from negative film. The dusts are detected only if they are lighter than the background

Color or black&white pictures are detected automatically. If the picture present much grain, the image processing are tuned whithout informing the user.

  * Scan resolution : it modifies the size of the smallest detectable dust. The more dpi, the bigger the dust must be in images.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FilmSettings.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FilmSettings.png)


# Action options #

There are 2 options to accelerate the use of Tamanoir for the user :
  * **Trust mode:** the corrections are automatically applied if the proposal is really good.
  * **Empty area detection:** it allows the software to detect the dusts only on uniform areas, such as skies or uniform walls, ...
  * **Sensitivity** : to lower its level of sensitivity, when there are many small dusts that do not mind for the user

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Options.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-EN-Options.png)



---


Comments and suggestions are welcome !! We are looking for new contributors too...