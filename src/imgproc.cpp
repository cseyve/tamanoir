/***************************************************************************
 *           imgproc.cpp - Main image processing 
 *
 *  Tue Oct 23 22:10:56 2007
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
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <highgui.h>

#include "imgproc.h"
#include "imgutils.h"

extern FILE * logfile;
u8 g_debug_imgverbose = 0;
u8 g_debug_imgoutput = 0;
extern u8 g_debug_correlation;


/// Minimal dust size in pixel^2
#define DUST_MIN_SIZE		5

/// Minimal hot pixel size in pixel^2
#define HOTPIXEL_MIN_SIZE	1

#define TRUST_CORREL_MAX 4
#define TRUST_AREA_MAX	25


/** global option : stop if a dust has no replace candidate */
u8 g_option_stopoguess = 0;
u8 g_dataset_mode = 0;
u8 g_evaluate_mode = 0;

/** @brief dataset file output */
FILE * g_dataset_f = NULL;

int found_score = 0;
t_known_dust * known_dusts = NULL;
int nb_known_dusts = 0;
int nb_known_dusts_forced = 0;
int max_known_dusts = 0;
double orig_width=0, orig_height=0;



TamanoirImgProc::TamanoirImgProc(int bw, int bh) {
	fprintf(logfile, "TamanoirImgProc::%s:%d block %dx%d...\n", 
		__func__, __LINE__, bw, bh);
	
	init();
	blockSize = cvSize(bw, bh);
}



void TamanoirImgProc::init() {

	m_filename[0] = '\0';
#ifdef SIMPLE_VIEW
	m_show_crop_debug = false;
#else
	m_show_crop_debug = true;
#endif
	m_lock = false;
	
	
	m_FilmType = FILM_UNDEFINED;
	m_hotPixels = false;
	
	/* Default values : 
	- Trust good correction proposals : no
	- DPI : 2400 dpi
	- hot pixels : no (scan from film !)
	*/
	m_trust = false;
	m_dpi = 2400;
	m_hotPixels = false;
	
	/** Original image size */
	originalSize = cvSize(0,0);

	/** Processed imge size */
	processingSize = cvSize(0,0);

#ifndef WIN32
	pthread_mutex_init(&mutex, NULL);
#endif

	// Image buffers
	originalImage = NULL;
	origBlurredImage = NULL;
	
	// Working images : full size 
	grayImage = NULL;
	medianImage = NULL;
	diffImage = NULL;
	varianceImage = NULL;
	growImage = NULL;
	
	// Working images : cropped
	cropImage = NULL;
	dilateImage = NULL;
	correctImage = NULL;
	tmpCropImage = NULL;
	cropColorImage = NULL;
	correctColorImage = NULL;
	sobelImage = NULL;
	
	// Display images
	displayImage = displayBlockImage = NULL;
	originalSmallImage = NULL;

	disp_cropColorImage =
		disp_correctColorImage =
		disp_dilateImage = 
		disp_cropImage = NULL;
	
	displaySize = cvSize(0,0);
	displayCropSize = cvSize(0,0);
	
	memset(&m_correct, 0, sizeof(t_correction));
	memset(&m_last_correction, 0, sizeof(t_correction));
	
	/** Clear stats */
	memset(&m_dust_stats, 0, sizeof(dust_stats_t));
}

TamanoirImgProc::~TamanoirImgProc() {
	purge();
}



void TamanoirImgProc::purge() {
	// Dataset output file
	if(g_dataset_f) {
		if(g_dataset_mode) {
			fprintf(g_dataset_f, "\n[Stats]\n");
			processAndPrintStats(&m_dust_stats, g_dataset_f);
		}
		else if(g_evaluate_mode) {
			// Print performance statistics
			m_perf_stats.false_negative = nb_known_dusts  // ground truth
					- m_perf_stats.true_positive;
			
			fprintf(stderr, "[ImgProc]::%s:%d : PERFORMANCES / Ground Truth\n"
					"\tGround truth : %d dusts / %d forced\n"
					"\tTrue positive : %d / GT:%d = %g %%\n"
					"\tFalse positive : %d / GT:%d = %g %%\n"
					, __func__, __LINE__,
					nb_known_dusts, nb_known_dusts_forced,
					m_perf_stats.true_positive, nb_known_dusts, 100.f*(float)m_perf_stats.true_positive/(float)nb_known_dusts,
					m_perf_stats.false_positive, nb_known_dusts, 100.f*(float)m_perf_stats.false_positive/(float)nb_known_dusts
					);
		}
		
		fclose(g_dataset_f);
		
		g_dataset_f = NULL;
	}

	if(originalImage) tmReleaseImage(&originalImage);  originalImage = NULL;
	if(displayImage) tmReleaseImage(&displayImage); displayImage = NULL;
	if(displayBlockImage) tmReleaseImage(&displayBlockImage); displayBlockImage = NULL;
	if(originalSmallImage) tmReleaseImage(&originalSmallImage); originalSmallImage = NULL;
	
	// Big images
	if(grayImage) tmReleaseImage(&grayImage);  grayImage = NULL;
	if(origBlurredImage) tmReleaseImage(&origBlurredImage); origBlurredImage = NULL;
	if(medianImage) tmReleaseImage(&medianImage);  medianImage = NULL;
	if(diffImage) 	tmReleaseImage(&diffImage);  diffImage = NULL;
	if(varianceImage) 	tmReleaseImage(&varianceImage);  varianceImage = NULL;
	if(growImage) 	tmReleaseImage(&growImage);  growImage = NULL;

	// purge display and processing buffers
	purgeDisplay();
	purgeCropped();
}

void TamanoirImgProc::purgeCropped() {
	// Cropped images
	if(cropImage) tmReleaseImage(&cropImage);  cropImage = NULL;
	if(cropColorImage)	tmReleaseImage(&cropColorImage); cropColorImage = NULL;
	if(dilateImage) 	tmReleaseImage(&dilateImage);  		dilateImage = NULL;
	if(correctImage) 	tmReleaseImage(&correctImage);  	correctImage = NULL;
	if(tmpCropImage) 	tmReleaseImage(&tmpCropImage);  	tmpCropImage = NULL;
	if(sobelImage) 		tmReleaseImage(&sobelImage);  	sobelImage = NULL;
	processingSize = cvSize(0,0);
}

void TamanoirImgProc::purgeDisplay() {
	fprintf(stderr, "TamanoirImgProc::%s:%d \n", __func__, __LINE__);
	// Display images
	if(disp_cropImage) tmReleaseImage(&disp_cropImage);  disp_cropImage = NULL;
	if(disp_cropColorImage) 	tmReleaseImage(&disp_cropColorImage); disp_cropColorImage = NULL;
	if(disp_correctColorImage) 	tmReleaseImage(&disp_correctColorImage); disp_correctColorImage = NULL;
	if(disp_dilateImage) 	tmReleaseImage(&disp_dilateImage); 	disp_dilateImage = NULL;
	displayCropSize = cvSize(0,0);
}




void TamanoirImgProc::setDisplaySize(int w, int h) {
	if(!//medianImage // is forst created when loading gray image
	   grayImage
	   ) {
		displaySize = cvSize(w, h);
		return;
	}

	if(w == 0 || h == 0) return;

	if(displayImage) { return; }
	
	IplImage * old_displayImg = displayBlockImage;
	if(displayImage) {
		if(w == displayImage->width && h == displayImage->height)
			return; // Already displayed
	}
	displayBlockImage = NULL;
	
	tmReleaseImage(&old_displayImg);
	tmReleaseImage(&displayImage);

	// Get best fit w/h for display in main frame
	int gray_width = grayImage->width;
	while((gray_width % 4) > 0)
		gray_width--;
	
	//tmReleaseImage(&displayImage); displayImage = NULL;
	
	int scaled_width = w;
	int scaled_height = h;

	float scale_x = (float)w / (float)grayImage->width ;
	float scale_y = (float)h / (float)grayImage->height;


	// use lower factor
	float factor = std::min(scale_x, scale_y);
	if(factor>1.f) factor = 1.f;
	scaled_width = (int)(factor*grayImage->width);
	scaled_height = (int)(factor*grayImage->height);

	fprintf(stderr, "TamanoirImgProc::%s:%d : factor=%g => scaled size %dx%d\n", __func__, __LINE__,
			factor,
			scaled_width, scaled_height);
	
	// Use only size with 4xN dimensions
	while(scaled_width % 4) { scaled_width--; }
	while(scaled_height % 4) { scaled_height--; }
	
	displaySize = cvSize(scaled_width, scaled_height);
	
	// Resize
//	displayImage = tmCreateImage(displaySize, IPL_DEPTH_8U, 1);
	IplImage * new_displayImage = tmCreateImage(displaySize, IPL_DEPTH_8U, originalImage->nChannels);
	if(originalImage->depth != new_displayImage->depth) {
		IplImage * tmpDisplayImage = tmCreateImage(cvSize(originalImage->width,originalImage->height),
			IPL_DEPTH_8U, originalImage->nChannels);
		cvConvertImage(originalImage, tmpDisplayImage );
		cvResize(tmpDisplayImage, new_displayImage, CV_INTER_LINEAR );
		tmReleaseImage(&tmpDisplayImage);


	} else {
		cvResize(originalImage, new_displayImage, CV_INTER_LINEAR );
	}

	// update size, because it may change in cvResize
	displaySize = cvSize(new_displayImage->width, new_displayImage->height);

	// Main display image (with permanent modification)
	displayImage = tmCreateImage(displaySize, IPL_DEPTH_8U, originalImage->nChannels);

	fprintf(stderr, "TamanoirImgProc::%s:%d scaling %dx%d -> %dx%d...\n",
		__func__, __LINE__,
		originalImage->width, originalImage->height,
		new_displayImage->width, new_displayImage->height
		);
	
	if(originalImage->nChannels == 1) {
		// Prevent image values to be > 253
		for(int r=0; r<new_displayImage->height; r++)
		{
			u8 * lineGray = (u8 *)new_displayImage->imageData 
				+ r * new_displayImage->widthStep;
			for(int c = 0 ; c<new_displayImage->width; c++)
				if(lineGray[c] >= COLORMARK_FAILED)
					lineGray[c] = COLORMARK_FAILED-1;
		}
	} else {
		if(originalImage->depth != IPL_DEPTH_8U) {
			fprintf(stderr, "TamanoirImgProc::%s:%d : invert R <-> B for 8 bit images\n", 
					__func__, __LINE__);
			for(int r=0; r<new_displayImage->height; r++)
			{
				u8 * lineGray = (u8 *)new_displayImage->imageData 
					+ r * new_displayImage->widthStep;
				for(int c = 0 ; c< new_displayImage->width * new_displayImage->nChannels; c+=new_displayImage->nChannels) {
					u8 tmp = lineGray[c];
					lineGray[c] =  lineGray[c+2];
					lineGray[c+2] = tmp;
				}
			}
		}
	}
	
	if(g_debug_savetmp) { tmSaveImage(TMP_DIRECTORY "displayImage" IMG_EXTENSION, new_displayImage); }

	cvCopy(new_displayImage, displayImage);

	// only set the pointer at the end of the fonction, it will unlock the graphical user interface
	displayBlockImage = new_displayImage;
}





