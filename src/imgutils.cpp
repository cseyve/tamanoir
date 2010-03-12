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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>



#define IMGUTILS_CPP

#include "imgutils.h"

#include <highgui.h>


FILE * logfile = stderr;
u8 g_debug_alloc = 0;

extern u8 g_debug_imgverbose;

static u8 tmInitGlobals_init = 0;
void tmInitGlobals() {
	if(tmInitGlobals_init) return;

	tmInitGlobals_init = 1;
#ifndef __LINUX__
	if(getenv( "HOME")) {
		strcpy(g_tmp_directory, getenv( "HOME"));
		strcat(g_tmp_directory, "/tmp/");

		umask(0000);
		int errorCode = mkdir(g_tmp_directory, 0777);
		if(errorCode != 0) {
			errorCode = errno;

		}

		if(errorCode == 0 || errorCode == EEXIST) {
			chmod(g_tmp_directory, 0777);
		} else {
			fprintf(stderr, "[imgutils] %s:%d : cannot create directory '%s'"
					" because of error=%d='%s'\n",
					__func__, __LINE__, g_tmp_directory,
					errorCode, strerror(errorCode));
			// So use the default directory
			strcpy(g_tmp_directory, TMP_DIRECTORY);
		}
	}
#endif

}

int tmByteDepth(IplImage * iplImage) {
	int byte_depth = iplImage->nChannels;


	/* IplImage:
	int  depth; // pixel depth in bits: IPL_DEPTH_8U, IPL_DEPTH_8S, IPL_DEPTH_16S,
			IPL_DEPTH_32S, IPL_DEPTH_32F and IPL_DEPTH_64F are supported
	*/

	switch(iplImage->depth) {
	case IPL_DEPTH_8U:
		byte_depth *= sizeof(unsigned char);
		break;
	case IPL_DEPTH_8S:
		byte_depth *= sizeof(char);
		break;
	case IPL_DEPTH_16U:
		byte_depth *= sizeof(u16);
		break;
	case IPL_DEPTH_16S:
		byte_depth *= sizeof(i16);
		break;
	case IPL_DEPTH_32S:
		byte_depth *= sizeof(i32);
		break;
	case IPL_DEPTH_32F:
		byte_depth *= sizeof(float);
		break;
	case IPL_DEPTH_64F:
		byte_depth *= sizeof(double);
		break;
	default:
		break;
	}

	return byte_depth;
}
/* Print image properties */
void tmPrintProperties(IplImage * img) {
	/*
		 *

	  IplImage
		|-- int  nChannels;     // Number of color channels (1,2,3,4)
		|-- int  depth;         // Pixel depth in bits:
		|                       //   IPL_DEPTH_8U, IPL_DEPTH_8S,
		|                       //   IPL_DEPTH_16U,IPL_DEPTH_16S,
		|                       //   IPL_DEPTH_32S,IPL_DEPTH_32F,
		|                       //   IPL_DEPTH_64F
		|-- int  width;         // image width in pixels
		|-- int  height;        // image height in pixels
		|-- char* imageData;    // pointer to aligned image data
		|                       // Note that color images are stored in BGR order
		|-- int  dataOrder;     // 0 - interleaved color channels,
		|                       // 1 - separate color channels
		|                       // cvCreateImage can only create interleaved images
		|-- int  origin;        // 0 - top-left origin,
		|                       // 1 - bottom-left origin (Windows bitmaps style)
		|-- int  widthStep;     // size of aligned image row in bytes
		|-- int  imageSize;     // image data size in bytes = height*widthStep
		|-- struct _IplROI *roi;// image ROI. when not NULL specifies image
		|                       // region  to be processed.
		|-- char *imageDataOrigin; // pointer to the unaligned origin of image data
		|                          // (needed for correct image deallocation)
		|
		|-- int  align;         // Alignment of image rows: 4 or 8 byte alignment
		|                       // OpenCV ignores this and uses widthStep instead
		|-- char colorModel[4]; // Color model - ignored by OpenCV

	 */
	fprintf(stderr, "[imgutils] %s:%d : IMAGE PROPERTIES : img=%p\n"
			"\t nChannels = %d\n"
			"\t depth = %d => %d bytes per pixel\n"
			"\t width = %d\n"
			"\t height = %d\n"
			"\t imageData = %p\n"
			"\t dataOrder = %d - %s\n"
			"\t origin = %d - %s\n"
			"\t widthStep = %d\n"
			"\t imageSize = %d\n"
			"\t align = %d (OpenCV ignores this and uses widthStep instead)\n"
			"\t colorModel = %c%c%c%c\n"
			"==============================\n\n"

			, __func__, __LINE__, img,
			img->nChannels,
			img->depth, tmByteDepth(img),
			img->width,
			img->height,
			img->imageData,
			img->dataOrder, (img->dataOrder==0?"INTERLEAVED":"SEPARATE"),
			img->origin,(img->origin==0?"TOP-LEFT":"BOTTOM-LEFT (Windows BMP)"),
			img->widthStep,
			img->imageSize,
			img->align,
			img->colorModel[0],img->colorModel[1],
				img->colorModel[2],img->colorModel[3]
			);

}

/** @brief Tracking of RAM usage */
typedef struct {
	IplImage * img_ptr;
	int ram_size;
	char file[32];
	char func[32];
	int line;
} t_iplimage_usage;

static t_iplimage_usage * g_iplimg_usage_tab = NULL;
static int g_max_iplimg_usage = 0;
static int g_iplimg_usage_total_ram = 0;

/* Create an IplImage width OpenCV's cvCreateImage and clear buffer */
IplImage * subCreateImage(const char * file, const char * func, int line,
						 CvSize size, int depth, int channels) {

	int usage_space = -1;
	if(!g_iplimg_usage_tab ) {
		g_max_iplimg_usage = 50;
		g_iplimg_usage_tab = new t_iplimage_usage [ g_max_iplimg_usage ];
		memset(g_iplimg_usage_tab, 0, sizeof(t_iplimage_usage) * g_max_iplimg_usage );
		usage_space = 0;
	} else {
		for(int sp=0; usage_space<0
			&& sp<g_max_iplimg_usage
			&& usage_space<0; sp++) {

			if(g_iplimg_usage_tab[sp].img_ptr == NULL ) {
				usage_space = sp;
			}
		}

		if(usage_space < 0) {
			// there was no more space => resize the
			int old_max_iplimg_usage = g_max_iplimg_usage ;
			t_iplimage_usage * old_iplimg_usage_tab = g_iplimg_usage_tab;
			g_max_iplimg_usage += 10;

			fprintf(stderr, "[utils] %s:%d : no more space => "
					"resize array %d => %d\n"
					"\t Still allocated : \n",
					__func__, __LINE__,
					old_max_iplimg_usage, g_max_iplimg_usage );
			for(int sp=0; sp<old_max_iplimg_usage; sp++) {
				if(g_iplimg_usage_tab[sp].img_ptr != NULL ) {
					if(g_debug_alloc) {
						fprintf(stderr, "\t[%d]\t%dx%dx%d @ [%s] %s:%d\n",
							sp,
							g_iplimg_usage_tab[sp].img_ptr->width,
							g_iplimg_usage_tab[sp].img_ptr->height,
							g_iplimg_usage_tab[sp].img_ptr->nChannels,
							g_iplimg_usage_tab[sp].file,
							g_iplimg_usage_tab[sp].func,
							g_iplimg_usage_tab[sp].line
							);
					}
				}
			}


			// Allocate new tab and copy old into new
			g_iplimg_usage_tab = new t_iplimage_usage [ g_max_iplimg_usage ];

			memset(g_iplimg_usage_tab, 0, sizeof(t_iplimage_usage)*g_max_iplimg_usage );
			memcpy(g_iplimg_usage_tab, old_iplimg_usage_tab, sizeof(t_iplimage_usage)*old_max_iplimg_usage );

			usage_space = old_max_iplimg_usage; // last old is new first available

			delete [] old_iplimg_usage_tab;
		}
	}


	if(g_debug_alloc) {
		fprintf(stderr, "[utils] %s:%d : creating IplImage : %dx%d x depth=%d x channels=%d\n",
			__func__, __LINE__,
			size.width, size.height, depth, channels);
	}
	IplImage * img = cvCreateImage(size, depth, channels);
	if(img) {
		g_iplimg_usage_tab[usage_space].img_ptr = img;
		g_iplimg_usage_total_ram += img->widthStep * img->height;
		g_iplimg_usage_tab[usage_space].ram_size = img->widthStep * img->height;


		strncpy(g_iplimg_usage_tab[usage_space].file, file, 31);
		g_iplimg_usage_tab[usage_space].file[31]='\0';
		strncpy(g_iplimg_usage_tab[usage_space].func, func, 31);
		g_iplimg_usage_tab[usage_space].func[31]='\0';
		g_iplimg_usage_tab[usage_space].line = line;

		if(!(img->imageData)) {
			fprintf(stderr, "[utils] %s:%d : ERROR : img->imageData=NULL while "
				"creating IplImage => %dx%d x depth=%d x channels=%d\n",
				__func__, __LINE__,
				img->width, img->height, img->depth, img->nChannels);
		}
		memset(img->imageData, 0, sizeof(char) * img->widthStep * img->height);
		return img;
	} else {
		fprintf(stderr, "[utils] %s:%d : ERROR : creating IplImage => %dx%d x depth=%d x channels=%d\n",
			__func__, __LINE__,
			img->width, img->height, img->depth, img->nChannels);
	}

	if(img->width==0 || img->height==0 || img->imageData==0) {
		fprintf(stderr, "[utils] %s:%d : ERROR : creating IplImage => %dx%d x depth=%d x channels=%d\n",
			__func__, __LINE__,
			img->width, img->height, img->depth, img->nChannels);
	}

	return img;
}

/** @brief Release an image and clear pointer */
void subReleaseImage(const char * file, const char * func, int line,
					 IplImage ** img) {
	if(!img) return;
	if(!(*img) ) return;

	int found = -1;
	for(int i = 0; found<0 && i<g_max_iplimg_usage; i++) {
		if(g_iplimg_usage_tab[i].img_ptr == *img) {
			found = i;
			if(g_debug_alloc) {
				fprintf(stderr, "[imgutils] %s:%d clear img [%d] %dx%dx%d "
					"allocated at [%s] %s:%d\n",
					__func__, __LINE__, i,
					(*img)->width, (*img)->height, (*img)->nChannels,
					g_iplimg_usage_tab[i].file, g_iplimg_usage_tab[i].func, g_iplimg_usage_tab[i].line);
			}
			memset(&g_iplimg_usage_tab[i], 0, sizeof(t_iplimage_usage));

			g_iplimg_usage_total_ram -= (*img)->widthStep * (*img)->height;
		}
	}

	if(found<0) {
		fprintf(stderr, "[imgutils] %s:%d : could not find where image has been allocated ! %dx%dx%d",
				__func__, __LINE__,
				(*img)->width, (*img)->height, (*img)->nChannels );
	}

	if( !(*img)->imageData ) return;
	cvReleaseImage(img);
	*img = NULL;
}


/** @brief print the list of allocated IplImage */
void tmPrintIplImages() {
	fprintf(stderr, "[imgutils] %s:%d : total RAM used : %d B = %G MB : \n",
			__func__, __LINE__,
			g_iplimg_usage_total_ram, (float)g_iplimg_usage_total_ram/(1024.f*1024.f)
			);
	for(int i = 0; i<g_max_iplimg_usage; i++) {
		if(g_iplimg_usage_tab[i].img_ptr) {
			fprintf(stderr, "\t%p : %d x %d x %d ch x %d bytes\n",
					g_iplimg_usage_tab[i].img_ptr,
					g_iplimg_usage_tab[i].img_ptr->width, g_iplimg_usage_tab[i].img_ptr->height,
					g_iplimg_usage_tab[i].img_ptr->nChannels, tmByteDepth(g_iplimg_usage_tab[i].img_ptr)
					);
		}
	}
}






/** process a dilatation around the dust */
void tmDilateImage(IplImage * src, IplImage * dst,
				   int kernel_size, // default: 3
				   int iterations // default: 1
				   ) {
	// Ref. : http://opencvlibrary.sourceforge.net/CvReference#cv_imgproc_morphology
	if(kernel_size <= 3) {
		cvDilate(src, dst,
			NULL /* e.g. 3x3 kernel */,
			iterations // # iterations
			);
	} else {
		// Avoid multiple dynamic allocations
		static IplConvKernel *elt = NULL;
		static int last_elt_size = 0;
		if(!elt || last_elt_size != kernel_size) {
			if(elt) {
				cvReleaseStructuringElement(&elt);
			}
			elt = tmCreateStructElt( kernel_size ); // with ellipse shape by default
			last_elt_size = kernel_size;
		}


		cvDilate(src, dst,
			elt,
			iterations // # iterations
			);

	}
}

