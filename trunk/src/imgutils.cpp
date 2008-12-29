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


#define IMGUTILS_CPP

#include "imgutils.h"

#include <highgui.h>


FILE * logfile = stderr;

extern u8 g_debug_imgverbose;

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
		byte_depth *= sizeof(unsigned short);
		break;
	case IPL_DEPTH_16S: 
		byte_depth *= sizeof(short);
		break;
	case IPL_DEPTH_32S: 
		byte_depth *= sizeof(long);
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
			img->colorModel[0],img->colorModel[1],img->colorModel[2],img->colorModel[3]
			
			);
	
}

/* Create an IplImage width OpenCV's cvCreateImage and clear buffer */
IplImage * tmCreateImage(CvSize size, int depth, int channels) {
	
	/*
	fprintf(stderr, "[utils] %s:%d : creating IplImage : %dx%d x depth=%d x channels=%d\n",
			__func__, __LINE__, 
			size.width, size.height, depth, channels);
	*/
	IplImage * img = cvCreateImage(size, depth, channels);
	if(img) {
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
void tmReleaseImage(IplImage ** img) {
	if(!img) return;
	if(!(*img) ) return;
	cvReleaseImage(img);
	*img = NULL;
}

/** process a dilatation around the dust */
void tmDilateImage(IplImage * src, IplImage * dst) {
	// Ref. : http://opencvlibrary.sourceforge.net/CvReference#cv_imgproc_morphology
	cvDilate(src, dst, 
		NULL /* e.g. 3x3 kernel */, 
		1 // # iterations
		);
}


// If image has a odd size, many functions will fail on OpenCV, so add a pixel
IplImage * tmAddBorder4x(IplImage * originalImage) {
	if(   (originalImage->width % 4) > 0
	   || (originalImage->height % 4) > 0
	   ) {
		
		fprintf(logfile, "TamanoirImgProc::%s:%d : => Size %dx%d is odd\n",
				__func__, __LINE__, originalImage->width, originalImage->height);
		int new_width = originalImage->width;
		while( (new_width % 4)) new_width++;
		int new_height = originalImage->height;
		while( (new_height % 4)) new_height++;
		
		fprintf(logfile, "TamanoirImgProc::%s:%d : => resize to %d x %d \n",
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
		
		return originalImage;
	}
	
	return originalImage;
}

/** Allocate a morphology structural element */
IplConvKernel * createStructElt()
{
	IplConvKernel *elt;
	int size = 7;
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

	IplConvKernel *elt = createStructElt();

	// perform open
        cvMorphologyEx (src, dst, tmp, elt,
		CV_MOP_OPEN,
		iterations);

	cvReleaseStructuringElement(&elt);
}

void tmCloseImage(IplImage * src, IplImage * dst, IplImage * tmp, int iterations) {

	IplConvKernel *elt = createStructElt();

	// perform open
	cvMorphologyEx (src, dst, tmp, elt,
		CV_MOP_CLOSE,
		iterations);

	cvReleaseStructuringElement(&elt);
}

void tmMarkFailureRegion(IplImage * origImage, 
	int x, int y, int w, int h, unsigned char color) {
	
	
	if(origImage->nChannels == 1)
	{
		cvRectangle(origImage, 
			cvPoint(x - 1, y-1),
			cvPoint(x +w + 1, y+h+1),
			cvScalar(color), 1);
	} else {
		switch(color) {
		//grayQImage.setColor(COLORMARK_CORRECTED, qRgb(0,255,0));
		case COLORMARK_CORRECTED:
			cvRectangle(origImage, 
				cvPoint(x - 1, y-1),
				cvPoint(x +w + 1, y+h+1),
				CV_RGB(0,255,0), 1);
			break;
		//grayQImage.setColor(COLORMARK_REFUSED, qRgb(255,255,0));
		case COLORMARK_REFUSED:
			cvRectangle(origImage, 
				cvPoint(x - 1, y-1),
				cvPoint(x +w + 1, y+h+1),
				CV_RGB(255,255,0)
				//CV_RGB(0,255,255)
				, 1);
			break;
		//grayQImage.setColor(COLORMARK_FAILED, qRgb(255,0,0));
		case COLORMARK_FAILED:
			cvRectangle(origImage, 
				cvPoint(x - 1, y-1),
				cvPoint(x +w + 1, y+h+1),
				CV_RGB(255,0,0)
				//CV_RGB(0,0,255)
				, 1);
			break;
		//grayQImage.setColor(COLORMARK_CURRENT, qRgb(0,0,255));
		case COLORMARK_CURRENT:
			cvRectangle(origImage, 
				cvPoint(x - 1, y-1),
				cvPoint(x +w + 1, y+h+1),
				//CV_RGB(255,0,0)	
				CV_RGB(0,0,255)
				, 1);
			break;
		default:
			cvRectangle(origImage, 
				cvPoint(x - 1, y-1),
				cvPoint(x +w + 1, y+h+1),
				CV_RGB(color,color,color), 1);
			break;
		}
	}
}

/** Return the ratio of pixels non 0 in an IplImage in a region */
float tmNonZeroRatio(IplImage * origImage, int orig_x, int orig_y, int w, int h,
                int exclu_x, int exclu_y, int exclu_w, int exclu_h,
                u8 threshval) {
	int nbpixnon0 = 0;
	
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
	u8 * origBuffer = (u8 *)(origImage->imageData);
	for(int y=orig_y; y<orig_y+h; y++) {
		int pos = y * origImage->widthStep + orig_x;
		for(int x=orig_x; x<orig_x+w; x++, pos++) {
                        if( (x<exclu_x || x >exclu_x+exclu_w)
                            && (y<exclu_y || y >exclu_y+exclu_h))
                        	if( origBuffer[pos] >= threshval)
                        		nbpixnon0++;
		}
	}
	
	return (float)nbpixnon0 / ((float)(w*h));
}


/*
 * Fill a rectangle with a given color -> orig rectangle
 */
void tmFillRegion(IplImage * origImage, 
	int dest_x, int dest_y,
	int copy_width, int copy_height,
        u8 fillValue)
{

	int orig_width = origImage->width;
	int orig_height = origImage->height;

	if(dest_x<0) { dest_x = 0; }
	if(dest_y<0) { dest_y = 0; }
	
	// Clip destination
	if(dest_x + copy_width >= orig_width)
		copy_width = orig_width - dest_x;
        
	if(dest_y + copy_height >= orig_height)
		copy_height = orig_height - dest_y;
        
	if(copy_width <= 0 || copy_height <= 0) {
		fprintf(logfile, "imgutils : %s:%d : INVALID clear %dx%d +%d,%d\n", 
			__func__, __LINE__, 
			copy_width, copy_height,
			dest_x, dest_y);

		return;
	}
	
	if(g_debug_imgverbose)
		fprintf(logfile, "imgutils : %s:%d : clear %dx%d +%d,%d\n", 
			__func__, __LINE__, 
			copy_width, copy_height,
			dest_x, dest_y);
	
	// Clear buffer
	int pitch = origImage->widthStep;
	int byte_depth = tmByteDepth(origImage);
	int copylength = copy_width * byte_depth;
	
	u8 * origImageBuffer = (u8 *)origImage->imageData;
	
	// Raw clear
	for(int y=0; y<copy_height; y++, dest_y++) {
		memset(origImageBuffer + dest_y * pitch + dest_x*byte_depth, 
                        fillValue, 
                        copylength);
	}
}
 
/*
 * Clone copy rectangle -> orig rectangle
 */
void tmCloneRegion(IplImage * origImage, 
	int dest_x, int dest_y,
	int src_x, int src_y, 
	int copy_width, int copy_height,
	IplImage * destImage )
{

	int orig_width = origImage->width;
	int orig_height = origImage->height;

	if(!destImage)
		destImage = origImage;
	
	if(dest_x<0) { dest_x = 0; }
	if(dest_y<0) { dest_y = 0; }
	if(src_x<0) { src_x = 0; }
	if(src_y<0) { src_y = 0; }
	
	// Clip copy or destination
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
	if(copy_width <= 0 || copy_height<=0) {
		fprintf(logfile, "imgutils : %s:%d : INVALID clone src=%d,%d+%dx%d => dest=%d,%d\n", 
			__func__, __LINE__, 
			src_x, src_y, copy_width, copy_height,
			dest_x, dest_y);
		return;
	}
	if(g_debug_imgverbose)
		fprintf(logfile, "imgutils : %s:%d : clone %d,%d+%dx%d => %d,%d\n", 
			__func__, __LINE__, 
			src_x, src_y, copy_width, copy_height,
			dest_x, dest_y);
	
	// FIXME : ROUGH copy for the moment (the regions must not be connected !)
	// Copy buffer
	int pitch = origImage->widthStep;
	int byte_depth = tmByteDepth(origImage);
	int copylength = copy_width * byte_depth;
	int channels = origImage->nChannels;
	
	u8 * origImageBuffer = (u8 *)origImage->imageData;
	u8 * destImageBuffer = (u8 *)destImage->imageData;

	// Raw copy
	if(0) {
		for(int y=0; y<copy_height; y++, dest_y++, src_y++) {
			memcpy(destImageBuffer + dest_y * pitch + dest_x*byte_depth, 
				origImageBuffer + src_y * pitch + src_x*byte_depth, 
				copylength);
		}
	} else {
		int y,x;
		int height_margin = copy_height / 4;
		if(height_margin < 2) height_margin = 2;
		int width_margin = copy_width / 4;
		if(width_margin < 2) width_margin = 2;
		int copy_height_2 = copy_height/2;
		int copy_width_2 = copy_width/2;
		
		// Raw in center, proportional copy in border
		for(y=0; y<copy_height; y++, dest_y++, src_y++) {
			int dy = abs(y - copy_height_2);
			float coef_y = 1.f;
			if(dy > copy_height_2-height_margin)
				coef_y = 1.f -(float)(dy-(copy_height_2-height_margin))/(float)(height_margin);
			
			for(x = 0; x<copy_width; x++) {
				int dx = abs(x - copy_width_2);
				float coef_x = 1.f;
				if(dx > copy_width_2-width_margin)
					coef_x = 1.f -(float)(dx-(copy_width_2-width_margin))/(float)(width_margin);
				
				float coef_copy = coef_x * coef_y;
				
				if(coef_copy < 0.)
					coef_copy = 0.;
				else if(coef_copy > 1.)
					coef_copy = 1.;
				
				float coef_orig = 1.f - coef_copy;
				
				
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
/*
 * Convert a colored/16bit image to 8bit gray */
IplImage * tmFastConvertToGrayscale(IplImage * img) {
	if(!img) { return NULL; }
	IplImage * grayImage = tmCreateImage(cvSize(img->width, img->height), IPL_DEPTH_8U, 1);

	// Convert
	switch(img->depth) {
	default:
		tmReleaseImage(&grayImage);
		return NULL;
	case IPL_DEPTH_8U: // 8bit image as input
		switch(img->nChannels) {
		case 1:	// same size, image is already grayscaled
			memcpy(grayImage->imageData, img->imageData, img->widthStep*img->height);
			break;
		default: { // Use Green plane as grayscaled
			int offset = 1; // green
			int bytedepth = tmByteDepth(img);
			unsigned char * buf_in = (unsigned char *)img->imageData;
			unsigned char * buf_out = (unsigned char *)grayImage->imageData;
			for(int pos = offset; pos < img->widthStep * img->height; pos+=bytedepth, buf_out++) {
				*buf_out = buf_in[pos];
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


/*
 * Crop image
 */
void tmCropImage(IplImage * origImage, 
	IplImage * cropImage, 
	int crop_x, int crop_y) {
	
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
						if(val == 255) val = 254;
						
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
		for(int y=ytop; y<ybottom; ly++, y++) 
			memcpy(cropImageBuffer + ly * cropImage->widthStep, 
				origImageBuffer + xmin_x_depth + y * origImage->widthStep, copywidth);
		
		//tmSaveImage ("/dev/shm/tmCropImage_cropImage.ppm", cropImage);
		//tmSaveImage ("/dev/shm/tmCropImage_origImage.ppm", origImage);
	}
}

void tmMarkCloneRegion(IplImage * origImage, 
	int orig_x, int orig_y,
	int copy_x, int copy_y, 
	int copy_width, int copy_height,
	bool mark
	) {

	int orig_width = origImage->width;
	int orig_height = origImage->height;

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
	
	// Debug : original (=bad) as red, copy source (=good) in green
	if(mark) {
		cvRectangle(origImage, 
					cvPoint(orig_x, orig_y+copy_height),
					cvPoint(orig_x+copy_width, orig_y),
					CV_RGB(255,0,0),
					1);
	}
	
	cvRectangle(origImage, 
				cvPoint(copy_x, copy_y+copy_height),
				cvPoint(copy_x+copy_width, copy_y),
				origImage->nChannels>1?CV_RGB(0,255,0):cvScalarAll(255),
				1);
	// Copy vector
	int copy_center_x = copy_x+copy_width/2;
	int copy_center_y = copy_y+copy_height/2;
	int src_center_x = orig_x+copy_width/2;
	int src_center_y = orig_y+copy_height/2;
	int copy_vector_x = copy_center_x - src_center_x;
	int copy_vector_y = copy_center_y - src_center_y;
	
	float vector_len = sqrt(copy_vector_x*copy_vector_x+copy_vector_y*copy_vector_y);
	if(vector_len > 10) {
		float shorter_len = vector_len - 10.f;
		copy_vector_x = (int)roundf((float)copy_vector_x * shorter_len / vector_len);
		copy_vector_y = (int)roundf((float)copy_vector_y * shorter_len / vector_len);
	}
		
    // Main move
	cvLine(origImage, 
		   cvPoint(copy_center_x, copy_center_y),
		   cvPoint(copy_center_x - copy_vector_x, copy_center_y - copy_vector_y),
				origImage->nChannels>1?CV_RGB(0,255,0):cvScalarAll(255),
		   1);
	
	int arrow_vector_x = -copy_vector_x;
	int arrow_vector_y = -copy_vector_y;
	// Arrow
	cvLine(origImage, 
		   cvPoint(copy_center_x + arrow_vector_x*3/4 - arrow_vector_y*1/4 ,
				   copy_center_y + arrow_vector_y*3/4 + arrow_vector_x*1/4 ),
		   cvPoint(copy_center_x - copy_vector_x, copy_center_y - copy_vector_y),
				origImage->nChannels>1?CV_RGB(0,255,0):cvScalarAll(255),
		   1);
	cvLine(origImage, 
		   cvPoint(copy_center_x + arrow_vector_x*3/4 + arrow_vector_y*1/4 ,
				   copy_center_y + arrow_vector_y*3/4 - arrow_vector_x*1/4 ),
		   cvPoint(copy_center_x - copy_vector_x, copy_center_y - copy_vector_y),
				origImage->nChannels>1?CV_RGB(0,255,0):cvScalarAll(255),
		   1);
	
}



float tmCorrelation(
	IplImage * img1, IplImage * img2,
	IplImage * maskImage, 
	int x1, int y1,
	int x2, int y2,
	int w, int h,
	int difftolerance,
	int * pmaxdiff) 
{
	// Don't process on borders
	if(x1<0 || y1<0 || x1+w>img1->width || y1+h>=img1->height) 
	{
		*pmaxdiff = 1000;
		return 1000.f;
	}
	if(x2<0 || y2<0 || x2+w>img2->width || y2+h>=img2->height) 
	{
		*pmaxdiff = 1000;
		return 1000.f;
	}
	/* IplImage:
	int  depth; // pixel depth in bits: IPL_DEPTH_8U, IPL_DEPTH_8S, IPL_DEPTH_16S,
                           IPL_DEPTH_32S, IPL_DEPTH_32F and IPL_DEPTH_64F are supported 
	*/
	*pmaxdiff = 1000;
	long diff = 0;
	long nbpix = 0;
	
	int pitch1 = img1->widthStep;
	int pitch2 = img2->widthStep;
	int pitchmask = maskImage->widthStep;
	
	int channels = img1->nChannels;
	//int depth = tmByteDepth(img1);
	//int w_x_depth = w*depth;
	//int w_x_channels = w * channels;
	int x1_x_depth = x1 * tmByteDepth(img1); // e.g. nChannel * pixel byte depth
	int x2_x_depth = x2 * tmByteDepth(img2);
	
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
				if(*maskpos == 0) {
					for(int d=0; d<channels; d++) {
						long l_diff = abs( (long)*img1pos - (long)*img2pos);
						if(l_diff > maxdiff)
						{
							maxdiff = l_diff;
							if(l_diff > difftolerance) // Abort correlation
							{
								return l_diff;
							}
						}
						
						diff += l_diff; nbpix++;
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
						if(l_diff > maxdiff)
						{
							maxdiff = l_diff;
							if(l_diff > difftolerance) // Abort correlation
							{
								return l_diff;
							}
						}
						diff += l_diff; nbpix++;
						img1pos++;
						img2pos++;
					}
					
				} else {
					img1pos+=channels;
					img2pos+=channels;
				} 
				
				img1pos++;
				img2pos++;
				//diffpos++;
				maskpos++;
			}
		}
		break;
	}
	
	if(nbpix == 0) 
		return 5000.f;
	
	// Return worst case
	if(pmaxdiff)
		*pmaxdiff = maxdiff;
	
	// Return best 
	return (float)((float)diff / (float)nbpix);
}


u8 g_debug_correlation = 0;
IplImage * correlationImage = NULL;

IplImage * getCorrelationImage() { return correlationImage; };

/** Find a neighbour region to copy on the dust */
int tmSearchBestCorrelation(
	IplImage * origImage, IplImage * maskImage, 
	int seed_x, int seed_y, 
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
	int correl_width  = 2 * seed_width;
	int correl_height = 2 * seed_height;
	seed_x -= correl_width/2; if(seed_x<0) seed_x = 0;
	seed_y -= correl_height/2;if(seed_y<0) seed_y = 0;
	
	
	int search_width  = 3 * correl_width;
	int search_height = 3 * correl_height;

	
	// Best offset
	int best_offset_x = 0;
	int best_offset_y = 0;
	
	float least_worst  = 255.f;
	float best_dist = 9.f; // 5 / 255 
	float best_best = 1.f; // 3 / 255 
	
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
	
	int difftolerance = (int)best_dist;
	
	for(int dy = -search_height; dy < search_height; dy++) {
		// We look only if the regions cannot touch 
		if(dy+seed_y > 0 && dy+seed_y+correl_height<origImage->height)
		for(int dx = -search_width; dx < search_width; dx++) {
			if(dx+seed_x > 0 && dx+seed_x+correl_width<origImage->width)
			if(dy < -correl_height || dy >= correl_height 
				|| dx < -correl_width || dx >= correl_width) {
				
				int maxdiff = 0;
				float l_dist = tmCorrelation(origImage, origImage,
						maskImage, 
						seed_x, seed_y,
						seed_x + dx, seed_y + dy,
						correl_width, correl_height,
						difftolerance,
						&maxdiff
						);
				
				if(l_dist < least_worst) 
					least_worst = l_dist;
				
				if(g_debug_correlation) {
					fprintf(logfile, "\timgutils %s:%d : (+%d,+%d) = %f / difftol=%d maxdiff=%d\n", 
							__func__, __LINE__,	dx, dy, l_dist, difftolerance, maxdiff); 
					u8 * correlationImageBuffer = (u8 *)correlationImage->imageData;
					correlationImageBuffer[ correlationImage->widthStep * (seed_y + dy)
											+ seed_x + dx] = (u8)l_dist;
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
							"(+%d,+%d) = %f / %d  (%dx%d)\n", 
							__func__, __LINE__,
							dx, dy, l_dist, difftolerance,
							correl_width, correl_height
							);
				
					// Top quality correlation : exist asap
					if(l_dist < best_best) {
						*copy_dest_x = seed_x;
						*copy_dest_y = seed_y;
						*copy_src_x = seed_x + best_offset_x;
						*copy_src_y = seed_y + best_offset_y;
						*copy_width = correl_width;
						*copy_height = correl_height;
						
						if(best_correl) 
							*best_correl = (int)least_worst;
						
						return 1;
					}
				}
			}
		}
	}
	
	if(retval) {
		*copy_dest_x = seed_x;
		*copy_dest_y = seed_y;
		*copy_src_x = seed_x + best_offset_x;
		*copy_src_y = seed_y + best_offset_y;
		*copy_width = correl_width;
		*copy_height = correl_height;
	}
	
	
	if(best_correl) 
		*best_correl = (int)least_worst;
	return retval;
}
	






int processDiff(int l_FilmType, IplImage * grayImage,  IplImage * medianImage,  IplImage * diffImage, unsigned long * diffHisto) 
{
	unsigned char * grayImageBuffer = (unsigned char *)grayImage->imageData;
	unsigned char * blurImageBuffer = (unsigned char *)medianImage->imageData;
	unsigned char * diffImageBuffer = (unsigned char *)diffImage->imageData;
	int width = medianImage->widthStep;
	int height = medianImage->height;
	
	switch(l_FilmType) {
	default:
	case FILM_UNDEFINED: /* dust can be darker or brighter => absolute value */
		fprintf(logfile, "[imgutils] %s:%d : Undefined film type : looking for diffImage = |Blurred - Grayscaled| ...\n", 
			__func__, __LINE__);
		for(int pos=0; pos<width*height; pos++) 
		{
			u8 diff = diffImageBuffer[pos] = (u8)abs((long)grayImageBuffer[pos] 
					- (long)blurImageBuffer[pos]);
			diffHisto[diff]++;
		}
		break;
	case FILM_NEGATIVE: /* dust must be brighter => gray - median */
		fprintf(logfile, "[imgutils] %s:%d : NEGATIVE film type : looking for diffImage = Grayscaled-Blurred ...\n", 
			__func__, __LINE__);
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
		fprintf(logfile, "[imgutils] %s:%d : POSITIVE film type : looking for diffImage = Blurred-Grayscaled ...\n", 
			__func__, __LINE__);
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
 * Grow a region from one seed 
 */


#define spmax 576*2

void tmGrowRegion(unsigned char * growIn, unsigned char * growOut, 
	int swidth, int sheight, 
	int c, int r, 
	unsigned char threshold,
	unsigned char fillValue,
	CvConnectedComp * areaOut)
{
	int pile_x[spmax];
	int pile_y[spmax];
	
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
		memset(growOut + row+xi, fillValue, w);
		surf += w;
		
		if(xi<growXMin) growXMin = xi;
		if(xf>growXMax) growXMax = xf;
		if(y<growYMin) growYMin = y;
		if(y>growYMax) growYMax = y;

        
		// we look for new seed
		pile_sp --;

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
		
			while(x>xi) { // 8-con
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
	
	// conclusion
	areaOut->rect.x = growXMin;
	areaOut->rect.y = growYMin;
	areaOut->rect.width = (growXMax - growXMin+1);
	areaOut->rect.height = (growYMax - growYMin+1);
	
	areaOut->area = (double)surf;
}



extern int saveIplImageAsTIFF(IplImage* img,  const char * outfilename, char * compressionarg);


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
					__func__, __LINE__, filename);
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
		case 3:
		default:
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
			
			unsigned short * buffer = (unsigned short *)(src->imageData 
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