int TamanoirImgProc::loadFile(const char * filename) {
	
	int retry = 0;
	while(retry < 10 && m_lock) {
		sleep(1); retry++;
		fprintf(stderr, "[imgproc]::%s:%d : locked !!\n", __func__, __LINE__);
	}
	
	if(m_lock) {
		return -1;
	}
	m_lock = true;
	
	m_progress = 0;
	
	// Clear display and processing images
	purge();
	
	/* Load with OpenCV cvLoadImage 
	IplImage* cvLoadImage( const char* filename, int iscolor CV_DEFAULT(1));
	*/
	fprintf(logfile, "TamanoirImgProc::%s:%d : loading '%s'...\n", 
		__func__, __LINE__, filename);
	struct timeval tvLoad1;
	gettimeofday(&tvLoad1, NULL);
	originalImage = cvLoadImage(filename,
					(CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR)
					);
	m_progress = 10;
	
	if(!originalImage) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : cannot open file '%s' !!\n",
					__func__, __LINE__, filename);
		m_lock = false;
		m_filename[0] = '\0';
		return -1;
	}
	struct timeval tvLoad2;
	gettimeofday(&tvLoad2, NULL);

	long dt_ms = (tvLoad2.tv_sec - tvLoad1.tv_sec)*1000
				 + (tvLoad2.tv_usec - tvLoad1.tv_usec)/1000;

	tmPrintProperties (originalImage);
	
	// Save image in /dev/shm
	//tmSaveImage ("/dev/shm/originalImage.ppm", originalImage );
	
	strcpy(m_filename, filename);
	
	// Reset evaluation
	memset(&m_perf_stats, 0, sizeof(t_perf_stats));
	
	if(g_dataset_mode || g_evaluate_mode) {
		if(g_dataset_f) {
			fclose(g_dataset_f);
			g_dataset_f = NULL;
		}
		

		if(!g_dataset_f) {
			char datafile[512];
			sprintf(datafile, "%s.data", m_filename);
			
			if(g_evaluate_mode) {
				g_dataset_f = fopen(datafile, "r");
				
				// Reset evaluation
				if(known_dusts) delete [] known_dusts;
				known_dusts = NULL;
				nb_known_dusts = nb_known_dusts_forced = 0;
				max_known_dusts = 0;
				orig_width = orig_height = 0.;
			} else {
				g_dataset_f = fopen(datafile, "w");
			}
			
			if(!g_dataset_f) {
				fprintf(stderr, "[imgproc]::%s:%d : cannot open file %s\n", __func__, __LINE__,
					datafile);
			}
			else {
				fprintf(stderr, "[imgproc]::%s:%d : DATASET: open file %s\n", __func__, __LINE__,
					datafile);
			}

			// Write header
			if(g_dataset_f && g_dataset_mode) {
				
				fprintf(g_dataset_f, "# Tamanoir dataset for file '%s'\n\n", 
							m_filename);
				fprintf(g_dataset_f, "Path\t%s\n", 
							m_filename);
				fprintf(g_dataset_f, "Size\t%dx%d\n", originalImage->width, originalImage->height);
				fprintf(g_dataset_f, "Channels\t%d\n", originalImage->nChannels);
				fprintf(g_dataset_f, "Depth\t%d\n", tmByteDepth(originalImage) );
				
				
				fprintf(g_dataset_f, "\n[Settings]\n");
				fprintf(g_dataset_f, "FilmType\t%d\n", m_FilmType);
				fprintf(g_dataset_f, "Dpi\t%d\n", m_dpi);
				fprintf(g_dataset_f, "HotPixels\t%d\n", m_hotPixels?'T':'F');
				fprintf(g_dataset_f, "Trust\t%d\n", m_trust?'T':'F');
				fprintf(g_dataset_f, "\n[Dusts]\n");
				fprintf(g_dataset_f, "#Dust\tPosition\tSeed\tForced\n");
				fflush(g_dataset_f);
			}
		}
	}
	
	switch(originalImage->depth) {
	case IPL_DEPTH_8S: 
		fprintf(logfile, "[imgutils] %s:%d : depth IPL_DEPTH_8S\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_16S: 
		fprintf(logfile, "[imgutils] %s:%d : depth IPL_DEPTH_16S\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_32S: 
		fprintf(logfile, "[imgutils] %s:%d : depth IPL_DEPTH_32S\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_32F: 
		fprintf(logfile, "[imgutils] %s:%d : depth IPL_DEPTH_32F\n", __func__, __LINE__);
		break;
	case IPL_DEPTH_64F: 
		fprintf(logfile, "[imgutils] %s:%d : depth IPL_DEPTH_64F \n", __func__, __LINE__);
		break;	
	case IPL_DEPTH_8U: 
		fprintf(logfile, "[imgutils] %s:%d : depth IPL_DEPTH_8U \n", __func__, __LINE__);
		break;	
	case IPL_DEPTH_16U: 
		fprintf(logfile, "[imgutils] %s:%d : depth IPL_DEPTH_16U \n", __func__, __LINE__);
		break;	
	}
	
	
	fprintf(logfile, "TamanoirImgProc::%s:%d : '%s' => w=%d x h=%d x depth=%d x %d bytes : read time= %ld ms\n",
		__func__, __LINE__, filename, 
		originalImage->width, originalImage->height, originalImage->nChannels,
		tmByteDepth(originalImage), dt_ms);
	

	originalImage = tmAddBorder4x(originalImage);
	m_progress = 15;
	fprintf(logfile, "TamanoirImgProc::%s:%d : '%s' => w=%d x h=%d x channels=%d => %d bytes per pixel\n"
			"\tLoaded in %ld ms\n",
			__func__, __LINE__, filename, 
			originalImage->width, originalImage->height, originalImage->nChannels,
			tmByteDepth(originalImage),
			dt_ms);
	
	// convert to Grayscaled image
	if(dt_ms > 2000) {
		// Huge file : should use faster method than reload to convert to grayscale
		fprintf(logfile, "TamanoirImgProc::%s:%d : fast conversion to grayscaled image...\n",
			__func__, __LINE__);
		grayImage = tmFastConvertToGrayscale(originalImage);

		m_progress = 20;
	}
	
	
	#ifdef CV_LOAD_IMAGE_GRAYSCALE
	if(originalImage->depth != IPL_DEPTH_8U
			&& !grayImage
		//&& dt_ms < 5000 // Maybe the hard drive is a removable disk, so it will be faster to convert from already read data
		) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : reload as grayscaled image...\n",
			__func__, __LINE__);
		grayImage = cvLoadImage(filename, CV_LOAD_IMAGE_GRAYSCALE);
		m_progress = 20;
		grayImage = tmAddBorder4x(grayImage);
		m_progress = 25;
	}
	else
	#endif
	if(!grayImage) {
		m_progress = 20;

		// convert full image to grayscale
		grayImage = tmCreateImage(cvSize(originalImage->width, originalImage->height), IPL_DEPTH_8U, 1);
		fprintf(logfile, "TamanoirImgProc::%s:%d : created grayscaled "
				"=> w=%d x h=%d x channels=%d => %d bytes per pixel\n",
				__func__, __LINE__,
				grayImage->width, grayImage->height, grayImage->nChannels,
				tmByteDepth(originalImage));

		switch(originalImage->nChannels) {
		default:
		case 1:
			fprintf(logfile, "TamanoirImgProc::%s:%d : copy to Grayscaled image...\n",
				__func__, __LINE__);
			memcpy(grayImage->imageData, originalImage->imageData, originalImage->widthStep * originalImage->height);
			break;
		case 3:
			fprintf(logfile, "TamanoirImgProc::%s:%d : convert RGB24 to Grayscaled image...\n",
				__func__, __LINE__);
			cvCvtColor(originalImage, grayImage, CV_RGB2GRAY);
			break;
		case 4:
			fprintf(logfile, "TamanoirImgProc::%s:%d : convert BGRA to Grayscaled image...\n",
				__func__, __LINE__);
			cvCvtColor(originalImage, grayImage, CV_BGRA2GRAY);
			break;
		}
	}
	m_progress = 25;
	
	if(displaySize.width > 0) {
		setDisplaySize(displaySize.width, displaySize.height);
	}

	m_lock = false;
	
	float mean = 0.f, variance = 0.f, diff_mean = 0.f;
	int uniform = findUniform(&mean, &diff_mean, &variance);
	if(uniform >= 0) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : Process uniform zone detection => mean=%g, diff=%g, var=%g\n",
			__func__, __LINE__, mean, diff_mean, variance);
	}
	
	fprintf(logfile, "TamanoirImgProc::%s:%d : pre-processing image...\n", 
		__func__, __LINE__);
	preProcessImage();
	
	return 0;
}

/*
 * IMAGE PRE-PROCESSING => DUST DETECTION MAP
 */