/** process a dilatation around the dust */
void tmErodeImage(IplImage * src, IplImage * dst,
				   int kernel_size, // default: 3
				   int iterations // default: 1
				   ) {
	// Ref. : http://opencvlibrary.sourceforge.net/CvReference#cv_imgproc_morphology
	if(kernel_size <= 3) {
		cvErode(src, dst,
			NULL /* e.g. 3x3 kernel */,
			iterations // # iterations
			);
	} else {
		// Avoid multiple dynamic allocations
		static IplConvKernel *elt = NULL;
		static int last_elt_size = 0;
		if(!elt || last_elt_size != kernel_size) {
			if(elt) {
				cvReleaseStructuringElement(&elt);
			}
			elt = tmCreateStructElt( kernel_size ); // with ellipse shape by default
			last_elt_size = kernel_size;
		}

		cvErode(src, dst,
			elt,
			iterations // # iterations
			);
	}
}



// If image has a odd size, many functions will fail on OpenCV, so add a pixel
IplImage * tmAddBorder4x(IplImage * originalImage) {
	if(   (originalImage->width % 4) > 0
	   || (originalImage->height % 4) > 0
	   ) {

		fprintf(logfile, "[utils] %s:%d : => Size %dx%d is odd\n",
				__func__, __LINE__, originalImage->width, originalImage->height);
		int new_width = originalImage->width;
		while( (new_width % 4)) new_width++;
		int new_height = originalImage->height;
		while( (new_height % 4)) new_height++;

		fprintf(logfile, "[utils] %s:%d : => resize to %d x %d \n",
				__func__, __LINE__, new_width, new_height);
		IplImage * copyImage = tmCreateImage(
				cvSize( new_width, new_height),
				originalImage->depth,
				originalImage->nChannels);

		for(int r = 0; r < originalImage->height; r++) {
			memcpy( copyImage->imageData + r * copyImage->widthStep,
				originalImage->imageData + r * originalImage->widthStep, originalImage->widthStep);
		}

		IplImage * oldImage = originalImage;
		cvReleaseImage(&oldImage);

		originalImage = copyImage;

		return copyImage;
	}

	return originalImage;
}

/** Allocate a morphology structural element */
IplConvKernel * tmCreateStructElt(int size)
{
	IplConvKernel *elt;
	int shape = 2;
	CvElementShape shapes[3] = { CV_SHAPE_RECT, //a rectangular element;
								 CV_SHAPE_CROSS, //a cross-shaped element;
								 CV_SHAPE_ELLIPSE};

	elt = cvCreateStructuringElementEx (
			(int)size, (int)size,
			(int)(size/2), (int)(size/2),
			shapes[shape], NULL);
	return elt;
}

void tmOpenImage(IplImage * src, IplImage * dst, IplImage * tmp, int iterations) {

	IplConvKernel *elt = tmCreateStructElt();

	// perform open
	cvMorphologyEx (src, dst, tmp, elt,
					CV_MOP_OPEN,
					iterations);

	cvReleaseStructuringElement(&elt);
}

void tmCloseImage(IplImage * src, IplImage * dst, IplImage * tmp, int iterations) {

	IplConvKernel *elt = tmCreateStructElt(3);

	// perform open
	cvMorphologyEx (src, dst, tmp, elt,
					CV_MOP_CLOSE,
					iterations);

	cvReleaseStructuringElement(&elt);
}


void tmScaleMax(IplImage * diffImage, IplImage * displayDiffImage) {
	if(displayDiffImage) {
		if(g_debug_imgverbose) {
			fprintf(stderr, "[ImgUtils] %s:%d : downscale diffImage=%p %dx%d => %p %dx%d\n",
				__func__, __LINE__,
				diffImage, diffImage->width, diffImage->height,
				displayDiffImage, displayDiffImage->width, displayDiffImage->height
				);
		}
		float scale_y = (float)diffImage->height / (float)displayDiffImage->height;
		float scale_x = (float)diffImage->width / (float)displayDiffImage->width;
		int r_disp = 0;
		for(float f_r=0;f_r<diffImage->height
					&& r_disp<displayDiffImage->height; f_r+=scale_y, r_disp++) {
			int rmin = (int)f_r;
			if(rmin>=diffImage->height)
				continue;
			int rmax = (int)roundf(f_r + scale_y);
			if(rmax >= diffImage->height) { rmax = diffImage->height-1; }
			// scale r
			u8 * displayline = IPLLINE_8U(displayDiffImage, r_disp);
			int c_disp = 0;
			for(float f_c=0; f_c<diffImage->width
								&& c_disp<displayDiffImage->width;
				f_c+=scale_x, c_disp++)
			{
				// Scale c
				int cmin = (int)f_c;
				int cmax = (int)roundf(f_c + scale_x);
				if(cmax >= diffImage->width) { cmax = diffImage->width-1; }
				u8 diffmax = 0;
				for(int r = rmin; r<=rmax; r++) {
					u8 * diffline = IPLLINE_8U(diffImage, r);

					for(int c = cmin; c<=cmax; c++) {
						if(diffline[c]>diffmax) {
							diffmax = diffline[c];
						}
					}
				}

				displayline[c_disp] = diffmax;
			}
		}

	}
}

void tmScaleMin(IplImage * diffImage, IplImage * displayDiffImage) {
	if(displayDiffImage) {
		if(g_debug_imgverbose) {
			fprintf(stderr, "[ImgUtils] %s:%d : downscale diffImage", __func__, __LINE__);
		}
		float scale_y = (float)diffImage->height / (float)displayDiffImage->height;
		float scale_x = (float)diffImage->width / (float)displayDiffImage->width;
		int r_disp = 0;
		for(float f_r=0;f_r<diffImage->height
					&& r_disp<displayDiffImage->height; f_r+=scale_y, r_disp++) {
			int rmin = (int)f_r;
			if(rmin>=diffImage->height)
				continue;
			int rmax = (int)roundf(f_r + scale_y);
			if(rmax >= diffImage->height) { rmax = diffImage->height-1; }
			// scale r
			u8 * displayline = IPLLINE_8U(displayDiffImage, r_disp);
			int c_disp = 0;
			for(float f_c=0; f_c<diffImage->width
								&& c_disp<displayDiffImage->width;
				f_c+=scale_x, c_disp++)
			{
				// Scale c
				int cmin = (int)f_c;
				int cmax = (int)roundf(f_c + scale_x);
				if(cmax >= diffImage->width) { cmax = diffImage->width-1; }
				u8 diffmin = 255;
				for(int r = rmin; r<=rmax; r++) {
					u8 * diffline = IPLLINE_8U(diffImage, r);

					for(int c = cmin; c<=cmax; c++) {
						if(diffline[c]<diffmin) {
							diffmin = diffline[c];
						}
					}
				}
				displayline[c_disp] = diffmin;
			}
		}
	}
}

void tmScaleMean(IplImage * diffImage, IplImage * displayDiffImage) {
	if(displayDiffImage) {
		if(g_debug_imgverbose) {
			fprintf(stderr, "[ImgUtils] %s:%d : downscale diffImage", __func__, __LINE__);
		}
		float scale_y = (float)diffImage->height / (float)displayDiffImage->height;
		float scale_x = (float)diffImage->width / (float)displayDiffImage->width;
		int r_disp = 0;

		for(float f_r=0;f_r<diffImage->height
					&& r_disp<displayDiffImage->height; f_r+=scale_y, r_disp++) {
			int rmin = (int)f_r;
			if(rmin>=diffImage->height)
				continue;
			int rmax = (int)roundf(f_r + scale_y);
			if(rmax >= diffImage->height) { rmax = diffImage->height-1; }
			// scale r
			u8 * displayline = IPLLINE_8U(displayDiffImage, r_disp);
			int c_disp = 0;
			for(float f_c=0; f_c<diffImage->width
								&& c_disp<displayDiffImage->width;
				f_c+=scale_x, c_disp++)
			{
				// Scale c
				int cmin = (int)f_c;
				int cmax = (int)roundf(f_c + scale_x);
				if(cmax >= diffImage->width) { cmax = diffImage->width-1; }
				float diffmean = 0;
				for(int r = rmin; r<=rmax; r++) {
					u8 * diffline = IPLLINE_8U(diffImage, r);

					for(int c = cmin; c<=cmax; c++) {
						diffmean += diffline[c];
					}
				}
				int sq = (cmax-cmin+1)*(rmax - rmin+1);
				diffmean /= sq;
				if(diffmean >= 254.5f)
					displayline[c_disp] = (u8)255;
				else
					displayline[c_disp] = (u8)roundf(diffmean);
			}
		}
	}
}




CvScalar getFakeColor(int nChannels, unsigned char color)
{
	// Grayscaled image use a colormap, so we return the same color
	if(nChannels == 1) {
		return cvScalarAll(color);
	}


	int R=0,G=0,B=0;

	// RGB24
	switch(color) {
		case COLORMARK_CORRECTED:
			R=0; G=255; B=0;// Green
			break;
		case COLORMARK_REFUSED:
			R=255; G=255; B=0;// Yellow
			break;
		case COLORMARK_FAILED:
			R=255; G=0; B=0;// Red
			break;
		case COLORMARK_CURRENT:
			G = 255; // Blue
			break;
		default:
			break;
	}

	if(nChannels == 4) // invert
		return CV_RGB(B,G,R);

	return CV_RGB(R,G,B);
}

void tmMarkFailureRegion(IplImage * origImage,
	int x, int y, int w, int h, unsigned char color) {

	CvScalar col = getFakeColor(origImage->nChannels, color);
	cvRectangle(origImage,
			cvPoint(x - 1, y-1),
			cvPoint(x + w + 1, y + h + 1),
			col, 1);
}

/** Return the ratio of pixels non 0 in an IplImage in a region */
float tmNonZeroRatio(IplImage * origImage,
					int orig_x, int orig_y, int w, int h,
					int exclu_x, int exclu_y, int exclu_w, int exclu_h,
					u8 threshval)
{
	int orig_width = origImage->width;
	int orig_height = origImage->height;
	if(w <= 0)
		return 1.f;
	if(h <= 0)
		return 1.f;

	if(orig_x<0) { orig_x = 0; }
	if(orig_y<0) { orig_y = 0; }
	if(orig_x+w >= orig_width) { w=orig_width - orig_x; }
	if(orig_y+h >= orig_height) { h=orig_height - orig_y; }

	// Count pixels
	int nbpixnon0 = 0;
	int nbpix = 0;
	for(int y=orig_y; y<orig_y+h; y++) {
		u8 * origline = IPLLINE_8U(origImage, y);
		for(int x=orig_x; x<orig_x+w; x++) {
			if( (x<exclu_x || x >exclu_x+exclu_w)
				&& (y<exclu_y || y >exclu_y+exclu_h)) {
				if( origline[x] >= threshval) {
					nbpixnon0++;
				}
				nbpix++;
			}
		}
	}
	if(nbpix==0) return 0.f;
	return (float)nbpixnon0 / ((float)(nbpix));
}


/*
 * Fill a rectangle with a given color -> orig rectangle
 */
void tmFillRegion(IplImage * origImage,
	int center_x, int center_y,
	int fill_width, int fill_height,
	u8 fillValue)
{
	int orig_width = origImage->width;
	int orig_height = origImage->height;
	// Compute top-left corner coordinates
	center_x -= fill_width/2;
	center_y -= fill_height/2;

	if(center_x<0) { center_x = 0; }
	if(center_y<0) { center_y = 0; }

	// Clip destination
	if(center_x + fill_width >= orig_width)
		fill_width = orig_width - center_x;

	if(center_y + fill_height >= orig_height)
		fill_height = orig_height - center_y;

	if(fill_width <= 0 || fill_height <= 0) {
		fprintf(logfile, "imgutils : %s:%d : INVALID clear %dx%d +%d,%d\n",
			__func__, __LINE__,
			fill_width, fill_height,
			center_x, center_y);

		return;
	}

	if(g_debug_imgverbose)
		fprintf(logfile, "imgutils : %s:%d : clear %dx%d +%d,%d\n",
			__func__, __LINE__,
			fill_width, fill_height,
			center_x, center_y);

	// Clear buffer
	int pitch = origImage->widthStep;
	int byte_depth = tmByteDepth(origImage);
	int fill_length = fill_width * byte_depth;

	u8 * origImageBuffer = (u8 *)origImage->imageData;

	// Raw clear
	for(int y=0; y<fill_height; y++, center_y++) {

		memset(origImageBuffer + center_y * pitch + center_x*byte_depth,
			fillValue,
			fill_length);
	}
}
/** use cache to fasten

*/
static pthread_mutex_t * coeftab_mutex = NULL;
float *coeftab_src = NULL;
float *coeftab_dest = NULL;
int coeftab_size = 0;
int coeftab_w = 0;
int coeftab_h = 0;


