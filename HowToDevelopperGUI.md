# Introduction #

This page explains how to use the Tamanoir user interface to correct your picture scans.


# Details #

**Caution:** _For the moment, only color slides are well supported for dust detection. See ToDo for future developments._

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Global.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Global.png)

Tamanoir's GUI is divided in several sections:
  * **Load/save buttons** on top line
  * **Global picture display** on left
  * **Detailed views of dust and correction** on right
  * **Action buttons** to correct or skip correction
  * **Options:** film type, correction accelerations

Each button/view contains a tooltip which explains its function.


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
  * **Red rectangles** where the process could not find any fitting correction.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-MainDisplay.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-MainDisplay.png)

# Detail views #

There are 4 image displays for detailled view of currnt dust :
  * Top-left is a crop of **original image** around the detected dust. The proposed correction is represented by a green rectangle for the source of cloning, and the destination of the cloning is linked to source by a green line. **You can move the position of the source with the mouse**.
  * Bottom-left is the **corrected image**. If you move the source of the cloning on the original crop display, this display is refreshed immediatly.

  * Top-right is the binary image of detected dust. It's almost shown for debug purpose.
  * Bottom-right is the binary image of high frequency regions in original images. It's almost shown for debug purpose.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Dust_and_correction.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Dust_and_correction.png)

# Action buttons : Auto/Skip/Apply #

There are 3 kind of actions :
  * **Skip button:** make the process to go to next detected dust **without applying the correction** on original image. The user refuses the proposed correction.
  * **Apply button:** apply the proposed correction then goes to next dust.
  * **Back button "<":** go back one dust which has been skipped. It's usefull when we skip many dusts very fast, and skip by error a good proposal.
  * **Rewind button "|<":** restart dust detection from top-left of image. You will have to refuse all the dusts you have already cancelled.
  * **Auto button:** process all the image fast. It only correct trivial dust, like in _"Trust mode"_.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Action_buttons.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Action_buttons.png)


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
  * **Hot pixels detection:** it allows the software to detect 1-pixel light "dusts" in images. This should be used with "Auto" action or "Trust mode" to be acceptable for user.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Options.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-Options.png)



---


Comments and suggestions are welcome !! We are looking for new contributors too...