int TamanoirImgProc::preProcessImage() {
	
	int retry = 0;
	while(retry < 10 && m_lock) {
		sleep(1); retry++;
		fprintf(stderr, "[imgproc]::%s:%d : locked !!\n", __func__, __LINE__);
	}
	if(m_lock)
		return -1;
	
	m_lock = true;
	
	
	memset(&m_last_correction, 0, sizeof(t_correction));
	memset(&m_correct, 0, sizeof(t_correction));
	m_seed_x = m_seed_y = 0; // Reset search position
	
	m_progress = 25;
	originalSize = cvSize(originalImage->width, originalImage->height);
	
	// Blurred grayscaled image (Gaussian blur) 
	if(!medianImage) 
		medianImage = tmCreateImage(cvSize(originalImage->width, originalImage->height),
			IPL_DEPTH_8U, 1);
	
	// Difference (allocated here because it may be used for Open filter 
	if(!diffImage) {
		diffImage = tmCreateImage(cvSize(originalImage->width, originalImage->height),
			IPL_DEPTH_8U, 1);
	}
	if(!varianceImage) {
		varianceImage = tmCreateImage(cvSize(originalImage->width, originalImage->height),
			IPL_DEPTH_32F, 1);
	}

	m_progress = 30;
	// Smooth siz depend on DPI - size of 9 is ok at 2400 dpi
	m_smooth_size = 1 + 2*(int)(4 * m_dpi / 2400);
	if(m_smooth_size < 3)
		m_smooth_size = 3;
	
	fprintf(logfile, "TamanoirImgProc::%s:%d : blur grayscaled image => median image... size %d x %d\n",
		__func__, __LINE__, m_smooth_size, m_smooth_size);

	switch(m_FilmType) {
	default: {
		fprintf(logfile, "TamanoirImgProc::%s:%d : default type: smoothing input image as medianImage (smooth size:%d)...\n",
			__func__, __LINE__, m_smooth_size);

		cvSmooth(grayImage, medianImage, 
			CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
			m_smooth_size, m_smooth_size );
		
		}break;
	case FILM_NEGATIVE:
		fprintf(logfile, "TamanoirImgProc::%s:%d : blur original color image...\n", 
			__func__, __LINE__);
		int orig_smooth_size = 1 + 2*(int)(4 * m_dpi / 2400);

		if(!origBlurredImage) {
			origBlurredImage = tmCreateImage(cvSize(originalImage->width, originalImage->height),
				originalImage->depth, originalImage->nChannels);
			
			// First, blur the color image
			cvSmooth( originalImage, origBlurredImage,
				CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
				orig_smooth_size, orig_smooth_size); //int param1=3, int param2=0 );
					 // FIXME : adapt size to resolution
		}
		m_progress = 35;
		
		
		int open_iter = m_dpi / 2400;
		if(open_iter < 1) open_iter = 1;
		fprintf(logfile, "TamanoirImgProc::%s:%d : blur grayscaled image (Smooth %dx%d x %d times)...\n",
			__func__, __LINE__, m_smooth_size, m_smooth_size,1);

		cvSmooth(grayImage, medianImage, 
			CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
//			orig_smooth_size, orig_smooth_size); //
			m_smooth_size, m_smooth_size );
		/* // CSE : test 2009-01-°04 : with open instead of smooth
		tmOpenImage(
			grayImage,  // => src 
			medianImage, // => dest
			diffImage, // => tmp
			open_iter);
		*/
		fprintf(logfile, "TamanoirImgProc::%s:%d : blur grayscaled image OK\n", 
			__func__, __LINE__);
		break;
	}
		// For debug, save image in temporary directory
		if(g_debug_savetmp) tmSaveImage(TMP_DIRECTORY "medianImage" IMG_EXTENSION, medianImage);
	
	m_progress = 40;
	
	
	
	// ********* DIFFERENCE BETWEEN ORIGINAL AND BLURRED ****************
	// Also compute variance image
	fprintf(logfile, "TamanoirImgProc::%s:%d : difference processing image...\n",
		__func__, __LINE__);
		memset(diffImage->imageData, 0, diffImage->widthStep*diffImage->height);
	
	// Difference histogram
	unsigned long diffHisto[256];
	memset(diffHisto, 0, sizeof(unsigned long)*256);

	int var_size = 1 + 2*(4 * m_dpi/2400);
	if(var_size < 3) var_size = 3;
	processDiff(m_FilmType, grayImage, medianImage, diffImage, varianceImage, diffHisto, var_size);
	
	// Process difference histogram analysis : 
	unsigned long maxHisto = 0;
	double cdgH = 0;
	unsigned long cdgnb = 0;
	
	int indexMaxHisto = 0;
	
	for(int h=1; h<256; h++) {
		cdgH += h * diffHisto[h];
		cdgnb += diffHisto[h];
		if(diffHisto[h]>maxHisto) {
			indexMaxHisto = h;
			maxHisto = diffHisto[h];
		}
	}
	if(cdgnb>0)
		cdgH /= (double)cdgnb;
	m_threshold = (u8)roundf(cdgH * 2.0);
	
	fprintf(logfile, "TamanoirImgProc::%s:%d : Process histogram : cdgH = %g threshold=%d\n",
			__func__, __LINE__, cdgH, (int)m_threshold);
	
	m_progress = 45;
	
	// If image is really noisy, for example with B&W grain (or High sensitivity films), pre-process a 3x3 blur filter
	if( cdgH >= /*3.0*/ 2.8 ) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : GRAIN ERASER : Process 3x3 blur filter on input image ...\n",
			__func__, __LINE__);
		cvSmooth(grayImage, diffImage, 
			CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
			3, 3 );
		m_progress = 50;

		memcpy(grayImage->imageData, diffImage->imageData, 
			diffImage->widthStep*diffImage->height);

		unsigned long diffHisto2[256];
		memset(diffHisto2, 0, sizeof(unsigned long)*256);
		memset(diffImage->imageData, 0, diffImage->widthStep*diffImage->height);
		
		processDiff(m_FilmType, grayImage, medianImage, diffImage, varianceImage, diffHisto2, var_size);
	}
	m_progress = 60;
	
	if(m_threshold < 4)
		m_threshold = 4;
	
	
	unsigned char * diffImageBuffer = (unsigned char *)diffImage->imageData;
//	unsigned char * grayImageBuffer = (unsigned char *)grayImage->imageData;
	unsigned char * medianImageBuffer = (unsigned char *)medianImage->imageData;
	int width = diffImage->widthStep;
	int height = diffImage->height;
	int pos;

#if 0 // OLD VERSION WITH SEPAATE CRITERIONS => REPLACED WITH 1 CRITERION ONLY
	// THRESHOLD DIFF IMAGE
	switch(m_FilmType) {
	default:
		for(int r=0;r<height; r++) {
			pos = r * width;
			int posmax = pos + diffImage->width;
			//float * variance = (float *)(varianceImage->imageData + r*varianceImage->widthStep);
			for( ; pos < posmax; pos++//, variance++
				 ) {

				u8 diff = (u8)diffImageBuffer[pos];
				//float snr = (float)diff / (float)medianImageBuffer[pos];

				if( //snr > 0.05f &&
					diff > m_threshold
					//  &&diff > (2.f * (*variance))
					) { // Opaque pixels are whiter that median
					diffImageBuffer[pos] = DIFF_THRESHVAL;
				} else {
					if(diff >= tmmax(3, m_threshold-2))
						diffImageBuffer[pos] = DIFF_CONTOUR;
				//	u8 var_pix = (u8) ( (*variance) / (float)medianImageBuffer[pos] * 64.f);
				//	diffImageBuffer[pos] = var_pix;
				}
				//else if(diff >= tmmax(3, m_threshold-2)) {
				//		diffImageBuffer[pos] = DIFF_CONTOUR;
				//	} else diffImageBuffer[pos] = 0;

			}
		}
		break;
	case FILM_NEGATIVE: {
		for(pos=0; pos<width*height; pos++) 
		{
			u8 diff = (u8)diffImageBuffer[pos];
			// Try with signal/noise approach
			/* verison with absolute difference threshold *
			if( (diff >= m_threshold ) ) // Opaque pixels are whiter that median
				diffImageBuffer[pos] = DIFF_THRESHVAL;
			else
				if(diff >= tmmax(3, m_threshold-2))
					diffImageBuffer[pos] = DIFF_CONTOUR;
					*/
			//if( diff >= tmmax(3, m_threshold-2) ) // Opaque pixels are whiter that median
			{
				if( diff >= m_thrshold) {// (u8)tmmax(m_threshold, ((float)medianImageBuffer[pos] /8.f) ) ) { // Opaque pixels are whiter that median
					diffImageBuffer[pos] = DIFF_THRESHVAL;
				} else if(diff >= tmmax(3, m_threshold-2)) {
					diffImageBuffer[pos] = DIFF_CONTOUR;
				}
			}
		}

		}break;
	case FILM_POSITIVE: 
		for(pos=0; pos<width*height; pos++) 
		{
			u8 diff = (u8)diffImageBuffer[pos];
		
			if(diff >= m_threshold) 
				diffImageBuffer[pos] = DIFF_THRESHVAL;
			else
				if(diff >= tmmax(3, m_threshold-2))
					diffImageBuffer[pos] = DIFF_CONTOUR;
		}
		break;
	}
#else // NEW CRITERION
	for(int r=0;r<height; r++) {
		pos = r * width;
		int posmax = pos + diffImage->width;
		//float * variance = (float *)(varianceImage->imageData + r*varianceImage->widthStep);
		for( ; pos < posmax; pos++ ) {

			u8 diff = (u8)diffImageBuffer[pos];

			if(diff > m_threshold) {
				diffImageBuffer[pos] = DIFF_THRESHVAL;
			} else {
				if(diff >= tmmax(3, m_threshold-2)) {
					diffImageBuffer[pos] = DIFF_CONTOUR;
				}
			}
		}
	}
#endif
		// For debug, save image in temporary directory
		if(g_debug_savetmp) tmSaveImage(TMP_DIRECTORY "diffImage" IMG_EXTENSION, diffImage);
	m_progress = 70;
	
	// Do a close operation on diffImage
	if(1) {
		tmCloseImage(diffImage, medianImage, grayImage, 1);
			// For debug, save image in temporary directory
			if(g_debug_savetmp) tmSaveImage(TMP_DIRECTORY "diffImage-Closed" IMG_EXTENSION, medianImage);
		memcpy(diffImage->imageData, medianImage->imageData, medianImage->widthStep * medianImage->height);
	}

	m_progress = 80;
	
	fprintf(logfile, "TamanoirImgProc::%s:%d : init dust detector...\n", 
		__func__, __LINE__); fflush(stderr);
	
	
	
	
	// Difference 
	fprintf(logfile, "TamanoirImgProc::%s:%d : create grow image...\n", 
		__func__, __LINE__); fflush(stderr);
	if(!growImage)
		growImage = tmCreateImage(cvSize(originalImage->width, originalImage->height),
			IPL_DEPTH_8U, 1);
	
	
	
	fprintf(logfile, "TamanoirImgProc::%s:%d : create crop images....\n", 
		__func__, __LINE__); fflush(stderr);
	
	// Cropped image
	processingSize = cvSize( tmmin(blockSize.width, originalImage->width), 
							tmmin(blockSize.height, originalImage->height));

	allocCropped();

	m_lock = false;

	setResolution(m_dpi);
	
	m_progress = 100;
	
	return 0;
}


void TamanoirImgProc::allocCropped() {
	if(processingSize.width <= 0) return;
	if(processingSize.height <= 0) return;

	while( (processingSize.width % 4) > 0) {
		processingSize.width--;
	}
	while( (processingSize.height % 4) > 0) {
		processingSize.height--;
	}

	fprintf(stderr, "TamanoirImgProc::%s:%d : (re)alloc cropped images for processing : %d x %d\n",
		__func__, __LINE__, processingSize.width, processingSize.height);

	if(!cropImage) cropImage = tmCreateImage(processingSize,IPL_DEPTH_8U, 1);
	if(!tmpCropImage) tmpCropImage = tmCreateImage(processingSize,IPL_DEPTH_8U, 1);
	if(!cropColorImage) cropColorImage = tmCreateImage(processingSize,IPL_DEPTH_8U, originalImage->nChannels);
	if(!sobelImage) sobelImage = tmCreateImage(processingSize,IPL_DEPTH_16S, 1);
	if(!dilateImage) dilateImage = tmCreateImage(processingSize,IPL_DEPTH_8U, 1);
	if(!correctImage) correctImage = tmCreateImage(processingSize,IPL_DEPTH_8U, 1);

	if(originalImage->nChannels == 1) {
		correctColorImage = correctImage;
	} else {
		if(correctColorImage != correctImage && correctColorImage) {
			tmReleaseImage(&correctColorImage);
		}
		correctColorImage = tmCreateImage(processingSize,IPL_DEPTH_8U, originalImage->nChannels);
	}
}

/*
 Epson 4990 :
	N&B, 2400dpi, 400ISO: Tile 225,344 : diff_mean=2.24707 variance = 6.97754
	Provia 100, 2400 dpi, Tile 284,186 : diff_mean=1.09766 variance = 2.06445
	
 
*/

int TamanoirImgProc::findUniform(float * p_mean, float  * p_diff_mean, float * p_variance) {
	if(!grayImage) return -1;
	
	int tile_x, tile_y, tile_step = 32;
	u8 * buffer = (u8 *)grayImage->imageData;
	int pitch = grayImage->widthStep;
	int width = grayImage->width;
	int height = grayImage->height;
	
	for(tile_y = height/10; tile_y < height*9/10-tile_step*2; tile_y += tile_step ) {
		
		for(tile_x = width/10; tile_x < width*9/10-tile_step*2; tile_x += tile_step) {
			int x, y;
			int val_topleft = buffer[ tile_y * pitch + tile_x ];
			long val_mean = 0;
			bool resume = true;

			// Get mean of pixel values
			for(y=tile_y; y<tile_y+tile_step; y++) {
				
				for(x=tile_x; x<tile_x+tile_step; x++) {
					int val = buffer[y * pitch + x];
					if(abs(val - val_topleft) > 16) {
						// Break
						x = width;
						y = height;
						
						resume = false;
					}
					
					val_mean += val;
				}
			}
			
			// Get dispersion
			if(resume) {
				*p_mean = (float)val_mean / (float)(tile_step * tile_step);
				val_mean = (int)roundf((float)val_mean / (float)(tile_step * tile_step));
				if(val_mean > 64) { // Else we may be in noise
					long val_diff = 0;
					long val_diff2 = 0;
					// Get mean of pixel values
					for(y=tile_y; y<tile_y+tile_step; y++) {
						
						for(x=tile_x; x<tile_x+tile_step; x++) {
							int val = buffer[y * pitch + x];
							
							int diff = abs(val - val_mean);
							val_diff += diff;
							val_diff2 += diff * diff;
						}
					}
					
					float diff_mean = (float)val_diff / (float)(tile_step * tile_step);
					float variance = (float)val_diff2 / (float)(tile_step * tile_step);
					// If variance is low, choose this region
					
		//			fprintf(stderr, "Tile %d,%d : variance = %g", tile_x, tile_y, variance);
					if(diff_mean < 4) {
						fprintf(stderr, "Tile %d,%d : diff_mean=%g variance = %g\n", tile_x, tile_y, diff_mean, variance);
						
						if(diff_mean < 3) {
							*p_diff_mean = diff_mean;
							*p_variance = variance;
							return 0;
						}
					}
				}
			}
		}
	}
	
	return -1;
}