/*
 * Clone copy rectangle -> orig rectangle
 */
void tmCloneRegion(IplImage * origImage,
	int dest_x, int dest_y,
	int src_x, int src_y,
	int copy_width, int copy_height,
	IplImage * destImage ) {


	// Compute top-left corner coordinates
	dest_x -= copy_width/2;
	src_x -= copy_width/2;
	dest_y -= copy_height/2;
	src_y -= copy_height/2;

	// Then call the top-left value
	tmCloneRegionTopLeft(origImage,
		dest_x, dest_y,
		src_x, src_y,
		copy_width, copy_height,
		destImage );
}

/*
 * Clone copy rectangle -> orig rectangle
 */
void tmCloneRegionTopLeft(IplImage * origImage,
	int dest_x, int dest_y,
	int src_x, int src_y,
	int copy_width, int copy_height,
	IplImage * destImage )
{
	if(!coeftab_mutex) {
		coeftab_mutex = new Mutex_t;
		MUTEX_INIT(coeftab_mutex);
	}

	if(!destImage)
		destImage = origImage;

	if(copy_width <= 0 || copy_height<=0) {
		fprintf(logfile, "imgutils : %s:%d : INVALID clone src=%d,%d+%dx%d => dest=%d,%d\n",
			__func__, __LINE__,
			src_x, src_y, copy_width, copy_height,
			dest_x, dest_y);
		return;
	}

	MUTEX_LOCK(coeftab_mutex);
	bool recompute_coeftab = false;
	if(copy_width * copy_height > coeftab_size) {
		// resize buffer
		delete [] coeftab_src;
		delete [] coeftab_dest;
		coeftab_size = copy_width * copy_height ;
		coeftab_src = new float [ coeftab_size ];
		memset(coeftab_src , 0, sizeof(float)*coeftab_size );
		coeftab_dest = new float [ coeftab_size ];
		memset(coeftab_dest, 0, sizeof(float)*coeftab_size );

		recompute_coeftab = true;
	}

	if(copy_width != coeftab_w ||
	   copy_height != coeftab_h ) {
		recompute_coeftab = true;
	}
	// reocmpute the coefficients
	if(recompute_coeftab ) {
		coeftab_w = copy_width;
		coeftab_h = copy_height;

//		fprintf(stderr, "[imgutils]::%s:%d : recompute A=%d,B=%d\n",
//				__func__, __LINE__, coeftab_w, coeftab_h);

		int coefheight_2 = copy_height/2;
		int coefwidth_2 = copy_width/2;
		int y,x;

		// Raw in center, proportional copy in border
		for(y=0; y<coeftab_h; y++) {
			int dy = (y - coefheight_2);
//unused			float dy2 = dy*dy;
			float dynorm = (float)-dy/(float)coefheight_2;
			int postab = y * coeftab_w;
			for(x = 0; x<coeftab_w; x++, postab++) {
				int dx = (x - coefwidth_2);
				double theta = atan2(dynorm, (float)dx/coefwidth_2);
				float radius = sqrt(dx*dx+dy*dy);
				float maxdx = cos(theta)*(float)coefwidth_2;
				float maxdy = sin(theta)*(float)coefheight_2;

				float maxrad = sqrt(maxdx*maxdx+maxdy*maxdy);
				float coef_copy;

				float nominal = 0.6f * maxrad;
				if(radius<nominal) {
					coef_copy = 1.f;
				} else {
					coef_copy = 1.f - (radius-nominal) / (maxrad-nominal);
				}


				if(coef_copy < 0.)
					coef_copy = 0.;
				else if(coef_copy > 1.)
					coef_copy = 1.;

				float coef_orig = 1.f - coef_copy;

				// store in tab
				coeftab_src[postab] = coef_orig;
				coeftab_dest[postab] = coef_copy;
			}
		}
	}
	/* if(g_debug_imgverbose) {
		fprintf(logfile, "imgutils : %s:%d : clone %d,%d+%dx%d => %d,%d\n",
			__func__, __LINE__,
			src_x, src_y, copy_width, copy_height,
			dest_x, dest_y);
	} */
/*
	if(dest_x<0) { dest_x = 0; }
	if(dest_y<0) { dest_y = 0; }
	if(src_x<0) { src_x = 0; }
	if(src_y<0) { src_y = 0; }

	// Clip copy or destination : fixme : limit only the region
	if( dest_y > src_y) {
		if(dest_y + copy_height > orig_height)
			copy_height = orig_height - dest_y;
	} else {
		if(src_y + copy_height > orig_height)
			copy_height = orig_height - src_y;
	}
	if( dest_x > src_x) {
		if(dest_x + copy_width > orig_width)
			copy_width = orig_width - dest_x;
	} else {
		if(src_x + copy_width > orig_width)
			copy_width = orig_width - src_x;
	}
*/
	// Copy buffer
	int pitch = origImage->widthStep;
	int byte_depth = tmByteDepth(origImage);
	int copylength = copy_width * byte_depth;
	int channels = origImage->nChannels;
	int src_width = origImage->width;
	int src_height = origImage->height;
	int dest_width = destImage->width;
	int dest_height = destImage->height;

	u8 * origImageBuffer = (u8 *)origImage->imageData;
	u8 * destImageBuffer = (u8 *)destImage->imageData;

	// Raw copy
	if(0) {
		for(int y=0; y<copy_height; y++, dest_y++, src_y++) {
			memcpy(destImageBuffer + dest_y * pitch + dest_x*byte_depth,
				origImageBuffer + src_y * pitch + src_x*byte_depth,
				copylength);
		}
	} else { // Copy with fading on borders
		int y,x;

		// Raw in center, proportional copy in border
		for(y=0; y<copy_height; y++, dest_y++, src_y++)
		{
			if(dest_y>=0 && dest_y<dest_height
			   && src_y>=0 && src_y<src_height) {

				int postab = y  * coeftab_w ;

				for(x = 0; x<copy_width; x++, postab++) {
					if(dest_x+x>=0 && dest_x+x<dest_width
					   && src_x+x>=0 && src_x+x<src_width)
					{
						float coef_copy = coeftab_dest[postab];
						float coef_orig = coeftab_src[postab];

						for(int d = 0; d<channels; d++) {
							float val_orig=0, val_copy=0;

							u8 * porig_u8 = NULL;
							u16 * porig_u16 = NULL;
							u8 * pdest_u8 = NULL;
							u16 * pdest_u16 = NULL;

							switch(origImage->depth) {
							default:
								break;
							case IPL_DEPTH_8U:
								pdest_u8 = (u8 *)((destImageBuffer + dest_y * origImage->widthStep)
													+ channels * (dest_x + x)) + d;
								porig_u8 = (u8 *)((origImageBuffer + dest_y * origImage->widthStep)
													+ channels * (dest_x + x)) + d;
								val_orig = (float)( *porig_u8);
								val_copy = (float) *( (u8 *)(origImageBuffer + src_y * origImage->widthStep)
													+ channels*( src_x + x) + d);
								break;
							case IPL_DEPTH_16U:
								pdest_u16 = (u16 *)(destImageBuffer + dest_y * origImage->widthStep)
													+ channels * (dest_x + x) + d;
								val_copy = (float) *( (u16 *)(origImageBuffer + src_y * origImage->widthStep)
													+ channels * (src_x + x) + d);
								porig_u16 = (u16 *)(origImageBuffer + dest_y * origImage->widthStep)
													+ channels * (dest_x + x) + d;
								val_orig = (float)( *porig_u16);
								break;
							}

							//float out_val = val_copy;
							float out_val = coef_orig * val_orig + coef_copy * val_copy;
							if(pdest_u8)
								*pdest_u8 = (u8)roundf(out_val);
							else if(pdest_u16)
								*pdest_u16 = (u16)roundf(out_val);
						}
					}
				}
			}
		}
	}
	MUTEX_UNLOCK(coeftab_mutex);
}

IplImage * tmClone(IplImage * img_src) {
	if(!img_src) return NULL;

	IplImage * img_dest = tmCreateImage(
			cvGetSize(img_src),
			img_src->depth,
			img_src->nChannels);
	tmCopyImage(img_src, img_dest);
	return img_dest;
}

/*
 * Copy an image in another
 */
void tmCopyImage(IplImage * img_src, IplImage * img_dest)
{
	if(img_dest->widthStep == img_src->widthStep
	   && img_dest->height == img_src->height) {
		memcpy(img_dest->imageData, img_src->imageData,
			   img_src->widthStep*img_src->height);
	} else {
		cvZero(img_dest); // prevent from using uninitialised pixels for valgrind
		int copy_width = tmmin(img_dest->widthStep, img_src->widthStep);
		fprintf(stderr, "[imgutils] %s:%d : copy %dx%d (pitch=%d) x %dx%d - into %dx%d (pitch %d) x %dx%d\n",
				__func__, __LINE__,
				img_src->width, img_src->height,  img_src->widthStep, img_src->nChannels, img_src->depth,
				img_dest->width, img_dest->height,  img_dest->widthStep, img_dest->nChannels, img_dest->depth
				);

		for(int r=0; r<tmmin(img_dest->height, img_src->height); r++) {
			memcpy( img_dest->imageData + r*img_dest->widthStep,
					img_src->imageData + r*img_src->widthStep,
					copy_width);
		}
	}
}

/*
 * Convert a colored/16bit image to 8bit gray */
IplImage * tmFastConvertToGrayscale(IplImage * img) {
	if(!img) { return NULL; }

	IplImage * grayImage = tmCreateImage(cvSize(img->width, img->height), IPL_DEPTH_8U, 1);

	// Convert
	switch(img->depth) {
	default:
		fprintf(stderr, "[imgutils] %s:%d : UNSUPPORTED DEPTH !!\n", __func__, __LINE__);
		tmReleaseImage(&grayImage);
		return NULL;
	case IPL_DEPTH_8U: // 8bit image as input
		switch(img->nChannels) {
		case 1:	// same size, image is already grayscaled
			fprintf(stderr, "[imgutils] %s:%d : just copy !!\n", __func__, __LINE__);
			tmCopyImage(img, grayImage);
			break;
		default: { // Use Green plane as grayscaled
			int offset = 1; // green
			int bytedepth = tmByteDepth(img);
			for(int r=0; r< img->height; r++) {
				u8 * buf_in = IPLLINE_8U(img, r);
				u8 * buf_out = IPLLINE_8U(grayImage, r);
				int pos = offset;
				for(int c = 0; c<grayImage->width; c++, pos+=bytedepth) {
					buf_out[c] = buf_in[pos];
				}
			}
			}break;
		}
		break;
	case IPL_DEPTH_16U: { // 16bit image as input
		int offset = 1; // green

		switch(img->nChannels) {
		case 1:	// same size, image is already grayscaled
			break;
		default:  // Use Green plane as grayscaled
			offset = 1+2; // green (1=MSB + MSB+LSB of red or blue)
		}

		int bytedepth = tmByteDepth(img);
		unsigned char * buf_in = (unsigned char *)img->imageData;
		unsigned char * buf_out = (unsigned char *)grayImage->imageData;
		for(int pos = offset; pos < img->widthStep * img->height; pos+=bytedepth, buf_out++) {
			*buf_out = buf_in[pos];
		}
		}break;
	}

	return grayImage;
}

void tmInsertImage(IplImage * cropImage, IplImage * destImage,
				   int insert_x, int insert_y)
{
	int dest_width = destImage->width;
	int dest_height = destImage->height;

	int crop_width = cropImage->width;
	int crop_height = cropImage->height;
	if(insert_x<0) return;
	if(insert_y<0) return;
	if(insert_x+crop_width>=dest_width) return;
	if(insert_y+crop_height>=dest_height) return;

	int byte_depth_dest = tmByteDepth(destImage);
	int byte_depth_crop = tmByteDepth(cropImage);
	if(byte_depth_dest != byte_depth_crop) {
		fprintf(stderr, "[imgutils] %s:%d : byte depth does not fit\n",__func__, __LINE__);
		return;
	}

	// Copy into dest image
	for(int y = 0; y<crop_height; y++) {
		memcpy(destImage->imageData + (insert_y+y)*destImage->widthStep + insert_x*byte_depth_dest,
			   cropImage->imageData + y*cropImage->widthStep,
			   cropImage->width * byte_depth_crop);
	}
}

/*
 * Crop image
 */
