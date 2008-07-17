/***************************************************************************
 *           imgutils - Basic utilities for image processing 
 *
 *  Sun Oct 28 14:10:56 2007
 *  Copyright  2007  Christophe Seyve 
 *  Email cseyve@free.fr
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef IMGUTILS_H
#define IMGUTILS_H


// For Open CV Functions
#include <cv.h>
#include <cv.hpp>
#include <cvaux.h>

#ifndef tmmin 
#define tmmin(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef tmmax
#define tmmax(a,b) ((a)>(b)?(a):(b))
#endif

#ifdef LINUX //not __X11_DARWIN__
#define TMP_DIRECTORY	"/dev/shm/"
#define IMG_EXTENSION	".pgm"
#else
#define TMP_DIRECTORY	"/Users/tof/tmp/"
#define IMG_EXTENSION	".jpg"
#endif

typedef unsigned char u8;
typedef unsigned short u16;

#ifdef IMGUTILS_CPP
#define IMGUTILS_EXTERN
#define IMGUTILS_NULL	=0
#else
#define IMGUTILS_EXTERN	extern
#define IMGUTILS_NULL
#endif

IMGUTILS_EXTERN u8 g_debug_savetmp IMGUTILS_NULL;

/** @brief Return image layer depth in bytes (e.g. 8bit jpeg is 1 byte, 16bit:2bytes...) */
int tmByteDepth(IplImage * iplImage);

/** @brief Process a dilatation */
void tmDilateImage(IplImage * src, IplImage * dst);

/** S@brief ave an OpenCV IplImage as file */
void tmSaveImage(const char * filename, IplImage * src);

/** @brief Do a contrained region growing around a seed */ 
void tmGrowRegion(unsigned char * growIn, unsigned char * growOut, 
	int swidth, int sheight, 
	int c, int r, 
	unsigned char threshold,
	unsigned char fillValue,
	CvConnectedComp * areaOut);

/** @brief Crop an IplImage around a point in another IplImage */
void tmCropImage(IplImage * origImage, IplImage * cropImage, int center_x, int center_y);

/** @brief Return the ratio of pixels non 0 in an IplImage in a region */
float tmNonZeroRatio(IplImage * origImage, int x, int y, int w, int h);

/** @brief Add a border to image to reach a 4x size */
IplImage * tmAddBorder4x(IplImage * originalImage);

/** @brief Return correlation image */
IplImage * getCorrelationImage();

/** @brief Find a neighbour region to copy on the dust */
int tmSearchBestCorrelation(
	IplImage * origImage, IplImage * maskImage, 
	int orig_x, int orig_y, 
	int orig_width, int orig_height,
	// Output 
	int * copy_dest_x, int * copy_dest_y,
	int * copy_src_x, int * copy_src_y,
	int * copy_width, int * copy_height,
	int * best_correlation
	);

/** @brief Copy a part of an image into itself ('clone tool') */
void tmCloneRegion(IplImage * origImage, 
	int orig_x, int orig_y, 
	int copy_x, int copy_y,
	int copy_width, int copy_height,
	IplImage * destImage = NULL);

/** @brief Mark the copy action from a part of an image into itself ('clone tool') */
void tmMarkCloneRegion(IplImage * origImage, 
	int orig_x, int orig_y, 
	int copy_x, int copy_y,
	int copy_width, int copy_height,
	bool mark = false);
	
/** @brief Mark failure as a gray region */
void tmMarkFailureRegion(IplImage * origImage, 
	int x, int orig_y, int w, int h, unsigned char color = 127);

/** @brief Allocate a morphology structural element */
IplConvKernel * createStructElt();

/** @brief Process a close morphology operation */
void tmCloseImage(IplImage * src, IplImage * dst, IplImage * tmp, int iterations);
/** @brief Process an open morphology operation */
void tmOpenImage(IplImage * src, IplImage * dst, IplImage * tmp, int iterations);

#define FILM_UNDEFINED	0
#define FILM_NEGATIVE	1
#define FILM_POSITIVE	2


/** @brief Process difference between original image and blurred image, depending on film type 
\param l_FilmType film type (0=FILM_UNDEFINED, 1=FILM_NEGATIVE, 2=FILM_POSITIVE)
\param original (input) image 
\param blurred image 
\param difference image = output
\param diffHisto difference histogram (may be NULL)
*/
int processDiff(int l_FilmType, IplImage * grayImage,  IplImage * medianImage,  
	IplImage * diffImage, 
	unsigned long * diffHisto);

#endif