int TamanoirImgProc::saveFile(const char * filename) {
	int retry = 0;
	while(retry < 10 && m_lock) {
		sleep(1); retry++;
		fprintf(stderr, "[imgproc]::%s:%d : locked !!\n", __func__, __LINE__);
	}
	if(m_lock)
		return -1;
	
	m_lock = true;
	fprintf(logfile, "TamanoirImgProc::%s:%d : saving '%s'...\n", 
		__func__, __LINE__, filename);

		// Save for debug
		if(g_debug_savetmp) {
			tmSaveImage(TMP_DIRECTORY "growImage_final" IMG_EXTENSION, growImage);
			tmSaveImage(TMP_DIRECTORY "diffImage_final" IMG_EXTENSION, diffImage);
		}

	if(originalImage) {
		tmSaveImage(filename, originalImage);
		m_lock = false;
		return 0;
	}
	m_lock = false;
	return -1;
}



/*
 * Activate/desactivate hot pixels filtering
 */
bool TamanoirImgProc::setHotPixelsFilter(bool on) {
	if(m_hotPixels == on) return on;
	
	fprintf(logfile, "TamanoirImgProc::%s:%d : %s hot pixels filtering\n", __func__, __LINE__,
		(on ? "ACTIVATE" : "DESACTIVATE" ) );
	m_hotPixels = on ;
	
	
	m_dust_area_min = DUST_MIN_SIZE;
	if(m_hotPixels) {
		m_dust_area_min = HOTPIXEL_MIN_SIZE;
	}
	// Then re-process file
	fprintf(logfile, "TamanoirImgProc::%s:%d : re-preprocessing image...\n", 
		__func__, __LINE__);
	if(originalImage && on)
	{
		fprintf(logfile, "TamanoirImgProc::%s:%d : re-preprocessing image...\n", 
			__func__, __LINE__);
		preProcessImage();
		
		fprintf(logfile, "TamanoirImgProc::%s:%d : re-preprocessing image...\n", 
			__func__, __LINE__);
		firstDust();
	}
	fprintf(logfile, "TamanoirImgProc::%s:%d : re-preprocessing image...\n", 
			__func__, __LINE__);
		
	return m_hotPixels;
}


/*
 * Activate/desactivate trust on good corrections
 */
bool TamanoirImgProc::setTrustCorrection(bool on) {
	fprintf(logfile, "TamanoirImgProc::%s:%d : %s trust on good correction proposals (last=%d,%d)\n",
		__func__, __LINE__,
		(on ? "ACTIVATE" : "DESACTIVATE" ),
		m_last_correction.crop_x, m_last_correction.crop_y);
	
	if(m_trust != on) { // We changed to trust mode, return to last correction
		if(on) {
			m_seed_x = m_last_correction.crop_x + m_last_correction.rel_seed_x;
			m_seed_y = m_last_correction.crop_y + m_last_correction.rel_seed_y;
		}
	}
	
	// Then search for next
	nextDust ();
	
	m_trust = on;
	
	return m_trust;
}

void TamanoirImgProc::setFilmType(int type) {
	if(m_FilmType == type)
		return;
	
	m_FilmType = type; 
	
	// Then re-process file
	if(originalImage) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : re-preprocessing image...\n", 
			__func__, __LINE__);
		preProcessImage();
		
		fprintf(logfile, "TamanoirImgProc::%s:%d : go to first durst...\n", 
			__func__, __LINE__);
		firstDust();
		
		fprintf(logfile, "TamanoirImgProc::%s:%d : done.\n", 
			__func__, __LINE__);
		
	}
}

int TamanoirImgProc::setResolution(int dpi) {
	
	// Then re-process file
	fprintf(logfile, "TamanoirImgProc::%s:%d : set scan resolution to %d dpi\n", 
		__func__, __LINE__, dpi);

	if(originalImage) {
		// No need to pre-process again the image
		// We just reset the dust seeker
		// Allocate region growing structs
		m_dust_area_min = DUST_MIN_SIZE * dpi / 2400;
		if(m_hotPixels) {
			m_dust_area_min = HOTPIXEL_MIN_SIZE;
		}
		
		fprintf(logfile, "TamanoirImgProc::%s:%d : dust min area %d pixels^2\n", 
			__func__, __LINE__, m_dust_area_min);
		
		m_dust_area_max = 800 * dpi / 2400;
		if(dpi != m_dpi) {
			preProcessImage();
			firstDust();
		}
		
	}
	m_dpi = dpi;
	fprintf(logfile, "TamanoirImgProc::%s:%d : scan resolution = %d\n", 
		__func__, __LINE__, m_dpi);
	
	return m_dpi;
}


int TamanoirImgProc::setBlockSize(int w, int h) {
	if(w == blockSize.width && h == blockSize.height) {
		return 0;
	}
	blockSize = cvSize(w,h);
	firstDust();

	return 0;
}
CvSize TamanoirImgProc::getDisplayCropSize() {

	if(displayCropSize.width > 0 && displayCropSize.height > 0) {
		return displayCropSize;
	}

	// Else return default size for analysis
	return processingSize;
}

/* @brief Get progress in %age */
int TamanoirImgProc::getProgress() {
	if(!grayImage) return 0;
	if(!grayImage->height) return 0;

	return m_progress;
}

int TamanoirImgProc::firstDust() 
{
	// Reset last dust position
	m_seed_x = m_seed_y = 0;
	m_progress = 0;
	
	// Clear buffer of known dusts
	if(growImage)
		memset(growImage->imageData, 0, growImage->widthStep * growImage->height);
	
	// Disable trust for first image
	bool old_trust = m_trust;
	int ret = nextDust();
	m_trust = old_trust;
	
	return ret;
}

int TamanoirImgProc::nextDust() {
	int x, y;
	int pos;
	if(!diffImage || !growImage) return -1;
	if(!diffImage->imageData || !growImage->imageData) return -1;

	int retry = 0;
	while(retry < 10 && m_lock) {
		sleep(1); retry++;
		fprintf(stderr, "[imgproc]::%s:%d : locked !!\n", __func__, __LINE__);
	}
	if(m_lock)
		return -1;
	
	m_lock = true;
	
	
	int pitch = diffImage->widthStep;
	int width = diffImage->width;
	int height = diffImage->height;
	
	u8 * diffImageBuffer = (u8 *)diffImage->imageData;
	u8 * growImageBuffer = (u8 *)growImage->imageData;
	
	if(g_debug_imgverbose)
		fprintf(logfile, "TamanoirImgProc::%s:%d : searching seed from %d,%d ...\n", 
						__func__, __LINE__, m_seed_y, m_seed_x);
	
	memset(&m_correct, 0, sizeof(t_correction));
	
	memset(&m_lastDustComp, 0, sizeof(CvConnectedComp));
	
	m_correct.copy_width = -1; // Clear stored correction
	
	
	for(y = m_seed_y; y<height-1; y++) {
		pos = y * pitch + m_seed_x;
		for(x = m_seed_x; x<width-1; x++, pos++) {
			if(!growImageBuffer[pos]) {
				if(diffImageBuffer[pos] == DIFF_THRESHVAL ) {
					// Grow region here if the region is big enough
					if(//diffImageBuffer[pos+pitch] == DIFF_THRESHVAL
							1 || m_hotPixels)
					{
						int return_now = findDust(x,y);

						if(return_now > 0) {
							m_lock = false;

							return return_now;
						}
					}
				} // else clear diffimage because this is not a dust
				else {
					//diffImageBuffer[pos] = 0;
				}
			}
		}
		
		
		m_seed_x = 0;
	}
	
	m_lock = false;
	return 0;
}

int TamanoirImgProc::findDust(int x, int y) {

	return findDust(x,y, &m_correct);
}
#ifndef WIN32
#define MUTEX_LOCK(m) pthread_mutex_lock(m)
#define MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
#else
// FIXME
#define MUTEX_LOCK(m)
#define MUTEX_UNLOCK(m)
#endif