void tmCropImage(IplImage * origImage,
	IplImage * cropImage,
	int crop_x, int crop_y,
	bool threshold_false_colors)
{
	int orig_width = origImage->width;
	int orig_height = origImage->height;

	int crop_width = cropImage->width;
	int crop_height = cropImage->height;

	memset(cropImage->imageData, 0,  cropImage->widthStep * cropImage->height);

	// FIXME : test size and depth
	int xleft = tmmax(0, crop_x);
	int ytop = tmmax(0, crop_y);
	int xright = tmmin(orig_width, xleft + crop_width);
	int ybottom = tmmin(orig_height, ytop + crop_height);

	if(xleft >= xright) return;
	if(ytop >= ybottom) return;

	if((xright - xleft) < crop_width
	   || (ytop - ybottom) < crop_height) {
		// Clear destination buffer to prevent from using oudated values in region growing...
		cvZero(cropImage);
	}

	// Copy buffer
	int byte_depth = tmByteDepth(origImage);
	int byte_depth2 = tmByteDepth(cropImage);
	if(byte_depth != byte_depth2) {

		int copywidth = (xright - xleft)*byte_depth;

		if(cropImage->depth != IPL_DEPTH_8U) {
			fprintf(stderr, "[%s] %s:%d : unsupported conversion : depth %d=>%d for display image\n",
				__FILE__, __func__, __LINE__, byte_depth, byte_depth2);
			fprintf(stderr, "[%s] %s:%d : origImage: %d x ", __FILE__, __func__, __LINE__, origImage->nChannels);
			switch(origImage->depth) {
			case IPL_DEPTH_8U:
				fprintf(stderr, "IPL_DEPTH_8U\n");
				break;
			case IPL_DEPTH_8S:
				fprintf(stderr, "IPL_DEPTH_8S\n");
				break;
			case IPL_DEPTH_16U:
				fprintf(stderr, "IPL_DEPTH_16U\n");
				break;
			case IPL_DEPTH_16S:
				fprintf(stderr, "IPL_DEPTH_16S\n");
				break;
			case IPL_DEPTH_32S:
				fprintf(stderr, "IPL_DEPTH_32S\n");
				break;
			case IPL_DEPTH_32F:
				fprintf(stderr, "IPL_DEPTH_32F\n");
				break;
			case IPL_DEPTH_64F:
				fprintf(stderr, "IPL_DEPTH_64F\n");
				break;
			default:
				fprintf(stderr, "UNKNOWN\n");
				break;
			}
			fprintf(stderr, "[%s] %s:%d :cropImage: %d x ", __FILE__, __func__, __LINE__, cropImage->nChannels);
			switch(cropImage->depth) {
			case IPL_DEPTH_8U:
				fprintf(stderr, "IPL_DEPTH_8U\n");
				break;
			case IPL_DEPTH_8S:
				fprintf(stderr, "IPL_DEPTH_8S\n");
				break;
			case IPL_DEPTH_16U:
				fprintf(stderr, "IPL_DEPTH_16U\n");
				break;
			case IPL_DEPTH_16S:
				fprintf(stderr, "IPL_DEPTH_16S\n");
				break;
			case IPL_DEPTH_32S:
				fprintf(stderr, "IPL_DEPTH_32S\n");
				break;
			case IPL_DEPTH_32F:
				fprintf(stderr, "IPL_DEPTH_32F\n");
				break;
			case IPL_DEPTH_64F:
				fprintf(stderr, "IPL_DEPTH_64F\n");
				break;
			default:
				fprintf(stderr, "UNKNOWN\n");
				break;
			}
			exit(0);
		}


		if(origImage->depth == IPL_DEPTH_16U) {
			unsigned char * cropImageBuffer = (unsigned char *)(cropImage->imageData);

			copywidth = (xright - xleft);
			/*
				fprintf(stderr, "[imgutils] %s:%d : 16U->8U : orig={%dx%d x %d, crop:x:%d-%d, bytedepth=%d}"
					"\tcrop:{%dx%d x %d }\n",
					__func__, __LINE__,
					origImage->width, origImage->height, origImage->nChannels, xleft, xright, byte_depth,
					cropImage->width, cropImage->height, cropImage->nChannels );
			*/
			int nchannels2 = cropImage->nChannels;
			int ly = 0;
			int orig_pix_offset = xleft * byte_depth;

			for(int y=ytop; y<ybottom; ly++, y++) {
				unsigned char * origBuffer =
					(unsigned char *)(origImage->imageData
							+ 1 // To get the most significant byte MSB
							+ y * origImage->widthStep);
				int orig_pos = orig_pix_offset + (origImage->nChannels -  1)*2;
				//int orig_pos = orig_pix_offset;
				int crop_pix_offset = ly * cropImage->widthStep; // line offset
				for(int lx = 0; lx<copywidth; lx++,
					crop_pix_offset += nchannels2,
					orig_pos += byte_depth)  {// in pixels from left
					for(int ld = 0; ld<nchannels2; ld++) {// in component per pixel : for RGB : 0,1,2

						// Avoid 255 value for display
						unsigned char val = (unsigned char)(
							origBuffer[ orig_pos - ld*2 ]);
						//	origBuffer[ orig_pos + ld*2 ]);
						if(val > COLORMARK_THRESHOLD) {
							if(val == 255) val = 254; // always thrshold to 255
							if(threshold_false_colors) {
								val = COLORMARK_THRESHOLD;
							}
						}

						cropImageBuffer[
							crop_pix_offset
							+ ld // component offset
							] = val;
					}
				}
			}
		}
	} else {
		int copywidth = (xright - xleft)*byte_depth;
		if(copywidth < 0) return;
		int xmin_x_depth = xleft * byte_depth;

		u8 * cropImageBuffer = (u8 *)cropImage->imageData;
		u8 * origImageBuffer = (u8 *)origImage->imageData;

		int ly = 0;
		if(origImage->nChannels == 1) { // For 8bit, we can use false colors map, so we need to threshold
			if(!threshold_false_colors) {
				for(int y=ytop; y<ybottom; ly++, y++)
					memcpy(cropImageBuffer + ly * cropImage->widthStep,
						origImageBuffer + xmin_x_depth + y * origImage->widthStep, copywidth);
			} else {

				for(int y=ytop; y<ybottom; ly++, y++) {
					u8 * origLine = (u8 *)(origImage->imageData
										   + y * origImage->widthStep);
					u8 * cropLine = (u8 *)(cropImage->imageData
										   + ly * cropImage->widthStep);
					int c = 0;
					for(int x=xleft; x<xright; x++, c++) {
						u8 val = origLine[x];
						if(val > COLORMARK_THRESHOLD) {
							val = COLORMARK_THRESHOLD;
						}
						cropLine[c] = val;
					}
				}
			}
		} else {
			for(int y=ytop; y<ybottom; ly++, y++) {
				memcpy(cropImageBuffer + ly * cropImage->widthStep,
					origImageBuffer + xmin_x_depth + y * origImage->widthStep, copywidth);
			}
		}
/*
		if(cropImage->nChannels == 4) {
			FILE * f = fopen("/dev/shm/tmCropImage_cropImage-nChannels4.ppm", "wb");
			if(f) {
				fprintf(f, "P5\n%d %d\n255\n", cropImage->widthStep, cropImage->height);
				fwrite( cropImage->imageData, cropImage->widthStep, cropImage->height, f);
				fclose(f);
			}
		//	tmSaveImage ("/dev/shm/tmCropImage_cropImage-nChannels4.ppm", cropImage);
		}*/
	//tmSaveImage ("/dev/shm/tmCropImage_origImage.ppm", origImage);
	}
}

void tmMarkCloneRegion(IplImage * origImage,
	int orig_x, int orig_y,
	int copy_x, int copy_y,
	int copy_width, int copy_height,
	bool mark
	) {

//unused	int orig_width = origImage->width;
//unused	int orig_height = origImage->height;
/*
	// Clip copy
	if( orig_y > copy_y) {
		if(orig_y + copy_height > orig_height)
			copy_height = orig_height - orig_y;
	} else {
		if(copy_y + copy_height > orig_height)
			copy_height = orig_height - copy_y;
	}
	// Clip copy
	if( orig_x > copy_x) {
		if(orig_x + copy_width > orig_width)
			copy_width = orig_width - orig_x;
	} else {
		if(copy_x + copy_width > orig_width)
			copy_width = orig_width - copy_x;
	}
	if(copy_width < 0)
		return;
*/
	// Debug : original (=bad) as red, copy source (=good) in green
	if(mark) {
		CvScalar color =(origImage->nChannels > 1 ? CV_RGB(255,0,0) :
					   cvScalarAll(COLORMARK_FAILED) );


				/*
		if(origImage->nChannels > 1)
			cvRectangle(origImage,
					cvPoint(orig_x-copy_width/2, orig_y-copy_height/2),
					cvPoint(orig_x+copy_width/2, orig_y+copy_height/2),
					CV_RGB(255,0,0),
					1);
		else
			cvRectangle(origImage,
					cvPoint(orig_x-copy_width/2, orig_y-copy_height/2),
					cvPoint(orig_x+copy_width/2, orig_y+copy_height/2),
					cvScalarAll(COLORMARK_FAILED), // red in 8bit image
					1);
		*/
		cvEllipse(origImage,
					cvPoint( orig_x, orig_y),
					cvSize( (copy_width+1)/2, (copy_height+1)/2),
					0, 0, 360,
					color,
					1);
	}

/*
	cvRectangle(origImage,
				cvPoint(copy_x-copy_width/2, copy_y-copy_height/2),
				cvPoint(copy_x+copy_width/2, copy_y+copy_height/2),
				(origImage->nChannels>1?
					CV_RGB(0,255,0) :
					cvScalarAll(COLORMARK_CORRECTED)),
				1);
*/
	cvEllipse(origImage,
					cvPoint( copy_x, copy_y),
					cvSize(copy_width/2+1, copy_height/2+1),
					0, 0, 360,
					(origImage->nChannels>1?
						CV_RGB(0,255,0) :
						cvScalarAll(COLORMARK_CORRECTED)),
					1);
	// Copy vector
	int copy_center_x = copy_x;
	int copy_center_y = copy_y;
	int src_center_x = orig_x;
	int src_center_y = orig_y;
	int copy_vector_x = copy_center_x - src_center_x;
	int copy_vector_y = copy_center_y - src_center_y;

	float vector_len = sqrt(copy_vector_x*copy_vector_x+copy_vector_y*copy_vector_y);
	if(vector_len > 10) {
		float shorter_len = vector_len - 10.f;
		if(shorter_len < 8) {
			shorter_len = 8;
		}

		copy_vector_x = (int)roundf((float)copy_vector_x * shorter_len / vector_len);
		copy_vector_y = (int)roundf((float)copy_vector_y * shorter_len / vector_len);
	}

	// Main move
	cvLine(origImage,
		   cvPoint(copy_center_x, copy_center_y),
		   cvPoint(copy_center_x - copy_vector_x, copy_center_y - copy_vector_y),
				(origImage->nChannels>1?
					CV_RGB(0,255,0) :
					cvScalarAll(COLORMARK_CORRECTED)),
		   1);

	int arrow_vector_x = -copy_vector_x;
	int arrow_vector_y = -copy_vector_y;

	// Arrow
	cvLine(origImage,
		   cvPoint(copy_center_x + arrow_vector_x*3/4 - arrow_vector_y*1/4 ,
				   copy_center_y + arrow_vector_y*3/4 + arrow_vector_x*1/4 ),
		   cvPoint(copy_center_x - copy_vector_x, copy_center_y - copy_vector_y),
				(origImage->nChannels>1?
					CV_RGB(0,255,0) :
					cvScalarAll(COLORMARK_CORRECTED)),
		   1);
	cvLine(origImage,
		   cvPoint(copy_center_x + arrow_vector_x*3/4 + arrow_vector_y*1/4 ,
				   copy_center_y + arrow_vector_y*3/4 - arrow_vector_x*1/4 ),
		   cvPoint(copy_center_x - copy_vector_x, copy_center_y - copy_vector_y),
				(origImage->nChannels>1?
					CV_RGB(0,255,0) :
					cvScalarAll(COLORMARK_CORRECTED)),
		   1);
}