int TamanoirImgProc::findDust(int x, int y, t_correction * pcorrection) {
	if(!growImage) return -1; // Loading is not finished

	if(x<0 || x>=diffImage->width
	   || y<0 || y>=diffImage->height ) {
		return -1;
	}

	MUTEX_LOCK(&mutex);

	CvConnectedComp connect;
	memset(&connect, 0, sizeof(CvConnectedComp));
	
	u8 * diffImageBuffer = (u8 *)diffImage->imageData;
	u8 * growImageBuffer = (u8 *)growImage->imageData;

	// If crop size changed, we change the analysis size to provide centered cropped images to GUI
	if(displayCropSize.width > 0 && displayCropSize.height > 0) {
		if(abs(processingSize.width - displayCropSize.width)>=4
		   || abs(processingSize.height - displayCropSize.height)>=4) {
			fprintf(stderr, "TamanoirImgProc::%s:%d : changing crop size %dx%d => %dx%d\n",
					__func__, __LINE__, processingSize.width, processingSize.height,
					displayCropSize.width, displayCropSize.height);

			purgeCropped();
			processingSize = displayCropSize;

			allocCropped();
		}
	}


	bool force_search = (pcorrection != &m_correct); // force search
	if(force_search) {
		int lw = growImage->width;
		int lpitch = growImage->widthStep;
		int lh = growImage->height;
		
		// search a dust for region growing in
		int xleft = x - pcorrection->copy_width/2;
		if(xleft<0) xleft = 0; 
		else if(xleft >= lw) return 0;
		
		int xright = xleft + pcorrection->copy_width;
		if(xright<0) return 0; 
		else if(xright >= lw) xright = lw-1;
		
		if(xright <= xleft) return 0;
		
		int ytop  = y - pcorrection->copy_height/2;
		if(ytop<0) ytop = 0; 
		else if(ytop >= lh) return 0;
		
		int ybottom = y + pcorrection->copy_height/2;
		if(ybottom<0) return 0; 
		else if(ybottom >= lh) ybottom = lh-1;
		
		if( diffImageBuffer[y * diffImage->widthStep + x] != DIFF_THRESHVAL) {
			// No difference at this point
			// return 0;
			int closest_dist = 20*20 ;
			
			for(int ly=ytop; ly<=ybottom; ly++) {
				for(int lx=xleft; lx<=xright; lx++) {
					if(diffImageBuffer[ ly*lpitch + lx] == DIFF_THRESHVAL) {
						int dx = x - lx;
						int dy = y - ly;
						int dist = sqrt(dx*dx+dy*dy);
						if(dist < closest_dist) {
							closest_dist = dist;
							x = lx; y = ly;
						}
					}
				}
			}
		}
		
		
		// Clear gnown buffer to enable second region growing
		
		for(int ly=ytop; ly<ybottom; ly++) {
			memset(growImageBuffer + ly*lpitch + xleft, 0, xright - xleft);
		}
	}
	


	// Process a region growing 
	tmGrowRegion( diffImageBuffer, growImageBuffer, 
		diffImage->widthStep, diffImage->height, 
		x, y, 
		DIFF_THRESHVAL,
		255,
		&connect );
	if(g_debug_imgverbose > 1) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : found seed at %d,%d => area=%d,%d+%dx%d surf=%d  min=%d <? %d <? max=%d...\n", 
				__func__, __LINE__, x, y,
				(int)connect.rect.x, (int)connect.rect.y, (int)connect.rect.width, (int)connect.rect.height,
				 (int)connect.area,
				 m_dust_area_min, (int)connect.area, m_dust_area_max);
		fprintf(logfile, "TamanoirImgProc::%s:%d : => %d,%d+%dx%d...\n", 
				__func__, __LINE__,
				connect.rect.x,connect.rect.y,
				connect.rect.width,
					connect.rect.height);
	}

	//if(connect.area <=0) return 0;
	bool is_a_dust = ( (connect.area >= m_dust_area_min &&
	   connect.area < m_dust_area_max)
	   || force_search
	   );


	// Crop image and search for a dust on cropped images
	if( is_a_dust )
	{
		int connect_area = (int)connect.area;
		int connect_width = connect.rect.width;
		int connect_height = connect.rect.height;
		
		
		if(!force_search) {
			// There a seed here !!!
			m_seed_x = x+1;
			m_seed_y = y;
		
			
		
			/** Update stats */
			m_dust_stats.nb_grown++;
			if(connect_area<STATS_MAX_SURF) {
				m_dust_stats.grown_size[connect_area]++;
			}
		}
		
		
		// Crop area geometry
		int crop_width = cropImage->width;
		int crop_height = cropImage->height;
		
		int crop_x = connect.rect.x + connect.rect.width/2 - crop_width/2;
		int crop_y = connect.rect.y + connect.rect.height/2 - crop_height/2;
		
		// If forced, we keep the original cropping
		if(force_search) {
			crop_x = pcorrection->crop_x;
			crop_y = pcorrection->crop_y;
			if(connect_width <=0 || connect_height<=0) {
				connect_width = pcorrection->copy_width;
				connect_height = pcorrection->copy_height;
			}
		}


		if(crop_x < 0) 	
			crop_x = 0;
		if(crop_x + crop_width >= originalImage->width)
			crop_x = originalImage->width - crop_width-1;
		
		if(crop_y < 0) 		crop_y = 0;
		if(crop_y + crop_height >= originalImage->height)
			crop_y = originalImage->height - crop_height-1;
		
		
		
		// Real position of dust in cropped image
		int crop_center_x = x - crop_x;
		int crop_center_y = y - crop_y;
		
		if(g_debug_imgverbose > 1)
			fprintf(logfile, "TamanoirImgProc::%s:%d : grown dust at %d,%d+%dx%d => %d,%d in cropped image...\n", 
				__func__, __LINE__,
				connect.rect.x,connect.rect.y,
				connect.rect.width, connect.rect.height,
				crop_center_x, crop_center_y
				);


		// ------- Cropped images are used to lower the memory needs / and for GUI display of correction proposals
		// Re-greow region in cropped image
		tmCropImage(diffImage, cropImage,
					crop_x, crop_y);
		if(g_debug_savetmp) {
			tmSaveImage(TMP_DIRECTORY "a-diffImage" IMG_EXTENSION, 
						cropImage);
		}
		
		// clear grow buffer
		memset(correctImage->imageData, 0,
			correctImage->widthStep * correctImage->height);
		
		// Grow again region in cropped image
		CvConnectedComp crop_connect;
		tmGrowRegion(
			(u8 *)cropImage->imageData,
			(u8 *)correctImage->imageData,
			cropImage->widthStep, cropImage->height, 
			crop_center_x, crop_center_y, 
			DIFF_THRESHVAL,
			255,
			&crop_connect);
		
		if(g_debug_imgverbose > 1)
			fprintf(logfile, "TamanoirImgProc::%s:%d : => grown dust at %d,%d+%dx%d surf=%d.\n", 
				__func__, __LINE__,
				crop_connect.rect.x, crop_connect.rect.y,
				crop_connect.rect.width, crop_connect.rect.height,
				(int)crop_connect.area
				);
		if( (crop_connect.area >= m_dust_area_min &&
		   crop_connect.area < m_dust_area_max)
		   || ( force_search && crop_connect.area>0))
		{
			/* LOW LEVEL DEBUG ONLY : WHEN NOT COMENTED, IT DESTROY THE INPUT IMAGE !! 
			if(g_debug_savetmp)
			{
				cvRectangle(grayImage, 
						cvPoint(connect.rect.x-connect.rect.width,
							connect.rect.y-connect.rect.height), 
						cvPoint(connect.rect.x+connect.rect.width,
							connect.rect.y+connect.rect.height),
						cvScalarAll(0), 1);
				tmCropImage(grayImage, cropImage, 
						connect.rect.x+connect.rect.width/2,
						connect.rect.y+connect.rect.height/2);
			
				tmSaveImage(TMP_DIRECTORY "cropImage" IMG_EXTENSION, cropImage);
			}*/
			
			
			
			
			// Check if copy is not along a contour
			//int dx = abs(copy_src_x - copy_dest_x);
			//int dy = abs(copy_src_y - copy_dest_y);
			//if(dx < dy) // Check if we are not copying while following a border
			//	cvSobel(correctImage, sobelImage, 1, 0, 5);
			//else
			//	cvSobel(correctImage, sobelImage, 0, 1, 5);

			// Crop gray image into cropImage
			tmCropImage(grayImage, 
				tmpCropImage,
				crop_x, crop_y);
			
			// Do a dilatation around the grown
			tmDilateImage(correctImage, dilateImage);

			// => this dilated image will be used as mask for correlation search
			// but we have to fill the center of the dust 
			//	A full circle will only give us a empty circle : O
			unsigned char * dilateImageBuffer = (unsigned char *)dilateImage->imageData;
			unsigned char * cropGrayBuffer = (unsigned char *)tmpCropImage->imageData;

			int r,c, histoDust[512];
			memset(histoDust, 0, 512*sizeof(int));

			int neighbour_rmin = tmmax(crop_connect.rect.y - crop_connect.rect.height, 0);
			int neighbour_rmax = tmmin(crop_connect.rect.y+2*crop_connect.rect.height, dilateImage->height);
			int neighbour_cmin = tmmax(crop_connect.rect.x - crop_connect.rect.width, 0);
			int neighbour_cmax = tmmin(crop_connect.rect.x+2*crop_connect.rect.width, dilateImage->width);

			for(r=crop_connect.rect.y;r<crop_connect.rect.y+crop_connect.rect.height; r++) {
				int crop_pos = r*tmpCropImage->widthStep + crop_connect.rect.x;
				for(c=crop_connect.rect.x; c<crop_connect.rect.x+crop_connect.rect.width; c++,crop_pos++) {
					if( dilateImageBuffer[crop_pos] == 255) {
						int gray = (int)cropGrayBuffer[crop_pos];
						// Difference between image and median
						histoDust[ gray ]++;
					}
				}
			}

			// get min/max of pixel values
			int min_dust = 255;
			int max_dust = -255;
			double sum_dust = 0.;
			int nb_dust = 0;
			for(int h=0; h<256; h++) {
				if(histoDust[h]>0)
				{
					sum_dust += (double)(histoDust[h]*h);
					nb_dust += histoDust[h];
					if(h<min_dust) min_dust=h;
					if(h>max_dust) max_dust=h;
				}
			}
			if(nb_dust>0) {
				sum_dust /= (double)nb_dust;
			}
			// Fill the center of the dust
			//fprintf(stderr, "::%s:%d : min/max = %d / %d\n", __func__, __LINE__, min_dif, max_dif);
			for(r=crop_connect.rect.y;r<crop_connect.rect.y+crop_connect.rect.height; r++) {
				int crop_pos = r*tmpCropImage->widthStep + crop_connect.rect.x;
				for(c=crop_connect.rect.x; c<crop_connect.rect.x+crop_connect.rect.width; c++,crop_pos++) {
					if(cropGrayBuffer[crop_pos]>=min_dust
						&& cropGrayBuffer[crop_pos]<=max_dust) {
						dilateImageBuffer[crop_pos] = 255;
					}
				}
			}

			int best_correl = 100;
			// Check if this region is not the film grain by cheching the local
			// variance and difference
			if(is_a_dust
//			   && !force_search
			   ) {
				int histoNeighbour[512];
				// Update dust info
				memset(histoNeighbour, 0, 512*sizeof(int));
				memset(histoDust, 0, 512*sizeof(int));

				if(force_search) {
					fprintf(stderr, "::%s:%d : Dust : %d,%d + %dx%d\n",
							__func__, __LINE__,
							(int)crop_connect.rect.x, (int)crop_connect.rect.y,
							(int)crop_connect.rect.width, (int)crop_connect.rect.height
							);
					fprintf(stderr, "\tDust : min/max/mean = %d / %d / %g (nb=%d)\n",
							min_dust, max_dust, sum_dust, nb_dust);
				}

				for(r=neighbour_rmin;r<neighbour_rmax; r++) {
					int crop_pos = r*tmpCropImage->widthStep + neighbour_cmin;
					for(c=neighbour_cmin; c<neighbour_cmax;
							c++,crop_pos++) {
						int gray = (int)cropGrayBuffer[crop_pos];
						if(dilateImageBuffer[crop_pos]) {
							// Difference between image and median
							histoDust[ gray ]++;
						} else {
							histoNeighbour[ gray ]++;
						}
					}
				}


				min_dust = 255;
				max_dust = -255;
				sum_dust = 0.;
				nb_dust = 0;
				for(int h=0; h<256; h++) {
					if(histoDust[h]>0)
					{
						sum_dust += (double)(histoDust[h]*h);
						nb_dust += histoDust[h];
						if(h<min_dust) { min_dust=h; }
						if(h>max_dust) { max_dust=h; }
					}
				}
				if(nb_dust>0) {
					sum_dust /= (double)nb_dust;
				}
				
				int min_neighbour = 255;
				int max_neighbour = 0;
				double sum_neighbour = 0.;
				int nb_neighbour = 0;
				for(int h=0; h<256; h++) {
					if(histoNeighbour[h]>0) {
						sum_neighbour += (double)(histoNeighbour[h]*h);
						nb_neighbour += histoNeighbour[h];
						if( h < min_neighbour ) min_neighbour = h;
						if( h > max_neighbour ) max_neighbour = h;
					}
				}
				if(nb_neighbour>0) {
					sum_neighbour /= (double)nb_neighbour;
				}

				best_correl = max_neighbour - min_neighbour;

				if(force_search) {
					fprintf(stderr, "\tDust : min/max/mean = %d / %d / %g (nb=%d)\n",
							min_dust, max_dust, sum_dust, nb_dust);
					fprintf(stderr, "\t Neighbour : min/max/mean = %d / %d / %g / tolerance=%d\n",
							min_neighbour, max_neighbour, sum_neighbour, best_correl);
				}

				// test if colour is different enough
				if(!force_search) {
					switch(m_FilmType) {
					default:
					case FILM_UNDEFINED:
						if(fabs(sum_neighbour - sum_dust) <= 10 ) { //m_threshold
							// no difference visible => not a dust
							is_a_dust = false;
						}
						break;
					case FILM_POSITIVE:
						if( sum_dust > sum_neighbour - 10 ) { //m_threshold
							// not darker than neighbourhood  => not a dust
							is_a_dust = false;
						}
						break;
					case FILM_NEGATIVE:
						if( sum_dust < sum_neighbour + 10 ) { //m_threshold
							// not lighter than neighbourhood  => not a dust
							is_a_dust = false;
						}
						break;
					}
				}
			}
			
			
			// Crop median image
			tmCropImage(medianImage,
				tmpCropImage,
				crop_x, crop_y);

			// Process search around the dust in original image
			// Use center x,y - width,height
			connect_width = crop_connect.rect.width;
			connect_height = crop_connect.rect.height;
			
			
			
			int connect_center_x = crop_connect.rect.x + connect_width/2;
			int connect_center_y = crop_connect.rect.y + connect_height/2;
			
			// If the search if forced, limit search to input size
			if(force_search) {
				if( (x-crop_x) < connect_center_x-connect_width/2
					|| (x-crop_x) > connect_center_x+connect_width/2
					|| (y-crop_y) < connect_center_y-connect_height/2
					|| (y-crop_y) > connect_center_y+connect_height/2
					) {
					fprintf(stderr, "::%s:%d : FORCE => moving center  %d, %d +%dx%d => ",
							__func__, __LINE__,
							connect_center_x, connect_center_y,
							connect_width, connect_height
							);
					// FIXME : Maybe not a good ideao to limit position of search
					// because this point can be at the border of the dust
					// so the search rectangle may be too small
					connect_center_x = x - crop_x;
					connect_center_y = y - crop_y;
					connect_width = pcorrection->copy_width;
					connect_height = pcorrection->copy_height;

					fprintf(stderr, "%d, %d +%dx%d\n",
							connect_center_x, connect_center_y,
							connect_width, connect_height
							);

				}
			}
			
			while(connect_width < 5 
				&& (connect_center_x+connect_width)<crop_width-3) {
				connect_width += 2;
			}
			while(connect_height < 5 
				&& (connect_center_y+connect_height)<crop_height-3) {
				connect_height += 2;
			}
			

			
			// Crop original image
			if(origBlurredImage) { // If original is grainy it has been blurred
				tmCropImage(origBlurredImage,
					correctColorImage,
					crop_x, crop_y);
			} else {
				tmCropImage(originalImage,
					correctColorImage,
					crop_x, crop_y);
			}

			int copy_dest_x=0, copy_dest_y=0,
				copy_src_x=0, copy_src_y=0,
				copy_width=0, copy_height=0;


			// Find best proposal in originalImage
			int ret = tmSearchBestCorrelation(
				correctColorImage, dilateImage, 
				connect_center_x, connect_center_y,
				connect_width, connect_height, 
				&copy_dest_x, &copy_dest_y,
				&copy_src_x, &copy_src_y,
				&copy_width, &copy_height,
				&best_correl);
			if(g_debug_imgverbose > 1 || force_search) {

				int maxdiff = 0, nbdiff =  0;
				float l_dist = tmCorrelation(correctColorImage, correctColorImage,
						dilateImage,
						connect_center_x, connect_center_y,
						pcorrection->rel_src_x, pcorrection->rel_src_y,
						connect_width, connect_height,
						100,
						&maxdiff,
						&nbdiff
						);

				fprintf(stderr, "\tTamanoirImgProc::%s:%d : grown:%d,%d+%dx%d "
						"=> ret=%d best_correl=%d copy from %d,%d => to %d,%d \n"
						"\tdiff (force) : dist=%g / maxdiff=²%d\n",
					__func__, __LINE__,
					connect_center_x, connect_center_y,
					connect_width, connect_height,
					ret, best_correl,
					copy_src_x, copy_src_y,
					copy_dest_x, copy_dest_y,
					l_dist, maxdiff);
			}
			if(ret>0) {
				m_lastDustComp = connect; 
				u8 return_now = 1;
				
				/** Update stats */
				m_dust_stats.nb_grown_replaced++;
				if(connect_area<STATS_MAX_SURF) {
					m_dust_stats.grown_size_replaced[connect_area]++;
				}
				
				// If the source and the destination are in the same extended grown region,
				
				// Re-greow region in cropped image
				tmCropImage(diffImage, cropImage, 
							crop_x, crop_y);
				// clear grow buffer
				memset(correctImage->imageData, 0, 
					correctImage->widthStep * correctImage->height);
				
				// Grow again region in cropped image
				CvConnectedComp ext_connect;
				tmGrowRegion(
					(u8 *)cropImage->imageData, 
					(u8 *)correctImage->imageData, 
					cropImage->widthStep, cropImage->height, 
					crop_center_x, crop_center_y, 
					DIFF_CONTOUR,
					42, // The answer of Life, God, and 
					&ext_connect);
				
				float src_failure = tmNonZeroRatio(correctImage,
						copy_src_x, copy_src_y,
						copy_width, copy_height,
						copy_width, copy_height,0,0,// Exclusion ROI
						42);
				if(src_failure) {
					
					//fprintf(stderr, "")
					return_now = 0;
				}
				
				// Store correction in full image buffer
#ifdef SIMPLE_VIEWER
				if(!force_search)
#else
				if(1)
#endif
				{
//					pcorrection->dest_x = crop_x + copy_dest_x;
//					pcorrection->dest_y = crop_y + copy_dest_y;
					
					pcorrection->copy_width = copy_width;
					pcorrection->copy_height = copy_height;
				} else {


					// Force copy dest to be where we forced the seed
					// Centered dest determined by correlation
					int centered_dest_x = copy_dest_x - copy_width/2; 
					int centered_dest_y = copy_dest_y - copy_height/2; 
					
					// We forced connect_width and connect_height
					// => the copy_dest_x are top-left of a region of size copy_width x copy_height
					// -> the center of dest must me moved by (connect_width - copy_width)/2
					int offset_x = pcorrection->rel_dest_x - centered_dest_x;
					int offset_y = pcorrection->rel_dest_y - centered_dest_y;
					
					copy_dest_x = pcorrection->rel_dest_x ;
					copy_dest_y = pcorrection->rel_dest_y ;
					
					copy_src_x += offset_x;
					copy_src_y += offset_y;
				}
				
				// Relative storage
				pcorrection->crop_x = crop_x;
				pcorrection->crop_y = crop_y;
				pcorrection->rel_src_x = copy_src_x;
				pcorrection->rel_src_y = copy_src_y;
				pcorrection->rel_dest_x = copy_dest_x;
				pcorrection->rel_dest_y = copy_dest_y;
				
				pcorrection->rel_seed_x = crop_center_x;
				pcorrection->rel_seed_y = crop_center_y;
				
				// Update dest
//				pcorrection->src_x = pcorrection->crop_x + pcorrection->rel_src_x;
//				pcorrection->src_y = pcorrection->crop_y + pcorrection->rel_src_y;
				pcorrection->area = connect_area;
				
				// Fill size statistics
				pcorrection->area = (int)connect.area;
				pcorrection->width_mm = 25.4f * connect.rect.width / m_dpi;
				pcorrection->height_mm = 25.4f * connect.rect.height / m_dpi;
				
				
				if(m_trust && return_now) {
					// Check if correction area is in diff image
					int left = tmmin(copy_src_x, copy_dest_x);
					int right = tmmax(copy_src_x + copy_width, copy_dest_x + copy_width);
					int top = tmmin(copy_src_y, copy_dest_y);
					int bottom = tmmax(copy_src_y + copy_height, copy_dest_y + copy_height);
					
					float fill_failure = tmNonZeroRatio(diffImage,
						crop_x + left, crop_y + top,
						right - left, bottom - top,
						crop_x + copy_dest_x, crop_y + copy_dest_y,
						copy_width, copy_height
						);
					//	crop_x + left, crop_y + top,
					//	right - left, bottom - top);
					
					//fprintf(stderr, "[imgproc] %s:%d : fill_failure=%g\n", 
					//	__func__, __LINE__, fill_failure);
					
					// If we can trust the correction proposal, let's correct know !
					if( best_correl < TRUST_CORREL_MAX
						&& pcorrection->area < TRUST_AREA_MAX
						&& fill_failure == 0) {
						fprintf(stderr, "[imgproc] %s:%d : m_trust=%c & fill_failure=%g\n", 
								__func__, __LINE__, 
								m_trust?'T':'F', fill_failure);
						forceCorrection(*pcorrection, true);
						return_now = 0;
					} 
					
				}
				
					
				if(return_now) {
					
					if(g_evaluate_mode) {
						// Evaluate if dust is already known
						bool known = testKnownDust(m_correct, originalImage->width, originalImage->height);
						
						fprintf(logfile, "TamanoirImgProc::%s:%d : Evaluate : dust at %d,%d+%dx%d "
							"=> %d,%d in cropped image => known = %c...\n", 
							__func__, __LINE__,
							connect.rect.x,connect.rect.y,
								connect.rect.width, connect.rect.height,
							crop_center_x, crop_center_y, 
							(known ? 'T':'F')
							);
						
						if(known) {
							m_perf_stats.true_positive++;
						} else {
							m_perf_stats.false_positive++;
						}
					}
					
					//fprintf(stderr, "[imgproc] %s:%d : return 1\n", __func__, __LINE__);
					MUTEX_UNLOCK(&mutex);
					return 1;
				}
			} else {
				if(!force_search) m_perf_stats.no_proposal++;
				// DEBUG FUNCTIONS
				if(g_debug_imgverbose > 1) {
					fprintf(logfile, "TamanoirImgProc::%s:%d : dust at %d,%d+%dx%d "
						"=> %d,%d in cropped image => no replace candidate (best=%d)...\n", 
						__func__, __LINE__,
						connect.rect.x,connect.rect.y,
						connect.rect.width, connect.rect.height,
						crop_center_x, crop_center_y, best_correl
						);
					if(g_debug_savetmp)
					{
						tmSaveImage(TMP_DIRECTORY "failed-dilateImage" IMG_EXTENSION,  
								dilateImage);
					}
				}
				
				// Mark failure on displayImage
				if(g_debug_imgoutput && displayImage) {
					int disp_x = (crop_x + crop_connect.rect.x) * displayImage->width / grayImage->width;
					int disp_y = (crop_y + crop_connect.rect.y) * displayImage->height / grayImage->height;
					int disp_w = crop_connect.rect.width * displayImage->width / grayImage->width;
					int disp_h = crop_connect.rect.height * displayImage->height / grayImage->height;
					tmMarkFailureRegion(displayImage, 
							disp_x, disp_y, disp_w, disp_h, COLORMARK_FAILED);
					tmMarkFailureRegion(displayBlockImage,
							disp_x, disp_y, disp_w, disp_h, COLORMARK_FAILED);
				}
				
				if(g_debug_imgoutput) {

					tmMarkFailureRegion(originalImage, 
						crop_x + crop_connect.rect.x, crop_y + crop_connect.rect.y,
						crop_connect.rect.width, crop_connect.rect.height, 120);
					tmMarkFailureRegion(diffImage, 
						crop_x + crop_connect.rect.x, crop_y + crop_connect.rect.y,
						crop_connect.rect.width, crop_connect.rect.height, 120);
				}
				// END OF DEBUG FUNCTIONS
				
			}
			
			// DEBUG FUNCTIONS
			if(g_option_stopoguess || g_debug_correlation) {
				// Diff image for display in GUI
				tmCropImage(diffImage, 
						cropImage, 
						crop_x, crop_y);
				
				tmCropImage(originalImage, cropColorImage, 
						crop_x, crop_y);
				MUTEX_UNLOCK(&mutex);
				return 1;
			}
			// END OF DEBUG FUNCTIONS
			
			// For debug, save image in temporary directory
			//tmSaveImage(TMP_DIRECTORY "diffImageGrown.pgm", growImage);
		} else {
			if(g_debug_imgverbose > 1) {
				fprintf(logfile, "TamanoirImgProc::%s:%d : dust regrow=> %d,%d+%dx%d "
					"=> %d,%d in cropped image => PROBLEM : NOT FOUND WITH SAME THRESHOLD...\n", 
					__func__, __LINE__,
					crop_connect.rect.x, crop_connect.rect.y,
					crop_connect.rect.width, crop_connect.rect.height,
					crop_center_x, crop_center_y
					);
			}
			
			if(g_debug_imgoutput) {
				tmMarkFailureRegion(originalImage, 
					crop_x + crop_connect.rect.x, crop_y + crop_connect.rect.y,
					crop_connect.rect.width, crop_connect.rect.height, 32);
			}
		}
	}



	if(!is_a_dust) {
		/*fprintf(stderr, "TamanoirImgProc::%s:%d : not a dust @ %d,%d area=%g\n", __func__, __LINE__,
				x, y, connect.area);*/
		// Clear grown region with neutral mask
		tmEraseRegion( growImage, diffImage,
			x, y,
			255 );
	}
	MUTEX_UNLOCK(&mutex);
	return 0;
}


void TamanoirImgProc::cropViewImages() {
	cropCorrectionImages(m_correct);
}


void TamanoirImgProc::cropCorrectionImages(t_correction correction) {
	if(correction.copy_width <= 0) return;
	
	if(!originalImage) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : no originalImage processingSize %dx%d\n",
				__func__, __LINE__, processingSize.width, processingSize.height);
		return;
	}
	if(!diffImage) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : no diffImage processingSize %dx%d\n",
				__func__, __LINE__, processingSize.width, processingSize.height);
		return;
	}
	if(processingSize.width * processingSize.height <=0) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : error in processingSize %dx%d\n",
				__func__, __LINE__, processingSize.width, processingSize.height);
		return;
	}
	
	CvSize cropSize = processingSize;
	if(displayCropSize.width > 0 && displayCropSize.height > 0) {
		cropSize = displayCropSize;
	}

	if(disp_cropColorImage) {
		cropSize = cvSize( disp_cropColorImage->width, disp_cropColorImage->height );
	}
	if(correction.crop_width > 0 && correction.crop_height > 0) {
		if(cropSize.width != correction.crop_width || cropSize.height != correction.crop_height) {
			purgeDisplay();
			displayCropSize = cropSize = cvSize( correction.crop_width, correction.crop_height );
		}
	}
	
	
	// Allocate images
	if(!disp_cropColorImage) {
		disp_cropColorImage = tmCreateImage(cropSize, IPL_DEPTH_8U, originalImage->nChannels);
	}
	if(!disp_correctColorImage) {
		disp_correctColorImage = tmCreateImage(cropSize,IPL_DEPTH_8U, originalImage->nChannels);
	}
	if(g_debug_savetmp) {// force debug
		m_show_crop_debug = true;
	}
	if(!disp_cropImage && m_show_crop_debug) {
		disp_cropImage = tmCreateImage(cropSize,IPL_DEPTH_8U, 1);
	}
	
	if(0) { // Sobel : OBSOLETE DISPLAY
		if(!disp_dilateImage) 
		{
			disp_dilateImage = tmCreateImage(cropSize, IPL_DEPTH_16S, 1);
			memset( disp_dilateImage->imageData, 0, 
					disp_dilateImage->widthStep * disp_dilateImage->height * sizeof(short));
		}
		
		// Get Sobel
		tmCropImage(grayImage, disp_cropImage, 
					correction.crop_x, correction.crop_y);
		int dx = abs(correction.rel_src_x - correction.rel_dest_x);
		int dy = abs(correction.rel_src_y - correction.rel_dest_y);
		if(dx < dy) // Check if we are not copying while following a border
			cvSobel(disp_cropImage, disp_dilateImage, 1, 0, 5);
		else
			cvSobel(disp_cropImage, disp_dilateImage, 0, 1, 5);
	} else if(m_show_crop_debug) { // Grown region
		
		if(!disp_dilateImage) {
			disp_dilateImage = tmCreateImage(cropSize, IPL_DEPTH_8U, 1);
		} else {
			memset( disp_dilateImage->imageData, 0, 
					disp_dilateImage->widthStep * disp_dilateImage->height );
		}	
		// Get grown region
		tmCropImage(diffImage, disp_cropImage, 
					correction.crop_x, correction.crop_y);
		
		CvConnectedComp ext_connect;
		tmGrowRegion(
					(u8 *)disp_cropImage->imageData, 
					(u8 *)disp_dilateImage->imageData, 
					cropImage->widthStep, cropImage->height, 
					correction.rel_seed_x, correction.rel_seed_y, 
					DIFF_THRESHVAL,
					255,
					&ext_connect);
		// Top-right = dilatation image of dust
		if(g_debug_savetmp)
		{
			tmSaveImage(TMP_DIRECTORY "b-dilateImage" IMG_EXTENSION,
				disp_dilateImage);
		}
	}
	
	// Bottom-right: Diff image for display in GUI
	if(disp_cropImage) {
		tmCropImage(diffImage,
			disp_cropImage, 
			correction.crop_x, correction.crop_y);
		if(g_debug_savetmp)
		{
			tmSaveImage(TMP_DIRECTORY "z-diffImageGray" IMG_EXTENSION,
				disp_cropImage);
		}
	}
	
	// Top-left on GUI : Original image for display in GUI
	tmCropImage(originalImage, disp_cropColorImage, 
				correction.crop_x, correction.crop_y);
	
	if(g_debug_savetmp)
	{
		tmSaveImage(TMP_DIRECTORY "a-cropImage" IMG_EXTENSION, 
			disp_cropColorImage);
		
		
		tmSaveImage(TMP_DIRECTORY "z-cropImageGray" IMG_EXTENSION,
			disp_cropImage);
	}
	
	
	// Bottom-Left
	// If we use a blurred version for searching,
	//	update cropped color with original this time
	cvCopy(disp_cropColorImage, disp_correctColorImage);

	// For grayscaled images, limit color to 251
	for(int r=0; r<disp_cropColorImage->height; r++) {
		u8 * buf = (u8 *)(disp_cropColorImage->imageData + disp_cropColorImage->widthStep * r);
		for(int c=0; c<disp_cropColorImage->width; c++)
			if(buf[c]>251) buf[c]=251;
	}

	// Clone image region 
	tmCloneRegion(disp_cropColorImage, 
		correction.rel_dest_x, correction.rel_dest_y, // dest
		correction.rel_src_x, correction.rel_src_y, // src
		correction.copy_width, correction.copy_height,
		disp_correctColorImage
		);
	
	tmMarkCloneRegion(disp_cropColorImage, 
		correction.rel_dest_x, correction.rel_dest_y, // dest
		correction.rel_src_x, correction.rel_src_y, // src
		correction.copy_width, correction.copy_height,
		false // mark move dest
		);
	
	if(g_debug_savetmp)
	{
		tmSaveImage(TMP_DIRECTORY "c-correctImage" IMG_EXTENSION, 
			disp_correctColorImage);
	}
	

	
	
	
	// Main windows
	if(displayBlockImage) {
		cvCopy(displayImage, displayBlockImage);

		// Return when we can propose something for correction
		// Mark current on displayImage
		int disp_x = (correction.crop_x + correction.rel_dest_x) * displayImage->width / grayImage->width;
		int disp_y = (correction.crop_y + correction.rel_dest_y) * displayImage->height / grayImage->height;
		int disp_w = correction.copy_width * displayImage->width / grayImage->width;
		int disp_h = correction.copy_height * displayImage->height / grayImage->height;
		
		tmMarkFailureRegion(displayBlockImage,
					disp_x, disp_y, disp_w, disp_h, COLORMARK_CURRENT);

		if(correction.crop_width > 0 && correction.crop_height > 0) {
			disp_x = (correction.crop_x ) * displayImage->width / grayImage->width;
			disp_y = (correction.crop_y ) * displayImage->height / grayImage->height;
			disp_w = correction.crop_width * displayImage->width / grayImage->width;
			disp_h = correction.crop_height * displayImage->height / grayImage->height;
			tmMarkFailureRegion(displayBlockImage,
						disp_x, disp_y, disp_w, disp_h, COLORMARK_CURRENT);
		}
	}
}