float tmCorrelation(
	IplImage * img1, IplImage * img2,
	IplImage * maskImage,
	int x1, int y1,
	int x2, int y2,
	int w, int h,
	int difftolerance,
	int * pmaxdiff,
	int * pnbdiff)
{
	// Don't process on borders
	if(x1<0 || y1<0 || x1+w>img1->width || y1+h>=img1->height)
	{
		*pmaxdiff = 1001;
		return 1001.f;
	}
	if(x2<0 || y2<0 || x2+w>img2->width || y2+h>=img2->height)
	{
		*pmaxdiff = 1002;
		return 1002.f;
	}
	/* IplImage:
	int  depth; // pixel depth in bits: IPL_DEPTH_8U, IPL_DEPTH_8S, IPL_DEPTH_16S,
						   IPL_DEPTH_32S, IPL_DEPTH_32F and IPL_DEPTH_64F are supported
	*/
	*pmaxdiff = 1003;
	long diff = 0;
	long nbpix = 0;
	long nbdiff = 0;

	int pitch1 = img1->widthStep;
	int pitch2 = img2->widthStep;
	int pitchmask = maskImage->widthStep;

	int channels = img1->nChannels;
	//int depth = tmByteDepth(img1);
	//int w_x_depth = w*depth;
	//int w_x_channels = w * channels;
	int x1_x_depth = x1 * tmByteDepth(img1); // e.g. nChannel * pixel byte depth
	int x2_x_depth = x2 * tmByteDepth(img2);
	int wxh_4 = w * h * channels / 4;
	long maxdiff = 0;

	switch(img1->depth) {
	case IPL_DEPTH_8S:
		fprintf(logfile, "[imgutils] %s:%d : unsupported depth IPL_DEPTH_8S\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_16S:
		fprintf(logfile, "[imgutils] %s:%d : unsupported depth IPL_DEPTH_16S\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_32S:
		fprintf(logfile, "[imgutils] %s:%d : unsupported depth IPL_DEPTH_32S\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_32F:
		fprintf(logfile, "[imgutils] %s:%d : unsupported depth IPL_DEPTH_32F\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_64F:
		fprintf(logfile, "[imgutils] %s:%d : unsupported depth IPL_DEPTH_64F \n", __func__, __LINE__);
		break;
	case IPL_DEPTH_8U: {

		// Image buffers
		u8 * img1buffer = (u8 *)img1->imageData;
		u8 * img2buffer = (u8 *)img2->imageData;
		u8 * maskbuffer = (u8 *)maskImage->imageData;
		//u8 * diffbuffer = (u8 *)diffImage->imageData;

		// Do a distance correlation between the 2 images
		for(int dy = 0; dy<h; dy++) {
			u8 * img1pos = img1buffer + (y1+dy)*pitch1   	+ x1_x_depth;
			u8 * maskpos = maskbuffer + (y1+dy)*pitchmask	+ x1;

			u8 * img2pos = img2buffer + (y2+dy)*pitch2		+ x2_x_depth;

			for(int dx=0; dx<w; dx++) {
				// Don't compute if there is a doubt about this image (dust presence)
				// Compute only if mask is 0
				if( *maskpos == 0 ) {
					for(int d=0; d<channels; d++) {

						long l_diff = abs( (long)(*img1pos) - (long)(*img2pos) );

						if(l_diff > VISIBLE_DIFF) {
							nbdiff++;
							if(nbdiff > wxh_4) {// Too many pixels different / surface of object
								if(pnbdiff)
									*pnbdiff = nbdiff;
								if(pmaxdiff)
									*pmaxdiff = maxdiff;
								return 1004.f;
							}
						}

						// If difference is too high, return false
						if(l_diff > maxdiff)
						{
							maxdiff = l_diff;
							if(l_diff > difftolerance) { // Abort correlation
								if(pnbdiff)
									*pnbdiff = nbdiff;
								if(pmaxdiff)
									*pmaxdiff = maxdiff;
								return l_diff;
							}
						}

						diff += l_diff;
						nbpix++;

						img1pos++;
						img2pos++;
					}

				} else { // increase by one pixel wide
					img1pos += channels;
					img2pos += channels;
				}

				maskpos++;
			}
		}
		}break;
	case IPL_DEPTH_16U:
		fprintf(logfile, "[imgutils] %s:%d : unsupported depth IPL_DEPTH_16U \n", __func__, __LINE__);
		fprintf(stderr, "[imgutils]	%s:%d : OBSOLETE : correlation between IPL_DEPTH_16U\n",
			__func__, __LINE__);

		// Image buffers
		u16 * img1buffer = (u16 *)img1->imageData;
		u16 * img2buffer = (u16 *)img2->imageData;
		u8 * maskbuffer = (u8 *)maskImage->imageData;
		//u8 * diffbuffer = (u8 *)diffImage->imageData;

		// Do a distance correlation between the 2 images
		for(int dy = 0; dy<h; dy++) {
			u16 * img1pos = (u16 *) ((u8 *)img1buffer + (y1+dy)*pitch1   	+ x1_x_depth);
			u8 * maskpos = maskbuffer + (y1+dy)*pitchmask	+ x1;

			//u8 * diffpos = diffbuffer + (y1+dy)*pitchmask	+ x1;
			u16 * img2pos = (u16 *) ((u8 *)img2buffer + (y2+dy)*pitch2		+ x2_x_depth);

			for(int dx=0; dx<w; dx++) {
				// Don't compute if there is a doubt about this image (dust presence)
				//if(*diffpos >= 127)
				//	return 5000;

				// Compute only if mask is 0
				if(*maskpos == 0) {
					for(int d=0; d<channels; d++) {
						long l_diff = abs( (long)*img1pos - (long)*img2pos);
						if(l_diff > VISIBLE_DIFF) {
							nbdiff++;
							if(nbdiff>wxh_4) {// Too many pixels different / surface of object
								if(pnbdiff)
									*pnbdiff = nbdiff;
								if(pmaxdiff)
									*pmaxdiff = maxdiff;
								return 1005.f;
							}
						}
						if(l_diff > maxdiff)
						{
							maxdiff = l_diff;
							if(l_diff > difftolerance) // Abort correlation
							{
								if(pnbdiff)
									*pnbdiff = nbdiff;
								if(pmaxdiff)
									*pmaxdiff = maxdiff;
								return l_diff;
							}
						}
						diff += l_diff; nbpix++;
						img1pos++;
						img2pos++;
					}

				} else {
					img1pos += channels;
					img2pos += channels;
				}

				img1pos++;
				img2pos++;
				//diffpos++;
				maskpos++;
			}
		}
		break;
	}

	// Return worst case
	if(pmaxdiff)
		*pmaxdiff = maxdiff;
	if(pnbdiff)
		*pnbdiff = nbdiff;
	if(nbpix == 0)
		return 1006.f;

	if(nbdiff > nbpix/4) {
		// Too many different pixels / surface of search rectangle
		return maxdiff;
	}

	if(nbdiff > (w*h-nbpix)/2) {
		// Too many different pixels / surface of dust
		return maxdiff;
	}


	// Return best
	return (float)((double)diff / (double)nbpix);
}


u8 g_debug_correlation = 0;
IplImage * correlationImage = NULL;

IplImage * getCorrelationImage() { return correlationImage; };

/** Find a neighbour region to copy on the dust */
int tmSearchBestCorrelation(
	IplImage * origImage, IplImage * maskImage,
	int seed_center_x, int seed_center_y,
	int seed_width, int seed_height,

	// Output
	int * copy_dest_x, int * copy_dest_y,
	int * copy_src_x, int * copy_src_y,
	int * copy_width, int * copy_height,
	int * best_correl
	) {

	int retval = 0;

	if(g_debug_correlation) {
		if(correlationImage) {
			if(origImage->width != correlationImage->width
				|| origImage->height != correlationImage->height) {
				cvReleaseImage(&correlationImage); correlationImage = NULL;
			}
		}
		if(!correlationImage) {
			fprintf(stderr, "[imgutils] %s:%d : allocating correlationImage(%d x %d)...\n",
				__func__, __LINE__, origImage->width, origImage->height);

			correlationImage = cvCreateImage(cvSize(origImage->width, origImage->height), IPL_DEPTH_8U, 1);
		}
		memset(correlationImage->imageData, 0xff, correlationImage->widthStep * correlationImage->height);
	}


	// Global search windows around the dust position (input)

	// Size of the correlation rectangle
/* bon resultats mais pas mal de regions pas trouvees
	int correl_width  = 3 * seed_width;
	int correl_height = 3 * seed_height;

	int search_width  = 5 * correl_width;
	int search_height = 5 * correl_height;
	*/

	// Try out with only 2x size (half size on both side)
	int correl_width  = seed_width;
	int correl_height = seed_height;

	// Try out with only 5/2x size (half size on both side)
//	int correl_width  = 5 * seed_width/2;
//	int correl_height = 5 * seed_height/2;


	// Wa use top-left for correlation (fewer computations in tmCorrelation)
	int seed_x = seed_center_x - correl_width; if(seed_x<0) seed_x = 0;
	int seed_y = seed_center_y - correl_height;if(seed_y<0) seed_y = 0;

	correl_width *= 2;
	correl_height *= 2;

	int search_width  = 3 * correl_width;
	int search_height = 3 * correl_height;


	// Best offset
	int best_offset_x = 0;
	int best_offset_y = 0;

	float least_worst  = 255.f;
	float best_dist = 9.f;
	float best_best = 1.f;

	float depth_coef = 1.f;
	switch(origImage->depth) {
	case IPL_DEPTH_8U:
		break;
	case IPL_DEPTH_8S:
		break;
	case IPL_DEPTH_16U:
		depth_coef = 256.f;
		break;
	case IPL_DEPTH_16S:
		depth_coef = 128.f;
		break;
	case IPL_DEPTH_32S:
		break;
	case IPL_DEPTH_32F:
		break;
	case IPL_DEPTH_64F:
		break;
	default:
		break;
	}
	best_dist *= depth_coef;
	best_best *= depth_coef;
	least_worst *= depth_coef;

//Do not work with image with grain : 	int difftolerance = (int)best_dist;
	int difftolerance = (int)(depth_coef*100.f);
	//int difftolerance = (int)((*best_correl)*depth_coef);

	best_dist = (*best_correl) * depth_coef + 1;

	// Try with fix value, not depending on input best_correl
	best_dist = tmmin(40.f, tmmax(9.f,  (float)(*best_correl))) * depth_coef;

	for(int dy = -search_height; dy < search_height; dy++) {
		if(dy+seed_y > 0 && dy+seed_y+correl_height<origImage->height) {
			for(int dx = -search_width; dx < search_width; dx++) {
				if(dx+seed_x > 0 && dx+seed_x+correl_width<origImage->width)
				// We look only if the regions cannot touch
				if(dy < -correl_height || dy >= correl_height
					|| dx < -correl_width || dx >= correl_width) {

					int maxdiff = 0, nbdiff = 0;
					float l_dist = tmCorrelation(origImage, origImage,
							maskImage,
							seed_x, seed_y,
							seed_x + dx, seed_y + dy,
							correl_width, correl_height,
							difftolerance,
							&maxdiff,
							&nbdiff
							);

					if(l_dist < least_worst)
						least_worst = l_dist;

					if(g_debug_correlation) {
						fprintf(logfile, "\timgutils %s:%d : (+%d,+%d) = %f / difftol=%d maxdiff=%d bestbest=%g\n",
								__func__, __LINE__,	dx, dy, l_dist, difftolerance, maxdiff, best_best);
						u8 * correlationImageBuffer = (u8 *)correlationImage->imageData;
						correlationImageBuffer[ correlationImage->widthStep * (seed_center_y + dy)
												+ seed_center_x + dx] = (u8)l_dist;
					}


					if(l_dist < best_dist) {

						best_dist = l_dist;
						if(maxdiff < difftolerance)
							difftolerance = maxdiff;

						best_offset_x = dx;
						best_offset_y = dy;


						retval = 1;

						if(g_debug_imgverbose || g_debug_correlation)
							fprintf(logfile, "\timgutils %s:%d : current best = "
								"(+%d,+%d) = %f / %d  (%dx%d) bestbest=%g\n",
								__func__, __LINE__,
								dx, dy, l_dist, difftolerance,
								correl_width, correl_height,
								best_best
								);

						// Top quality correlation : exist asap
						if(l_dist < best_best) {
							*copy_dest_x = seed_center_x;
							*copy_dest_y = seed_center_y;
							*copy_src_x = seed_center_x + best_offset_x;
							*copy_src_y = seed_center_y + best_offset_y;
							*copy_width = correl_width;
							*copy_height = correl_height;

							if(best_correl) {
								*best_correl = (int)least_worst;
							}
							return 1;
						}
					}
				}
			}
		}
	}

	if(retval) {
		*copy_dest_x = seed_center_x;
		*copy_dest_y = seed_center_y;
		*copy_src_x = seed_center_x + best_offset_x;
		*copy_src_y = seed_center_y + best_offset_y;
		*copy_width = correl_width;
		*copy_height = correl_height;
	}

	if(best_correl)
		*best_correl = (int)least_worst;
	return retval;
}





/*
 * Difference between 2 images, knowning film type (positive or negative or undefined)
 */
int tmProcessDiff(int l_FilmType, IplImage * grayImage,  IplImage * medianImage,
				IplImage * diffImage, IplImage * varianceImage,
				unsigned long * diffHisto,
				int //unused size
				)
{
	unsigned char * grayImageBuffer = (unsigned char *)grayImage->imageData;
	unsigned char * blurImageBuffer = (unsigned char *)medianImage->imageData;
	unsigned char * diffImageBuffer = (unsigned char *)diffImage->imageData;
	static unsigned long l_diffHisto[256];
	if(!diffHisto) {
		diffHisto = l_diffHisto;
	}
	memset(diffHisto, 0, sizeof(unsigned long)*256);
	memset(diffImage->imageData, 0, diffImage->widthStep*diffImage->height);

	int width = medianImage->widthStep;
	int height = medianImage->height;
//unused	int w= medianImage->width;

#if (0)
	for(int r=0; r<grayImage->height; r++) {
		int pos = grayImage->widthStep * r;
		int posmax = pos + grayImage->widthStep;
		float * var = (float *)(varianceImage->imageData + r*varianceImage->widthStep);
		for(int c=0 ; pos<posmax; pos++, c++, var++)
		{
			// compute variance
			float sum_mean = 0.;
			float sum_val2 = 0.;
			int sum_nb = 0;
			for(int l_r = tmmax(0, r-size); l_r<tmmin(height, r+size); l_r++) {
				int l_pos_min = tmmax(0, c-size) + l_r*width;
				int l_pos_max = tmmin(w, c+size) + l_r*width;

				for(int l_pos = l_pos_min; l_pos < l_pos_max; l_pos++) {
					sum_val2 += (float)grayImageBuffer[l_pos]*(float)grayImageBuffer[l_pos];
					sum_mean += (float)grayImageBuffer[l_pos];
					sum_nb++;
				}
			}

			if(sum_nb>0) {
				float mean = (sum_mean/(float)sum_nb);
				*var = ( sum_val2/(float)sum_nb - mean*mean);
			}

		}
	}
#endif

	switch(l_FilmType) {
	default:
	case FILM_UNDEFINED: { /* dust can be darker or brighter => absolute value */
		//fprintf(logfile, "[imgutils] %s:%d : Undefined film type : looking for diffImage = |Blurred - Grayscaled| ...\n",
		//	__func__, __LINE__);
		for(int pos=0; pos<width*height; pos++)
		{
			//if(grayImageBuffer[pos] > blurImageBuffer[pos])
			{
				u8 diff = diffImageBuffer[pos] = abs((long)grayImageBuffer[pos] - (long)blurImageBuffer[pos]);
				diffHisto[diff]++;
			}
		}
		}break;
	case FILM_NEGATIVE: /* dust must be brighter => gray - median */
		//fprintf(logfile, "[imgutils] %s:%d : NEGATIVE film type : looking for diffImage = Grayscaled-Blurred ...\n",
		//	__func__, __LINE__);
		for(int pos=0; pos<width*height; pos++)
		{
			if(grayImageBuffer[pos] > blurImageBuffer[pos])
			{
				u8 diff = diffImageBuffer[pos] = grayImageBuffer[pos] - blurImageBuffer[pos];
				diffHisto[diff]++;
			}
		}
		break;
	case FILM_POSITIVE: /* dust must be darker => median - gray */
		//fprintf(logfile, "[imgutils] %s:%d : POSITIVE film type : looking for diffImage = Blurred-Grayscaled ...\n",
		//	__func__, __LINE__);
		for(int pos=0; pos<width*height; pos++)
		{
			if(grayImageBuffer[pos] < blurImageBuffer[pos])
			{
				u8 diff = diffImageBuffer[pos] = blurImageBuffer[pos] - grayImageBuffer[pos];
				diffHisto[diff]++;
			}
		}
		break;
	}
	return 0;
}


/*
 * Difference between 2 images, knowning film type (positive or negative or undefined)
 */
int tmProcessDilate_Erode(int l_FilmType,
						  IplImage * dilateImage, IplImage * erodeImage,
						  IplImage * diffImage,
						  unsigned long * diffHisto, int size)
{
	static unsigned long l_diffHisto[256];
	if(!diffHisto) {
		diffHisto = l_diffHisto;
	}
	memset(diffHisto, 0, sizeof(unsigned long)*256);

	int width = dilateImage->width;
	int height = dilateImage->height;

	cvZero(diffImage);

	switch(l_FilmType)
	{
	default:
	case FILM_UNDEFINED:  /* dust can be darker or brighter => absolute value */
		 //fprintf(logfile, "[imgutils] %s:%d : Undefined film type : looking for diffImage = |Blurred - Grayscaled| ...\n",
		 //	__func__, __LINE__);
		 for(int r=0; r<height; r++) {
			 u8 * diffed = (u8 *)diffImage->imageData + r * diffImage->widthStep;
			 u8 * dilated = (u8 *)dilateImage->imageData + r * dilateImage->widthStep;
			 u8 * eroded = (u8 *)erodeImage->imageData + r * erodeImage->widthStep;
			 for(int c=0; c<width; c++) {
				 u8 diff = diffed[c] = abs((int)dilated[c] - (int)eroded[c]);
				 diffHisto[ diff ]++;
			 }
		}
		break;
	case FILM_NEGATIVE: /* dust must be brighter => gray - median */
		//fprintf(logfile, "[imgutils] %s:%d : NEGATIVE film type : looking for diffImage = Dilated-Eroded ...\n",
		//	__func__, __LINE__);
		for(int r=0; r<height; r++) {
			u8 * diffed = (u8 *)diffImage->imageData + r * diffImage->widthStep;
			u8 * dilated = (u8 *)dilateImage->imageData + r * dilateImage->widthStep;
			u8 * eroded = (u8 *)erodeImage->imageData + r * erodeImage->widthStep;
			for(int c=0; c<width; c++) {
				if(dilated[c] > eroded[c]) {
					u8 diff = diffed[c] = (dilated[c] - eroded[c]);
					diffHisto[diff]++;
				}
			}
		}
		break;
	case FILM_POSITIVE: /* dust must be darker => median - gray */
		//fprintf(logfile, "[imgutils] %s:%d : POSITIVE film type : looking for diffImage = Blurred-Grayscaled ...\n",
		//	__func__, __LINE__);
		for(int r=0; r<height; r++) {
			u8 * diffed = (u8 *)diffImage->imageData + r * diffImage->widthStep;
			u8 * dilated = (u8 *)dilateImage->imageData + r * dilateImage->widthStep;
			u8 * eroded = (u8 *)erodeImage->imageData + r * erodeImage->widthStep;
			for(int c=0; c<width; c++) {
				if(1 || dilated[c] > eroded[c]) {
					u8 diff = diffed[c] = (dilated[c] - eroded[c]);
					diffHisto[diff]++;
				}
			}
		}
		break;
	}
	return 0;
}




/*
 * Grow a region from one seed
 */


#define spmax 576*2

void tmGrowRegion(IplImage * growInImage,
				  IplImage * growOutImage,
				  int c, int r,
				  unsigned char threshold,
				  unsigned char fillValue,
				  CvConnectedComp * areaOut)
{
	memset(areaOut, 0, sizeof(CvConnectedComp));
	if(growInImage->depth != IPL_DEPTH_8U
	   || growInImage->nChannels !=1 ) {
		return;
	}

	int pile_x[spmax];
	int pile_y[spmax];

	unsigned char * growIn = (u8 *)growInImage->imageData;
	unsigned char * growOut = (u8 *)growOutImage->imageData;
	int swidth = growInImage->widthStep;
	int sheight = growInImage->height;

	int x,y,xi,xf;

	// init stack
	int pile_sp = 0;
	pile_x[0] = c;
	pile_y[0] = r;

	areaOut->area = 0.f; // for current pix

	if(c<0 || c>=swidth) return;
	if(r<0 || r>=sheight) return;

	int surf = 1;
	int rmin = 0;
	int rmax = sheight-1;
	int cmin = 0;
	int cmax = swidth-1;
	// If image has ROI, use this ROI
	if(growInImage->roi) {
		cmin = growInImage->roi->xOffset;
		if(cmin < 0) cmin=0;
		rmin = growInImage->roi->yOffset;
		if(rmin < 0) rmin=0;
		cmax = growInImage->roi->xOffset + growInImage->roi->width-1;
		if(cmax >= swidth) cmax = swidth-1;
		rmax = growInImage->roi->yOffset + growInImage->roi->height-1;
		if(rmax >= sheight) rmax = sheight-1;

//		fprintf(stderr, "[imgutils] %s:%d : img ROI=%d,%d+%dx%d => use ROI:%d,%d - %d,%d\n",
//				__func__, __LINE__,
//				growInImage->roi->xOffset, growInImage->roi->yOffset,
//				growInImage->roi->width, growInImage->roi->height,
//				cmin, rmin, cmax, rmax);
	}

	if(fillValue==0)
		fillValue=1;


	// reinit growXMin, ...
	int growXMin = c;
	int growXMax = c;
	int growYMin = r;
	int growYMax = r;

	if(growIn[c+r * swidth] < threshold)
		return;

	while(pile_sp != -1)
	{
		// determinate extreme abscisses xi and xf
		y = pile_y[pile_sp];
		int row = y * swidth;

		//looking for right extremity
		x = pile_x[pile_sp]+1;


		if(x<=cmax) {
			while( growOut[x+row]==0 &&
				   growIn[x+row]>=threshold && x<cmax) {
				x++;
			}
			xf = x-1;
		}
		else
			xf = cmax;

		// idem for left
		x = pile_x[pile_sp]-1;
		// we look for new seed
		pile_sp --;

		if(x>cmin) {
			while( growOut[x+row]==0 &&
				   growIn[x+row]>=threshold && x>cmin) {
				x--;
			}
			xi = x+1;
		}
		else
			xi = cmin;

		// reset the line
		int w = xf - xi + 1;
		if(w>0) {
			memset(growOut + row+xi, fillValue, w);
			surf += w;

			if(xi<growXMin) growXMin = xi;
			if(xf>growXMax) growXMax = xf;
			if(y<growYMin) growYMin = y;
			if(y>growYMax) growYMax = y;



			//#define CON_8
			// line under current seed
			if( y < rmax -1) {
				if(xf < cmax - 1)
					x = xf + 1; // 8con
				else
					x = xf;
				if(xi <= 0) xi = 1;
				int row2 = row + swidth;

				while(x>=xi-1) {
					while( (growOut[x+row2]>0 || growIn[x+row2]<threshold)
						&& (x>=xi-1)) 	x--; // 8-connexity
					if( (x>=xi-1) && growOut[x+row2]==0 && growIn[x+row2]>=threshold) {
						if(pile_sp < spmax-1)
						{
							pile_sp++;
							pile_x[pile_sp] = x;
							pile_y[pile_sp] = y+1;
						}
					}
					while( growOut[x+row2]==0 && growIn[x+row2]>=threshold && (x>=xi-1)) // 8-con
						x--;
				}
			}

			// line above current line
			if( y > rmin) {
				if(xf < cmax - 1)
					x = xf + 1; // 8con
				else
					x = xf;
				if(xi <= 0) xi = 1;
				int row3 = row - swidth;

				while(x>=xi-1) { // 8-con
					while( (growOut[x+row3]>0 || growIn[x+row3]<threshold)
						&& (x>=xi-1)) x--;
					if( (x>=xi-1) && growOut[x+row3]==0 && (growIn[x+row3]>=threshold)) {
						if(pile_sp < spmax-1)
						{
							pile_sp++;
							pile_x[pile_sp] = x;
							pile_y[pile_sp] = y-1;
						}
					}

					while( growOut[x+row3]==0 && (growIn[x+row3]>=threshold)
						&& (x>=xi-1)) // 8-con
						x--;
				}
			}
		}
	}

	// conclusion
	areaOut->rect.x = growXMin;
	areaOut->rect.y = growYMin;
	areaOut->rect.width = (growXMax - growXMin+1);
	areaOut->rect.height = (growYMax - growYMin+1);

	areaOut->area = (double)surf;
}



void tmFloodRegion(IplImage * growInImage,
				   IplImage * growOutImage,
				   int c, int r,
				   unsigned char seedValue,
				   unsigned char threshold,
				   unsigned char fillValue,
				   CvConnectedComp * areaOut)
{
	memset(areaOut, 0, sizeof(CvConnectedComp));
	if(growInImage->depth != IPL_DEPTH_8U
	   || growInImage->nChannels !=1 ) {
		return;
	}
	int pile_x[spmax];
	int pile_y[spmax];

	unsigned char * growIn = (u8 *)growInImage->imageData;
	unsigned char * growOut = (u8 *)growOutImage->imageData;
	int swidth = growInImage->widthStep;
	int sheight = growInImage->height;
	int x,y,xi,xf;

	// init stack
	int pile_sp = 0;
	pile_x[0] = c;
	pile_y[0] = r;

	memset(areaOut, 0, sizeof(CvConnectedComp));
	areaOut->area = 0.f; // for current pix

	if(c<0 || c>=swidth) return;
	if(r<0 || r>=sheight) return;

	int surf = 1;
	int rmin = 0;
	int rmax = sheight-1;
	int cmin = 0;
	int cmax = swidth-1;

	// If image has ROI, use this ROI
	if(growInImage->roi) {
		cmin = growInImage->roi->xOffset;
		if(cmin < 0) cmin=0;
		rmin = growInImage->roi->yOffset;
		if(rmin < 0) rmin=0;
		cmax = growInImage->roi->xOffset + growInImage->roi->width-1;
		if(cmax >= swidth) cmax = swidth - 1;
		rmax = growInImage->roi->yOffset + growInImage->roi->height-1;
		if(rmax >= sheight) rmax = sheight-1;

//		fprintf(stderr, "[imgutils] %s:%d : img ROI=%d,%d+%dx%d "
//				"=> use ROI:%d,%d - %d,%d\n",
//				__func__, __LINE__,
//				growInImage->roi->xOffset, growInImage->roi->yOffset,
//				growInImage->roi->width, growInImage->roi->height,
//				cmin, rmin, cmax, rmax);
	}

	if(fillValue==0) {
		fillValue=1;
	}

	// reinit growXMin, ...
	int growXMin = c;
	int growXMax = c;
	int growYMin = r;
	int growYMax = r;

	while(pile_sp != -1)
	{
		// determinate extreme abscisses xi and xf
		y = pile_y[pile_sp];
		int row = y * swidth;

		//looking for right extremity
		x = pile_x[pile_sp]+1;

		if(x<=cmax) {
			while( growOut[x+row]==0 &&
				abs((int)growIn[x+row] - (int)seedValue) <= threshold && x<cmax) {
				x++;
			}
			xf = x-1;
		}
		else
			xf = cmax;

		// idem for left
		x = pile_x[pile_sp]-1;
		// we look for new seed
		pile_sp --;

		if(x>cmin) {
			while( growOut[x+row]==0 &&
				abs((int)growIn[x+row] - (int)seedValue)<=threshold && x>cmin) {
				x--;
			}
			xi = x+1;
		} else {
			xi = cmin;
		}

		// reset the line
		int w = xf - xi + 1;
		if(w > 0) {
			memset(growOut + row+xi, fillValue, w);
			surf += w;

			if(xi<growXMin) growXMin = xi;
			if(xf>growXMax) growXMax = xf;
			if(y<growYMin) growYMin = y;
			if(y>growYMax) growYMax = y;



	//#define CON_8
			// line under current seed
			if( y < rmax -1) {
				if(xf < cmax - 1) {
					x = xf + 1; // 8con
				} else {
					x = xf;
				}

				if(xi <= 0) { xi = 1; }
				int row2 = row + swidth;

				while(x>=xi-1) {
					while( (growOut[x+row2]>0
								|| abs((int)growIn[x+row2]-seedValue)>threshold)
							&& (x>=xi-1)) {	// 8-connexity
							x--;
					}

					if( (x>=xi-1) && growOut[x+row2]==0 && abs((int)growIn[x+row2]-seedValue)<=threshold) {
						if(pile_sp < spmax-1)
						{
							pile_sp++;
							pile_x[pile_sp] = x;
							pile_y[pile_sp] = y+1;
						}
					}

					while( growOut[x+row2]==0
						   && abs((int)growIn[x+row2]-seedValue) <= threshold
						   && (x>=xi-1))
					{	// 8-con
						x--;
					}
				}
			}

			// line above current line
			if( y > rmin) {
				if(xf < cmax - 1)
					x = xf + 1; // 8con
				else
					x = xf;
				if(xi <= 0) xi = 1;
				int row3 = row - swidth;

				while(x>=xi-1) { // 8-con
					while( (growOut[x+row3]>0
								|| abs((int)growIn[x+row3]-seedValue)>threshold)
							&& (x>=xi-1)) { x--;
					}
					if( (x>=xi-1) && growOut[x+row3]==0 && (abs((int)growIn[x+row3]-seedValue)<=threshold)) {
						if(pile_sp < spmax-1)
						{
							pile_sp++;
							pile_x[pile_sp] = x;
							pile_y[pile_sp] = y-1;
						}
					}

					while( growOut[x+row3]==0 && (abs((int)growIn[x+row3]-seedValue)<=threshold)
							&& (x>=xi-1) ) { // 8-con
						x--;
					}
				}
			}
		}
	}

	// conclusion
	areaOut->rect.x = growXMin;
	areaOut->rect.y = growYMin;
	areaOut->rect.width = (growXMax - growXMin+1);
	areaOut->rect.height = (growYMax - growYMin+1);

	areaOut->area = (double)surf;
}



/* Erase a region filled with @param fillValue in @param grownImage,
 *	and neutralize diffImage at the same place
 */
void tmEraseRegion(
	IplImage * grownImage,
	IplImage * diffImage,
	int c, int r,
	unsigned char fillValue_grown,
	unsigned char fillValue_diff)
{
	int pile_x[spmax];
	int pile_y[spmax];
	int swidth = grownImage->widthStep;
	int sheight = grownImage->height;

	int x,y,xi,xf;

	// init stack
	int pile_sp = 0;
	pile_x[0] = c;
	pile_y[0] = r;


	if(c<0 || c>=swidth) return;
	if(r<0 || r>=sheight) return;
	// Clip image search
	int surf = 1;
	int rmin = 0;
	int rmax = sheight-1;
	int cmin = 0;
	int cmax = grownImage->width-1;
	// If image has ROI, use this ROI
	if(grownImage->roi) {
		cmin = grownImage->roi->xOffset;
		if(cmin < 0) cmin=0;
		rmin = grownImage->roi->yOffset;
		if(rmin < 0) rmin=0;
		cmax = grownImage->roi->xOffset + grownImage->roi->width-1;
		if(cmax >= swidth) cmax = swidth - 1;
		rmax = grownImage->roi->yOffset + grownImage->roi->height-1;
		if(rmax >= sheight) rmax = sheight-1;

//		fprintf(stderr, "[imgutils] %s:%d : img ROI=%d,%d+%dx%d "
//				"=> use ROI:%d,%d - %d,%d\n",
//				__func__, __LINE__,
//				growInImage->roi->xOffset, growInImage->roi->yOffset,
//				growInImage->roi->width, growInImage->roi->height,
//				cmin, rmin, cmax, rmax);
	}

//	if(fillValue_grown==0)
//		fillValue_grown=1;


	// reinit growXMin, ...
	u8 * growIn = (u8 *)grownImage->imageData;
	u8 * diffOut = (diffImage ? (u8 *)diffImage->imageData : NULL);

	if(growIn[c+r * swidth] != fillValue_grown) {
		/*
		fprintf(stderr, "[imgutils] %s:%d : not a seed @ %d,%d\n", __func__, __LINE__,
				c, r);
				*/
		return;
	}

	while(pile_sp != -1)
	{
		// determinate extreme abscisses xi and xf
		y = pile_y[pile_sp];
		int row = y * swidth;

		//looking for right extremity
		x = pile_x[pile_sp]+1;

		if(x<=cmax) {
			while(growIn[x+row]== fillValue_grown && x<cmax) {
				x++;
			}
			xf = x-1;
		}
		else
			xf = cmax;

		// idem for left
		x = pile_x[pile_sp]-1;
		// we look for new seed
		pile_sp --;

		if(x>cmin) {
			while( growIn[x+row]== fillValue_grown && x>cmin) {
				x--;
			}
			xi = x+1;
		}
		else
			xi = cmin;


		// reset the line
		int w = xf - xi + 1;
		if(w>0) {
			memset(growIn + row+xi, 0, w);
			// And neutralize diffImage
			if(diffOut) {
				memset(diffOut + row+xi, fillValue_diff, w); }

			surf += w;
			/*fprintf(stderr, "\t%s:%d : clearing %d,%d + %d\n",
					__func__, __LINE__,
					xi, y, w);
			*/


			// line under current seed
			if( y < rmax -1) {
				if(xf < cmax - 1)
					x = xf + 1; // 8con
				else
					x = xf;
				if(xi <= 0) xi = 1;
				int row2 = row + swidth;

				while(x>=xi-1) {
					while( (growIn[x+row2] != fillValue_grown)
							&& (x>=xi-1)) 	x--; // 8-connexity
					if( (x>=xi-1) && growIn[x+row2]== fillValue_grown) {
						if(pile_sp < spmax-1)
						{
							pile_sp++;
							pile_x[pile_sp] = x;
							pile_y[pile_sp] = y+1;
						}
					}
					while( growIn[x+row2]== fillValue_grown && (x>=xi-1)) // 8-con
						x--;
				}
			}

			// line above current line
			if( y > rmin) {
				if(xf < cmax - 1)
					x = xf + 1; // 8con
				else
					x = xf;
				if(xi <= 0) xi = 1;
				int row3 = row - swidth;

				while(x>xi) { // 8-con
					while( (growIn[x+row3]!= fillValue_grown)
							&& (x>=xi-1)) x--;
					if( (x>=xi-1) &&  (growIn[x+row3]== fillValue_grown)) {
						if(pile_sp < spmax-1)
						{
							pile_sp++;
							pile_x[pile_sp] = x;
							pile_y[pile_sp] = y-1;
						}
					}

					while( growIn[x+row3]== fillValue_grown
							&& (x>=xi-1)) // 8-con
						x--;
				}
			}
		}
	}
}




// u16 May be too short for large images
typedef u32 etiquette;
static u8 growAllRegions_init = 0;
static u8 * s_ConditionBuf = NULL, * s_RegionLabel = NULL;
static etiquette * s_eqTab = NULL;
static int s_ConditionBufSize = 0;
static CvConnectedComp * s_GrownRegions = NULL;
static int s_GrownRegions_nb = 0;
static int s_GrownRegions_max = 0;

// insert equivalence between 2 labels in the table of equivalences
void tmInsertGrowEq(etiquette * eqTab, etiquette minLabel, etiquette maxLabel) {
	if(eqTab[minLabel]==0) {
		eqTab[minLabel]=maxLabel;
		return;
	}

	if(eqTab[minLabel]<maxLabel) {
		tmInsertGrowEq(eqTab, eqTab[minLabel], maxLabel);
		eqTab[minLabel] = maxLabel;
		return;
	}

	if(eqTab[minLabel]>maxLabel) {
		tmInsertGrowEq(eqTab, maxLabel, eqTab[minLabel]);
	}
}


/*
 * Append a region in "grow all" regions list
 */
int tmAddRegion(CvConnectedComp connect) {
	if(s_GrownRegions_nb >= s_GrownRegions_max) {
		// We need to increase the size of buffer
		s_GrownRegions_max += 100;
		CvConnectedComp * oldRegions = s_GrownRegions;
		CvConnectedComp * newRegions = new CvConnectedComp [ s_GrownRegions_max ];
		if(!newRegions) {
			fprintf(stderr, "[imgutils] %s:%d : cannot allocate more regions : keep %d regions max\n",
					__func__, __LINE__, s_GrownRegions_nb);
			return -1;
		}

		if(s_GrownRegions_nb > 0 && s_GrownRegions) {
			memcpy(newRegions, s_GrownRegions, s_GrownRegions_nb*sizeof(CvConnectedComp));
		}
		memset(newRegions + s_GrownRegions_nb, 0, (s_GrownRegions_max-s_GrownRegions_nb)*sizeof(CvConnectedComp));

		s_GrownRegions = newRegions;
		if(oldRegions) {
			delete [] oldRegions;
		}
	}

	// Append new region
	memcpy(s_GrownRegions+s_GrownRegions_nb, &connect, sizeof(CvConnectedComp));
	s_GrownRegions_nb++;

	return 0;
}

/*
 * Grow all regions in an image using a 3-N algorithm
 */
int tmGrowAllRegions(IplImage * inputImage, IplImage * growImage,
				   u8 threshold,
				   u8 fillvalue
				   )
{
	int swidth = inputImage->width, sheight = inputImage->height;
	int rmin = (int)0;
	int rmax = (int)sheight;
	int cmin = (int)0;
	int cmax = (int)swidth;
	if(inputImage->roi) {
		rmin = (int)inputImage->roi->yOffset;
		rmax = (int)(inputImage->roi->yOffset+inputImage->roi->height);
		cmin = (int)inputImage->roi->xOffset;
		cmax = (int)(inputImage->roi->xOffset+inputImage->roi->width);
	}

	u8 growImage_local = 0;
	if(!growImage) {
		growImage_local = 1;
		growImage = tmCreateImage(cvSize(inputImage->width, inputImage->height),
								  IPL_DEPTH_8U, 1);
	} else { // clear buffer
		cvZero(growImage);
	}

	// Allocate region growing if not allocated
	if(!s_ConditionBuf || swidth*sheight>s_ConditionBufSize) {
		if(s_ConditionBuf) delete [] s_ConditionBuf;
		if(s_RegionLabel) delete [] s_RegionLabel;

		s_ConditionBufSize = swidth*sheight;

		fprintf(stderr, "[imgutils]::%s:%d \tALLOCATING BUFFERS [%d]!\n", __func__, __LINE__, s_ConditionBufSize);
		s_ConditionBuf = new u8 [s_ConditionBufSize];
		memset(s_ConditionBuf, 0, sizeof(u8)*s_ConditionBufSize);
		s_RegionLabel = new u8 [s_ConditionBufSize];
		memset(s_RegionLabel, 0, sizeof(u8)*s_ConditionBufSize);
		fprintf(stderr, "[imgutils]::%s:%d \tALLOCATING BUFFERS DONE [%d]!\n", __func__, __LINE__,
				s_ConditionBufSize); fflush(stderr);
	}
#define EQTAB_LENGTH	4000
	// Clear table of equivalence
	if(!s_eqTab) {
		s_eqTab = new etiquette [EQTAB_LENGTH];
	}
	memset(s_eqTab, 0, sizeof(etiquette) * EQTAB_LENGTH);
	u16 currentEtq = 1;
	u16 topLabel = 1;

	int pos=0, r, c;
	// Create grow condition buffer and first pass
	for(r=rmin;r<rmax; r++) {
		pos = cmin + r*swidth; //position in arrays
		u8 * inputline = IPLLINE_8U(inputImage, r);
		for(c=cmin; c<cmax; c++, pos++)
		{
			unsigned char curCond = s_ConditionBuf[pos] =
					(inputline[c] >= threshold ? 255 : 0);

			int nbsame=0; // nb of pixels with same value
			// check past pixels
			unsigned char rminus = 2;
			unsigned short etqrminus = 0, etqcminus = 0;
			if(r>rmin) {
				rminus = s_ConditionBuf[pos-swidth];
				etqrminus = s_RegionLabel[pos-swidth];
				if(curCond == rminus) { // Same label on the pixel above
					nbsame++;
				}
			}

			unsigned char cminus = 2;
			if(c>cmin) {
				cminus = s_ConditionBuf[pos-1];
				etqcminus = s_RegionLabel[pos-1];
				if(curCond == cminus) { // Same label on pixel on left
					nbsame++;
				}
			}

			// labelling
			if(nbsame == 0) { // We changed condition => change label
				s_RegionLabel[pos] = currentEtq;
				currentEtq++;
			} else {
				if(nbsame == 1) { // Top and left aren't equal but both are the same condititon => use minimum label
					if(curCond == cminus) {
						s_RegionLabel[pos] = etqcminus;
					} else {
						s_RegionLabel[pos] = etqrminus;
					}
				} else { // nbsame==2
					if(etqcminus == etqrminus) {
						s_RegionLabel[pos] = etqrminus;
					} else {
						unsigned short minLabel, maxLabel;
						if(etqcminus < etqrminus) {
							minLabel = etqcminus;
							maxLabel = etqrminus;
						} else {
							maxLabel = etqcminus;
							minLabel = etqrminus;
						}
						s_RegionLabel[pos] = maxLabel;

						if(s_eqTab[minLabel]!=maxLabel) {
							tmInsertGrowEq(s_eqTab, minLabel, maxLabel);
						}// TO BE CONTINUED

						s_RegionLabel[pos] = s_eqTab[minLabel];
						if(maxLabel>topLabel) {
							topLabel = maxLabel;
							if(topLabel>65000)
							{
								fprintf(stderr,"[imgutils] %s:%d TopLabel overflow !!\n",
										__func__, __LINE__);
							}
						}
					}
				}
			}

			if(s_RegionLabel[pos] == 0) {
				fprintf(stderr,"[imgutils] %s:%d : (%d,%d) : 0 ! (nbsame=%d, etqcminus=%u etqrminus=%u currentEtq=%u)\n",
					__func__, __LINE__,
					c, r, nbsame, etqcminus, etqrminus, currentEtq
					);
				return -1;
			}
		}
	}

	for(etiquette t=topLabel; t>0; t--) {
		if(s_eqTab[t]) {
			etiquette eq0 = t, eq1 = s_eqTab[t];
			while(s_eqTab[eq1] > eq1) {
				s_eqTab[eq0] = s_eqTab[eq1];
				eq1 = s_eqTab[eq1];
			}
		} else {
			s_eqTab[t] = t;
		}
	}

	// Labelling
	pos = 0;
	unsigned short xMin[EQTAB_LENGTH];
	unsigned short yMin[EQTAB_LENGTH];
	unsigned short xMax[EQTAB_LENGTH];
	unsigned short yMax[EQTAB_LENGTH];
	unsigned short growAll_surf[EQTAB_LENGTH];
	memset(xMin, 0xff, sizeof(unsigned short)*EQTAB_LENGTH);
	memset(yMin, 0xff, sizeof(unsigned short)*EQTAB_LENGTH);
	memset(xMax, 0, sizeof(unsigned short)*EQTAB_LENGTH);
	memset(yMax, 0, sizeof(unsigned short)*EQTAB_LENGTH);
	memset(growAll_surf, 0, sizeof(unsigned short)*EQTAB_LENGTH);
	for(r=rmin;r<rmax; r++) {
		pos = r*swidth + cmin;

		u8 * growline = IPLLINE_8U(growImage, r);
		// FIXME : utiliser leftLimit et rightLimit
		for(c=cmin; c<cmax; c++, pos++)
		{
			unsigned short num =
				s_eqTab[ s_RegionLabel[pos] ];
			if(num != s_eqTab[1])
			{
				growline[c] = fillvalue;

				if(c < xMin[num])
					xMin[num] = c;
				else if(c > xMax[num])
					xMax[num] = c;
				if(r < yMin[num])
					yMin[num] = r;
				else if(r > yMax[num])
					yMax[num] = r;
			}
		}
	}


	CvConnectedComp connect;
	memset(&connect, 0, sizeof(CvConnectedComp));
	for(r=1; r<EQTAB_LENGTH; r++) {
		if(xMin[r]!=0xffff) {
			int i=(int)s_eqTab[r];

			//if(surf[i]>40)
			if(xMax[i]>xMin[i] && yMax[i]>yMin[i]) {
				/*
				fprintf(stderr,"[imgutils] %p: Motion Region : %d : %u,%u - %u,%u\n",
					this, i,
					xMin[eqTab[r]], yMin[eqTab[r]],
					xMax[eqTab[r]], yMax[eqTab[r]]);
				*/
				connect.rect.x = xMin[i];
				connect.rect.y = yMin[i];
				connect.rect.width = xMax[i]-xMin[i];
				connect.rect.height = yMax[i]-yMin[i];
				connect.area = growAll_surf[i];

				int retadd = tmAddRegion(connect);
				if(retadd < 0) {
					// There's a problem: it cannot allocate more memory
					// => return here, no need to process anymore
					return -1;
				}
			}
		}
	}

	return 0;

}













extern int saveIplImageAsTIFF(IplImage* img,  const char * outfilename, char * compressionarg);

IplImage * tmLoadImage(const char *filename, int * dpi) {
	IplImage * originalImage = NULL;
	if(strcasestr(filename, ".tif")) {

		originalImage  = tmOpenTiffImage(filename, dpi);

		if(originalImage) {

			if(g_debug_importexport) {
				// DEBUG
				fprintf(stderr, "%s %s:%d : saving for debug : "
						"%s/tmOpenTiffImage.ppm",
						__FILE__, __func__, __LINE__, g_tmp_directory);
				char debug_file[512];
				sprintf(debug_file, "%s/tmOpenTiffImage.ppm",
						g_tmp_directory);
				tmSaveImage(debug_file, originalImage);
			}

			// Ok, loading is done
			return originalImage;
		}
	}
	// load with OpenCV
	originalImage = cvLoadImage(filename,
					(CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR)
					);
	if(g_debug_importexport) {
		// DEBUG
		fprintf(stderr, "%s %s:%d : saving for debug : "
				"%s/cvLoadImage.ppm",
				__FILE__, __func__, __LINE__, g_tmp_directory);
		char debug_file[512];
		sprintf(debug_file, "%s/cvLoadImage.ppm",
				g_tmp_directory);
		tmSaveImage(debug_file, originalImage);
	}
	return originalImage;
}


void tmSaveImage(const char * filename, IplImage * src) {
	if(!src) { return; }

#ifdef HAVE_LIBTIFF
	// If image format if TIFF, save it with libtiff
	if(src->depth != IPL_DEPTH_8U)
		if(strstr(filename, ".tif") || strstr(filename, ".TIF") ) {
			int ret = saveIplImageAsTIFF(src, (char *)filename,
						// compression in none, packbits,jpeg,lzw,zip
						"none"
						//"lzw" //"packbits"
						//"zip" //"lzw"
														);
			if(ret != 0) {
				fprintf(logfile, "cv2tiff : %s:%d : Error with LibTIFF when saving "
					"file '%s' for writing !\n",
					__func__, __LINE__, (char *)filename);
			}
			else {
				// Ok, we can return now
				return;
			}
		}
#endif

	// If image format if PNM, save it with our own source code, else use OpenCV>hugigui function
	if(strstr(filename, "pgm") || strstr(filename, "PGM")
		|| strstr(filename, "ppm") ||  strstr(filename, "PPM")
		|| strstr(filename, "pnm") || strstr(filename, "PNM")) {

		// Save header
		FILE * f = fopen(filename, "wb");
		if(!f) {
			fprintf(logfile, "imgutils : %s:%d : cannot open file '%s' for writing !\n",
				__func__, __LINE__, filename);
			return;
		}
		int width = src->width, height = src->height;

		int byte_per_sample = tmByteDepth(src);
		int maxval = 255;
		switch(byte_per_sample) {
		default:
			fprintf(f, "P2\n"); // 32bit depth is not supported... => save RAW
			width = src->widthStep;
			break;
		case 3:
			fprintf(f, "P6\n"); // 24bit depth if default mode
			break;
		case 1:
			fprintf(f, "P5\n"); // 8bit depth if default mode
			break;
		case 2:
			fprintf(f, "P4\n"); // 16bit gray depth if default mode
			maxval = 65535;
			break;
		case 6:
			fprintf(f, "P3\n"); // 48bit depth if default mode
			maxval = 65535;

			{
				FILE * msb = fopen("/dev/shm/lsb.ppm", "wb");
				if(msb) {
					fprintf(msb, "P6\n%d %d\n255\n",  width, height);
					uchar * msbbuf = (uchar *)(src->imageData+1);
					for(int pos = 0; pos<width*height*6; pos+=2)
						fwrite(&msbbuf[pos], 1, 1, msb);
					fclose(msb);
				}
			}
			fprintf(f, "P5\n"); // 48bit depth if default mode
			width *= 6;
			maxval = 255;

			break;
		}
		fprintf(f, "%d %d\n%d\n", width, height, maxval);

		if(byte_per_sample == 6) {
			fprintf(stderr, "[imgutils] %s:%d : Middle of image bytes : "
							"byte_per_sample=%d : bytes={ ", __func__, __LINE__, byte_per_sample);
			int rmid = (int)(height / 2) + 1;
			int cmid = (int)(width / 2);

			u16 * buffer = (u16 *)(src->imageData
						+ rmid * src->widthStep + cmid * byte_per_sample);
			for(int iter=0; iter<4*3; iter++, buffer++) {
				fprintf(stderr, "%04x ", *buffer);
			}
			fprintf(stderr, "}\n\n");
		}

		if(src->width == src->widthStep) // Save buffer at once
		{
			fwrite(src->imageData, 1, src->widthStep * src-> height, f);
		} else {
			for(int r=0; r<src->height; r++) {
				fwrite(src->imageData + src->widthStep*r, 1,
					src->width * byte_per_sample, f);
			}
		}

		fclose(f);

	}
	else { // Use OpenCV function : cvSaveImage
		fprintf(stderr, "[imgutils] : %s:%d : saving file '%s' with OpenCV ! %d bit\n",
				__func__, __LINE__, filename, tmByteDepth( src )*8);
		cvSaveImage(filename, src);
	}
}