void TamanoirImgProc::setCopySrc(int rel_x, int rel_y) {
	setCopySrc(&m_correct, rel_x, rel_y);
}

void TamanoirImgProc::setCopySrc(t_correction * pcorrection, int rel_x, int rel_y) {
	if(pcorrection->crop_x <= 0) return;
	if(!diffImage) return;
	
	if(g_debug_imgverbose)
		fprintf(stderr, "[imgproc] %s:%d : rel_x=%d rel_y=%d  %dx%d\n", __func__, __LINE__,
			rel_x, rel_y, cropImage->width, cropImage->height);
	
	
	// update center of source 
	int x = rel_x - pcorrection->copy_width / 2;
	int y = rel_y - pcorrection->copy_height / 2;
	
	
	// Store correction in full image buffer
	pcorrection->rel_src_x = x;
	pcorrection->rel_src_y = y;
//	pcorrection->src_x = pcorrection->crop_x + pcorrection->rel_src_x;
//	pcorrection->src_y = pcorrection->crop_y + pcorrection->rel_src_y;


	// Update absolute dest if changed
//	pcorrection->dest_x = pcorrection->crop_x + pcorrection->rel_dest_x;
//	pcorrection->dest_y = pcorrection->crop_y + pcorrection->rel_dest_y;


	// Update display must be requested by the GUI !
}


/* Apply a former correction */
int TamanoirImgProc::skipCorrection(t_correction correction) {
	if(correction.copy_width <= 0) return 0;
	
	
	// Update progress
	int y = correction.crop_y + correction.rel_dest_y;
	m_progress = (int)(100 * y / grayImage->height);
	
	memcpy(&m_last_correction, &correction, sizeof(t_correction));
	
	// Mark skip action on displayImage
	if(displayImage) {
		int disp_x = (correction.crop_x + correction.rel_dest_x ) * displayImage->width / grayImage->width;
		int disp_y = (correction.crop_y + correction.rel_dest_y ) * displayImage->height / grayImage->height;
		int disp_w = correction.copy_width * displayImage->width / grayImage->width;
		int disp_h = correction.copy_height * displayImage->height / grayImage->height;
	
		tmMarkFailureRegion(displayImage,
			disp_x, disp_y, disp_w, disp_h, COLORMARK_REFUSED);
	}	
	return 0;
}

/* Apply proposed correction */
int TamanoirImgProc::applyCorrection()
{
	return applyCorrection(m_correct);
}

int TamanoirImgProc::forceCorrection(t_correction correction, bool force)
{
	if(!force) {
		// Update progress
		int y = correction.crop_y + correction.rel_dest_y;
		m_progress = (int)(100 * y / grayImage->height);
		
		memcpy(&m_last_correction, &correction, sizeof(t_correction));
	}
	
	return applyCorrection(correction, force);
}	



/* Apply a former correction */
int TamanoirImgProc::applyCorrection(t_correction correction, bool force)
{
        if(correction.copy_width < 0)
		return -1; // no available correction
	if(correction.copy_width <= 0)
		return -1; // no available correction
	
	/* Update stats */
	m_dust_stats.nb_grown_validated++;
	if(correction.area<STATS_MAX_SURF) {
		m_dust_stats.grown_size_validated[correction.area]++;
	}
	
	/* Update groundg truth */
	if(g_dataset_mode) {
		
		if(g_dataset_f) {
			fprintf(g_dataset_f, "Dust\t%d,%d+%dx%d"
						"\t%d,%d"
						"\t%c\n",
						correction.crop_x + correction.rel_dest_x, correction.crop_y + correction.rel_dest_y, correction.copy_width, correction.copy_height,
						correction.crop_x+correction.rel_seed_x, correction.crop_y+correction.rel_seed_y,
						(force?'T':'F') );
			fflush(g_dataset_f);
		}
	}
	
	
        if(g_debug_imgverbose //|| force
		) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : Apply clone on original image force=%s.\n", 
				__func__, __LINE__, force?"TRUE":"FALSE");

		fprintf(logfile, "Dust\t%d,%d+%dx%d"
						"\t%d,%d"
						"\t%c\n",
						correction.crop_x + correction.rel_dest_x, correction.crop_y + correction.rel_dest_y, correction.copy_width, correction.copy_height,
						correction.crop_x+correction.rel_seed_x, correction.crop_y+correction.rel_seed_y,
						(force?'T':'F') );
	}
	
	
	/** Update stats */
	m_dust_stats.nb_grown_validated++;
	if(correction.area<STATS_MAX_SURF) {
		m_dust_stats.grown_size_validated[correction.area]++;
	}

	// Correction if dest_x is not set, because it may not be set if we use the clone tool
        int correction_dest_x = correction.crop_x + correction.rel_dest_x;
        int correction_dest_y = correction.crop_y + correction.rel_dest_y;
        int correction_src_x = correction.crop_x + correction.rel_src_x;
        int correction_src_y = correction.crop_y + correction.rel_src_y;

	
	// Apply clone on original image
	tmCloneRegion(  originalImage, 
                        correction_dest_x, correction_dest_y,
                        correction_src_x, correction_src_y,
			correction.copy_width, correction.copy_height);
	
	// Delete same region in diff image to never find it again, even if we
	// use the rewind function
	//if(!force)
		tmFillRegion(  diffImage, 
                        correction_dest_x, correction_dest_y,
			correction.copy_width, correction.copy_height,
			DIFF_NEUTRALIZE);
	
	
	if( displayImage ) {
		// Mark failure on displayImage
                int disp_x = (correction_dest_x ) * displayImage->width / grayImage->width;
                int disp_y = (correction_dest_y ) * displayImage->height / grayImage->height;
		int disp_w = correction.copy_width * displayImage->width / grayImage->width;
		int disp_h = correction.copy_height * displayImage->height / grayImage->height;
		tmMarkFailureRegion(displayImage,
				disp_x, disp_y, disp_w, disp_h, COLORMARK_CORRECTED);
	}
	
	// And clear dust at destination position
	// to make this area usable for future copy source
	if(g_debug_imgoutput) {

		tmMarkCloneRegion(originalImage, 
                                        correction_dest_x, correction_dest_y,
                                        correction_src_x, correction_src_y,
					correction.copy_width, correction.copy_height);
	}
	
	
	memset(&correction, 0, sizeof(t_correction));
        correction.copy_width = -1;
	return 0;
}

/*******************************************************************************

							DATASET

*******************************************************************************/




/** Process then print statistics */
void processAndPrintStats(dust_stats_t * dust_stats, FILE * f) {
	if(f == NULL) {
		f = logfile;
	}
	
	if(dust_stats->nb_grown>0) {
		dust_stats->ratio_replaced_grown = (float)dust_stats->nb_grown_replaced/dust_stats->nb_grown;
		dust_stats->ratio_validated_grown = (float)dust_stats->nb_grown_validated/dust_stats->nb_grown;
	}
	
	if(dust_stats->nb_grown_replaced>0) {
		dust_stats->ratio_validated_replaced = (float)dust_stats->nb_grown_validated/dust_stats->nb_grown_replaced;
	}
/*	unsigned long grown_size[STATS_MAX_SURF];				*! grown size histogram */
/*	unsigned long grown_size_replaced[STATS_MAX_SURF];		*! grown size histogram */
/*	unsigned long grown_size_validated[STATS_MAX_SURF];		*! grown size histogram */
	
	if(f) {
		int max_size = 0;
		int max_size_replaced = 0;
		int max_size_validated = 0;
		for(int size=0; size<STATS_MAX_SURF; size++) {
			if(dust_stats->grown_size[size]>1)
				max_size = size;
			if(dust_stats->grown_size_replaced[size]>1)
				max_size_replaced = size;
			if(dust_stats->grown_size_validated[size]>1)
				max_size_validated = size;
			if(dust_stats->grown_size[size]>0)
				fprintf(f, "[%d] : %lu / %lu / %lu\n", size, 
					dust_stats->grown_size[size], 
					dust_stats->grown_size_replaced[size], 
					dust_stats->grown_size_validated[size]);
		}
		
		/* Statistics variables */
		fprintf(f, "Statistics : \n");
		fprintf(f, "\t nb_grown = %lu\n", dust_stats->nb_grown);
		fprintf(f, "\t nb_grown_replaced = %lu\n", dust_stats->nb_grown_replaced);
		fprintf(f, "\t nb_grown_validated = %lu\n", dust_stats->nb_grown_validated);
		fprintf(f, "\n");
	
		fprintf(f, "\t ratio_replaced/grown = %g %%\n", 100.f*dust_stats->ratio_replaced_grown);
		fprintf(f, "\t ratio_validated/grown = %g %%\n", 100.f*dust_stats->ratio_validated_grown);
		fprintf(f, "\t ratio_validated/replaced = %g %%\n", 100.f*dust_stats->ratio_validated_replaced);
		fprintf(f, "\n");
		
		fprintf(f, "Max sizes :\n");
		fprintf(f, "\tMax size: %d\n", max_size);
		fprintf(f, "\tMax size replaced: %d\n", max_size_replaced);
		fprintf(f, "\tMax size validated: %d\n", max_size_validated);
		fflush(f);
	}
}


/* Test if dust is already knwon */
bool testKnownDust(t_correction correction, int img_w, int img_h) {
	if(g_dataset_mode) return false;
	
	if(!known_dusts)  { // Read dataset file to fill known dusts tab 
	
		if(!g_dataset_f) { // Cannot read file
			fprintf(stderr, "%s:%d : File unknown\n", __func__,__LINE__);
			return false;
		}
		
		// Allocate array
		max_known_dusts = 2000;
		known_dusts = new t_known_dust [ max_known_dusts ];
		memset(known_dusts, 0, sizeof(t_known_dust) * max_known_dusts);
		nb_known_dusts = 0;
		
		// Read file
		
		rewind(g_dataset_f);
		
		char line[512];
		char * ret;
		do {
			// Parse file
			ret = fgets(line, 511, g_dataset_f);
			
			if(ret) {
				// Strip EOL
				if(strlen(line)>0) {
					if(line[strlen(line)-1]=='\n') { line[strlen(line)-1]='\0'; }
				}
				if(strlen(line)>0) {
					if(line[strlen(line)-1]=='\r') { line[strlen(line)-1]='\0'; }
				}
				
				// Then process
				if(strlen(line)>0) {
					char * cmd = line;
					char * next = strstr(line, "\t");
					if(next) {
						*next = '\0';
						next++;
						
						if(strcasestr(cmd, "Size")) { // Read image size
							int l_w, l_h;char c;
							int val = sscanf(next, "%d%c%d", &l_w, &c, &l_h);
							if(val==3) {
								fprintf(stderr, "[imgproc] %s:%d : read size %d x %d "
									"in '%s'\n", __func__,__LINE__,
									l_w, l_h, next);
								orig_width = (double)l_w;
								orig_height = (double)l_h;
							}
						}
						if(strcasestr(cmd, "Resolution")) { // Read image resolution
							int l_dpi;
							int val = sscanf(next, "%d", &l_dpi);
							if(val==1) {
								fprintf(stderr, "[imgproc] %s:%d : read resolution %d dpi"
									"in '%s'\n", __func__,__LINE__,
									l_dpi, next);
							}
						}
						
						if(strcasestr(cmd, "Dust")) { // Read dust
							int l_x, l_y, l_w, l_h, l_seed_x, l_seed_y;
							char c, flag[5]="";
							int val = sscanf(next, "%d%c%d%c%d%c%d%d%c%d%s", &l_x, &c, &l_y, &c, &l_w, &c, &l_h,
								&l_seed_x, &c, &l_seed_y, flag
								);
							if(val==11) {
								fprintf(stderr, "[imgproc] %s:%d : read dust %d, %d + %d x %d seed=%d, %d flag=%s "
									"in '%s'\n", __func__,__LINE__,
									l_x, l_y, l_w, l_h, 
									l_seed_x, l_seed_y, flag, 
									next);
								t_known_dust * pdust = known_dusts + nb_known_dusts;
								nb_known_dusts++;
								if(flag[0]=='T') {
									nb_known_dusts_forced++;
								} else {
									
								}
								if(orig_width > 0.) {
									pdust->dest_x = (double)l_x/orig_width;
									pdust->dest_y = (double)l_y/orig_height;
									pdust->dest_width = (double)l_w/orig_width;
									pdust->dest_height = (double)l_h/orig_height;
									
									pdust->seed_x = (double)l_seed_x/orig_width;
									pdust->seed_y = (double)l_seed_y/orig_height;
									
								} else { // Use absolute values
									pdust->dest_x = (double)l_x;
									pdust->dest_y = (double)l_y;
									pdust->dest_width = (double)l_w;
									pdust->dest_height = (double)l_h;
									
									pdust->seed_x = (double)l_seed_x;
									pdust->seed_y = (double)l_seed_y;
								}
							}
						}
					}
				}
			}
			
			// Test if there's enough place, if not enough, realloc array
			if(nb_known_dusts >= max_known_dusts) {
				int new_nb = max_known_dusts + 200;
				t_known_dust * new_tab = new t_known_dust [ new_nb ];
				memcpy(new_tab, known_dusts, sizeof(t_known_dust) * max_known_dusts);
				
				delete [] known_dusts;
				known_dusts = new_tab;
				max_known_dusts = new_nb;
			}
		} while(ret>0 && !feof(g_dataset_f));
	}
	
	
	// Then evaluate if dust is known
	double dust_x = (double)(correction.crop_x + correction.rel_seed_x);
	double dust_y = (double)(correction.crop_y + correction.rel_seed_y);
	if(img_w > 0) {
		dust_x /= img_w;
		dust_y /= img_h;
	}
	
	// Test if dust is known
	for(int i=0; i<nb_known_dusts; i++) {
		t_known_dust * pdust = known_dusts + i;
		if(fabs(dust_x - pdust->dest_x) < pdust->dest_width*0.5
			&& fabs(dust_y - pdust->dest_y) < pdust->dest_height*0.5) {
			
			found_score++;
			
			float glob_score = (float)found_score / (float)nb_known_dusts;
			fprintf(stderr, "%s:%d : score: %g %%\n", __func__, __LINE__, 100.f * glob_score);
			return true;
		}
	}
	
	return false;
}





