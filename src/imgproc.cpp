/***************************************************************************
 *  imgproc.cpp - Main image processing for dust detection & removing
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
#include "imgutils.h"
#include "imgproc.h"


extern FILE * logfile;
u8 g_debug_imgverbose = 0;
u8 g_debug_imgoutput = 0;
extern u8 g_debug_correlation;
u8 g_debug_dust_seek = 0;

/// Use and debug downscaled mode
u8 g_debug_dwsc = 0;


/// Minimal dust size in pixel^2
#define DUST_MIN_SIZE		5

/// Minimal hot pixel size in pixel^2
#define HOTPIXEL_MIN_SIZE	1

#define TRUST_CORREL_MAX 5.
#define TRUST_AREA_MAX	25
#define TRUST_EQUIV_DIFF 0.05f

#define INCH_IN_MM	25.4f

/** global option : stop if a dust has no replace candidate */
u8 g_option_stopoguess = 0;
u8 g_dataset_mode = 0;
u8 g_evaluate_mode = 0;

/** @brief Use dilate/erode morphology to separate dust from background */
u8 g_mode_use_erodedilate = 1;

/** @brief dataset file output */
FILE * g_dataset_f = NULL;

int found_score = 0;
t_known_dust * known_dusts = NULL;
int nb_known_dusts = 0;
int nb_known_dusts_forced = 0;
int max_known_dusts = 0;
double orig_width=0, orig_height=0;

u8 g_debug_TamanoirImgProc = TMLOG_INFO;

#define TMIMG_printf(a,...)       { \
			if( (a)<=g_debug_TamanoirImgProc ) { \
					struct timeval l_nowtv; gettimeofday (&l_nowtv, NULL); \
					fprintf(logfile,"%d.%03d %s [TmImgProc]::%s:%d : ", \
							(int)(l_nowtv.tv_sec % 1000), \
							(int)(l_nowtv.tv_usec/1000), \
							TMLOG_MSG((a)), __func__,__LINE__); \
					fprintf(logfile,__VA_ARGS__); \
					fprintf(logfile,"\n"); \
			} \
		}

#define PURGE_SIZECHANGED(_img, _size) if((_img) && \
						((_img)->width != (_size).width \
						 || (_img)->height != (_size).height)) { \
							tmReleaseImage(&(_img)); \
						}

#define RESIZE_SIZECHANGED_8U(_img, _size) { \
						if((_img) && \
							((_img)->width != (_size).width \
							 || (_img)->height != (_size).height)) { \
							tmReleaseImage(&(_img)); \
						} \
						if(!(_img)) { \
							(_img) = tmCreateImage((_size),IPL_DEPTH_8U,1); \
						} \
					}

#define TMIMG_save(_filename,_iplimg)	if(g_debug_savetmp) { \
				tmSaveImage(TMP_DIRECTORY _filename IMG_EXTENSION, _iplimg); }


TamanoirImgProc::TamanoirImgProc(int bw, int bh) {
	TMIMG_printf(TMLOG_INFO, "init with block size: %dx%d...\n", bw, bh)
	init();
	TMIMG_printf(TMLOG_INFO, "block size: %dx%d...\n", bw, bh)
	blockSize = cvSize(bw, bh);
}



void TamanoirImgProc::init() {
	memset(&m_preproc_status, 0, sizeof(tm_preproc_status));
	m_preproc_status.preproc_done = 0; // useless, just to see the clearing code
	memset(&m_dust_detection_props, 0, sizeof(tm_dust_detection_props));

	m_last_mode = TMMODE_NOFORCE;

	m_filename[0] = '\0';

	memset(&m_findDust_last_connect, 0, sizeof(CvConnectedComp ));
	m_findDust_last_seed_x = m_findDust_last_seed_y = -1;

	memset(&m_options, 0, sizeof(tm_options));
	m_options.dpi = 2400; // to avoid to divide by 0

	m_dwsc_smoothImage = m_dwsc_dustImage =
						 m_dwsc_diffImage = m_dwsc_growImage = NULL;
	m_downscale_factor = 0;
	m_dwscSize = cvSize(0,0);

#ifdef SIMPLE_VIEW
	m_show_crop_debug = false;
#else
	m_show_crop_debug = true;
#endif
	m_lock = false;


	/* Default values :
	- Trust good correction proposals : no
	- DPI : 2400 dpi
	- hot pixels : no (scan from film !)
	*/
	memset(&m_options, 0, sizeof(tm_options));
	m_options.filmType = FILM_UNDEFINED;
	m_options.hotPixels = false;
	m_options.sensitivity = 0;
	m_options.trust = false;
	m_options.dpi = 2400;
	m_options.hotPixels = false;


	/** Original image size */
	originalSize = cvSize(0,0);


	/** Processed imge size */
	processingSize = cvSize(0,0);

	m_block_seed_width = m_block_seed_height = 0;

	MUTEX_INIT(&mutex)

	// Image buffers
	originalImage = NULL;
	origBlurredImage = NULL;
	undoImage = undoDiffImage = NULL;
	undoImage_x = undoImage_y = -1;
	m_inpainting_lock = false;
	m_inpainting_rendered = true;
	inpaintRenderImage = inpaintMaskImage = NULL;

	// Working images : full size
	grayImage = NULL;
	medianImage = erodedImage = dilatedImage = NULL;
	diffImage = diffImageUnclosed = dustMaskImage = NULL;
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
	originalSmallImage = displayDiffImage = NULL;
	disp_cropOrigImage =
		disp_cropColorImage =
		disp_correctColorImage =
		disp_dilateImage =
		disp_cropImage = NULL;

	displaySize = cvSize(0,0);
	displayCropSize = cvSize(0,0);

	blockSize = cvSize(0,0);

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

	tmReleaseImage(&originalImage);
	tmReleaseImage(&displayDiffImage);
	tmReleaseImage(&displayImage);
	tmReleaseImage(&displayBlockImage);
	tmReleaseImage(&originalSmallImage);

	tmReleaseImage(&undoImage);
	tmReleaseImage(&undoDiffImage);
	tmReleaseImage(&inpaintRenderImage);

	undoImage_x = undoImage_y = -1;

	// Big images
	tmReleaseImage(&grayImage);
	tmReleaseImage(&origBlurredImage);
	tmReleaseImage(&medianImage);
	tmReleaseImage(&erodedImage);
	tmReleaseImage(&dilatedImage);
	tmReleaseImage(&diffImageUnclosed);
	tmReleaseImage(&diffImage);
	tmReleaseImage(&dustMaskImage);
	tmReleaseImage(&varianceImage);
	tmReleaseImage(&growImage);

	purgeDownscaled();

	// purge display and processing buffers
	purgeDisplay();
	purgeCropped();
}

void TamanoirImgProc::purgeDownscaled() {
	TMIMG_printf(TMLOG_DEBUG, "Purge downscaled buffers (size:%dx%d)",
				 m_dwsc_smoothImage?m_dwsc_smoothImage->width : -1,
				 m_dwsc_smoothImage?m_dwsc_smoothImage->height : -1
				 )
	tmReleaseImage(&m_dwsc_smoothImage);
	tmReleaseImage(&m_dwsc_dustImage);
	tmReleaseImage(&m_dwsc_diffImage);
	tmReleaseImage(&m_dwsc_growImage);

}


void TamanoirImgProc::purgeCropped() {
	TMIMG_printf(TMLOG_DEBUG, "Purge cropped buffers (size:%dx%d)",
				 cropImage?cropImage->width : -1,
				 cropImage?cropImage->height : -1
				 )
	// Cropped images
	tmReleaseImage(&cropImage);
	tmReleaseImage(&cropColorImage);
	tmReleaseImage(&dilateImage);
	tmReleaseImage(&tmpCropImage);
	tmReleaseImage(&sobelImage);

	bool purge_correctColorImage = (correctColorImage != correctImage);
	if(purge_correctColorImage) {
		tmReleaseImage(&correctColorImage);
	}
	tmReleaseImage(&correctImage);

	processingSize = cvSize(0,0);
}

void TamanoirImgProc::purgeDisplay() {
	fprintf(stderr, "TamanoirImgProc::%s:%d \n", __func__, __LINE__);
	// Display images
	tmReleaseImage(&disp_cropImage);
	tmReleaseImage(&disp_cropColorImage);
	tmReleaseImage(&disp_cropOrigImage);
	tmReleaseImage(&disp_correctColorImage);
	tmReleaseImage(&disp_dilateImage);

	tmReleaseImage(&undoImage);
	tmReleaseImage(&undoDiffImage);
	tmReleaseImage(&inpaintMaskImage);
	tmReleaseImage(&inpaintRenderImage);

	displayCropSize = cvSize(0,0);
}




void TamanoirImgProc::setDisplaySize(int w, int h) {
	if(w == 0 || h == 0) return;
	if(!//medianImage // is forst created when loading gray image
	   grayImage
	   ) {
		displaySize = cvSize(w, h);
		return;
	}

	if(!originalImage) { return; }

	IplImage * old_displayImg = displayBlockImage;
	if(displayImage) {
		if(w == displayImage->width && h == displayImage->height)
			return; // Already displayed
	}



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
	// Use only size with 4xN dimensions
	while(scaled_width % 4) { scaled_width--; }
	while(scaled_height % 4) { scaled_height--; }

	if(displayImage
	   && displayImage->width == scaled_width
	   && displayImage->height == scaled_height) {
		return;
	}

	tmReleaseImage(&old_displayImg);
	tmReleaseImage(&displayImage);
	displayBlockImage = NULL;
	fprintf(stderr, "TamanoirImgProc::%s:%d : factor=%g => scaled size %dx%d\n", __func__, __LINE__,
			factor,
			scaled_width, scaled_height);


	displaySize = cvSize(scaled_width, scaled_height);

	// Resize
//	displayImage = tmCreateImage(displaySize, IPL_DEPTH_8U, 1);
	IplImage * new_displayImage = tmCreateImage(displaySize, IPL_DEPTH_8U, originalImage->nChannels);
	if(1 || originalImage->depth != new_displayImage->depth) {
		fprintf(stderr, "TamanoirImgProc::%s:%d converting depth: %d => %d...\n",
			__func__, __LINE__,
			originalImage->depth, new_displayImage->depth
			);
		if(g_debug_savetmp) {
			FILE * f = fopen(TMP_DIRECTORY "RAW-originalImage.pgm", "wb");
			if(f) {
				fprintf(f, "P5\n%d %d\n255\n", originalImage->widthStep, originalImage->height);
				fwrite(originalImage->imageData, originalImage->widthStep, originalImage->height, f);
				fclose(f);
			}
		}
		IplImage * tmpDisplayImage = tmCreateImage(cvSize(originalImage->width,originalImage->height),
			IPL_DEPTH_8U, originalImage->nChannels);
		//cvConvertImage(originalImage, tmpDisplayImage ); // cvConvertIMage has bug for 16bit -> 8bit
		tmCropImage(originalImage, tmpDisplayImage, 0,0,
					true // threshold
					);
		TMIMG_save("tmpDisplayImage-depthConverted", tmpDisplayImage)

		cvResize(tmpDisplayImage, new_displayImage, CV_INTER_LINEAR );
		TMIMG_save("new_displayImage-depthConverted", new_displayImage)

		tmReleaseImage(&tmpDisplayImage);
	} else {
		cvResize(originalImage, new_displayImage, CV_INTER_LINEAR );
	}


	// update size, because it may change in cvResize
	displaySize = cvSize(new_displayImage->width, new_displayImage->height);

	// Main display image (with permanent modification)
	displayImage = tmCreateImage(displaySize, IPL_DEPTH_8U, originalImage->nChannels);
	tmReleaseImage(&displayDiffImage);

	TMIMG_printf(TMLOG_INFO, "scaling %dx%dx%d -> %dx%dx%d...\n",
		originalImage->width, originalImage->height,originalImage->nChannels,
		new_displayImage->width, new_displayImage->height, new_displayImage->nChannels
		)

	if(originalImage->nChannels == 1) {
		// Prevent image values to be > 253
		for(int r=0; r<new_displayImage->height; r++)
		{
			u8 * lineGray = (u8 *)new_displayImage->imageData
				+ r * new_displayImage->widthStep;
			for(int c = 0 ; c<new_displayImage->width; c++)
				if(lineGray[c] > COLORMARK_THRESHOLD)
					lineGray[c] = COLORMARK_THRESHOLD;
		}
	} else {
/*		if(originalImage->depth != IPL_DEPTH_8U) {
			fprintf(stderr, "TamanoirImgProc::%s:%d : invert R <-> B for 8 bit images\n",
					__func__, __LINE__);
			for(int r=0; r<new_displayImage->height; r++)
			{
				u8 * lineGray = (u8 *)new_displayImage->imageData
					+ r * new_displayImage->widthStep;
				for(int c = 0 ; c< new_displayImage->width * new_displayImage->nChannels; c+=new_displayImage->nChannels) {
					u8 tmp = lineGray[c];
					if(tmp > COLORMARK_THRESHOLD)
						tmp = COLORMARK_THRESHOLD;
					lineGray[c] =  lineGray[c+2];
					lineGray[c+2] = tmp;
				}
			}
		}
		*/
	}

	TMIMG_save("displayImage", new_displayImage)

	cvCopy(new_displayImage, displayImage);

	// only set the pointer at the end of the fonction, it will unlock the graphical user interface
	displayBlockImage = new_displayImage;
}



int TamanoirImgProc::abortLoading(bool abort) {
	if(m_preproc_status.preproc_done && abort) {
		TMIMG_printf(TMLOG_INFO, "Cannot abort because image is not pre-processing...")
		//return 0;
	}

	m_abortLoading = abort;
	return 0;
}

int TamanoirImgProc::loadFile(const char * filename) {
	memset(&m_preproc_status, 0, sizeof(tm_preproc_status));

	m_abortLoading = false;
	int retry = 0;
	while(retry < 10 && m_lock) {
		tmsleep(1); retry++;
		TMIMG_printf(TMLOG_DEBUG, "locked !! wait 1 s")
	}

	if(m_lock) {
		TMIMG_printf(TMLOG_DEBUG, "locked !! return err=%d", TMIMG_ERR_LOCKED)
		return TMIMG_ERR_LOCKED;
	}
	m_lock = true;

	m_progress = 0;

	// Clear display and processing images
	purge();

	if(m_abortLoading) { // check if we need to abort this processing
                TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
                m_lock = false;
                return TMIMG_ERR_ABORT;
	}

	/* Load with OpenCV cvLoadImage
	IplImage* cvLoadImage( const char* filename, int iscolor CV_DEFAULT(1));
	*/

	TMIMG_printf(TMLOG_INFO, "loading '%s'...", filename)
	struct timeval tvLoad1;
	gettimeofday(&tvLoad1, NULL);
	int file_dpi = 0;

	originalImage = tmLoadImage(filename, &file_dpi);
	m_progress = 10;

	if(file_dpi > 0) {
		TMIMG_printf(TMLOG_INFO, "resolution read in file: %d dpi", file_dpi)
		// There were an resolution in file
		m_options.dpi = file_dpi;
	}

	if(!originalImage) {
		TMIMG_printf(TMLOG_ERROR, "cannot open file '%s' !!", filename)
		m_lock = false;
		m_filename[0] = '\0';
		return -1;
	}
	struct timeval tvLoad2;
	gettimeofday(&tvLoad2, NULL);

	long dt_ms = (tvLoad2.tv_sec - tvLoad1.tv_sec)*1000
				 + (tvLoad2.tv_usec - tvLoad1.tv_usec)/1000;

	tmPrintProperties (originalImage);
        if(m_abortLoading) { // check if we need to abort this processing
                TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
                m_lock = false;
                return TMIMG_ERR_ABORT;
        }

	// store current filename for later saving
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
				TMIMG_printf(TMLOG_ERROR, "cannot open file %s", datafile)
			}
			else {
				TMIMG_printf(TMLOG_INFO, "DATASET: opened file %s", datafile)
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
				fprintf(g_dataset_f, "FilmType\t%d\n", m_options.filmType);
				fprintf(g_dataset_f, "Dpi\t%d\n", m_options.dpi);
				fprintf(g_dataset_f, "HotPixels\t%d\n", m_options.hotPixels?'T':'F');
				fprintf(g_dataset_f, "Trust\t%d\n", m_options.trust?'T':'F');
				fprintf(g_dataset_f, "\n[Dusts]\n");
				fprintf(g_dataset_f, "#Dust\tPosition\tSeed\tForced\n");
				fflush(g_dataset_f);
			}
		}
	}

	switch(originalImage->depth) {
	case IPL_DEPTH_8S:
		TMIMG_printf(TMLOG_INFO, "depth IPL_DEPTH_8S\n")
		break;
	case IPL_DEPTH_16S:
		TMIMG_printf(TMLOG_INFO, "depth IPL_DEPTH_16S\n")
		break;
	case IPL_DEPTH_32S:
		TMIMG_printf(TMLOG_INFO, "depth IPL_DEPTH_32S\n")
		break;
	case IPL_DEPTH_32F:
		TMIMG_printf(TMLOG_INFO, "depth IPL_DEPTH_32F\n")
		break;
	case IPL_DEPTH_64F:
		TMIMG_printf(TMLOG_INFO, "depth IPL_DEPTH_64F \n")
		break;
	case IPL_DEPTH_8U:
		TMIMG_printf(TMLOG_INFO, "depth IPL_DEPTH_8U \n")
		break;
	case IPL_DEPTH_16U:
		TMIMG_printf(TMLOG_INFO, "depth IPL_DEPTH_16U \n")
		break;
	}

	TMIMG_printf(TMLOG_INFO, "File '%s' => w=%d x h=%d x depth=%d x %d bytes : read time= %ld ms\n",
		filename,
		originalImage->width, originalImage->height, originalImage->nChannels,
		tmByteDepth(originalImage), dt_ms)
	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
                m_lock = false;
		return TMIMG_ERR_ABORT;
	}


	originalImage = tmAddBorder4x(originalImage);
	m_progress = 15;

	TMIMG_printf(TMLOG_INFO, "Loading finished: '%s' => w=%d x h=%d x channels=%d => %d bytes per pixel\n"
			"\tLoaded in %ld ms\n",
			filename,
			originalImage->width, originalImage->height, originalImage->nChannels,
			tmByteDepth(originalImage),
			dt_ms);
	m_lock = false;

	m_abortLoading = false;

	return 0;
}


void TamanoirImgProc::processMedian() {
	// Difference and median (allocated here because it may be used for Open filter
	if(!medianImage) {
		medianImage = tmCreateImage(cvGetSize(originalImage),
									IPL_DEPTH_8U, 1);
	}

	if(!diffImage) {
		diffImage = tmCreateImage(cvGetSize(originalImage),
								  IPL_DEPTH_8U, 1);
	}
	if(!diffImageUnclosed) {
		diffImageUnclosed = tmCreateImage(cvGetSize(originalImage),
								  IPL_DEPTH_8U, 1);
	}


	// ==========  ==========
	TMIMG_printf(TMLOG_INFO, "use dilate/erode=%c "
			"dpi=%d => blur grayscaled image "
			"=> median image... size %d x %d\n",
			(g_mode_use_erodedilate?'T':'F'),
			m_options.dpi,
			m_smooth_size, m_smooth_size)

	if(!g_mode_use_erodedilate) {
		if(!varianceImage) {
			varianceImage = tmCreateImage(cvGetSize(originalImage),
										  IPL_DEPTH_32F, 1);
		}
		// Blurred grayscaled image (Gaussian blur)
		cvSmooth(grayImage, medianImage,
				 CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
				 m_smooth_size, m_smooth_size );
		// For debug, save image in temporary directory
		TMIMG_save("medianImage", medianImage)
	} else {
		if(!erodedImage) {
			erodedImage = tmCreateImage(cvGetSize(originalImage),
										IPL_DEPTH_8U, 1);
		}
		if(!dilatedImage) {
			dilatedImage = tmCreateImage(cvGetSize(originalImage),
										 IPL_DEPTH_8U, 1);
		}

		// void tmDilateImage(IplImage * src, IplImage * dst, int kernelsize = 1, int iterations = 1);
		// Lighter dusts are shown in dilate
		TMIMG_printf(TMLOG_TRACE, "call tmDilateImage(grayImage=%p, dilatedImage=%p, m_smooth_size=%d, 1);...",
					 grayImage, dilatedImage, m_smooth_size)
		tmDilateImage(grayImage, dilatedImage, m_smooth_size, 1);
		TMIMG_save("medianImage-dilated", dilatedImage)


		TMIMG_printf(TMLOG_TRACE, "call tmErodeImage(grayImage=%p, erodedImage=%p, m_smooth_size=%d, 1);...",
					 grayImage, erodedImage, m_smooth_size)

		// void tmErodeImage(IplImage * src, IplImage * dst, int kernelsize = 1, int iterations = 1);
		// Darker dusts are shown in erode
		tmErodeImage(grayImage, erodedImage, m_smooth_size, 1);
		TMIMG_save("medianImage-eroded", erodedImage)
	}

}

/*
 * IMAGE PRE-PROCESSING => DUST DETECTION MAP
 */
int TamanoirImgProc::preProcessImage() {
	TMIMG_printf(TMLOG_INFO, "Pre-processing image....")
	m_preproc_status.preproc_done = false;
	int retry = 0;
	while(retry < 10 && m_lock) {
		tmsleep(1); retry++;
		TMIMG_printf(TMLOG_INFO, "locked !!")
	}
	if(m_lock) {
		TMIMG_printf(TMLOG_ERROR, "still locked !!")
		return -1;
	}
	if(!originalImage) {
		TMIMG_printf(TMLOG_ERROR, "no originalImage !!")
		return -1;
	}

	m_lock = true;

	// We need to process again the conversion to graycale
	// because grayscale image may be blurred

	// convert to Grayscaled image
	// we need to do it to integrate last corrections applied on original color image into the new grayscale

	// Huge file : should use faster method than reload to convert to grayscale
	fprintf(logfile, "TamanoirImgProc::%s:%d : fast conversion to grayscaled image...\n",
		__func__, __LINE__);
	grayImage = tmFastConvertToGrayscale(originalImage);
	TMIMG_save("grayImage-fastConv", grayImage)

	m_progress = 20;



	if(displaySize.width > 0) {
		TMIMG_printf(TMLOG_DEBUG, "re-setDisplaySize with former value %dx%d",
					 displaySize.width, displaySize.height)
		setDisplaySize( displaySize.width, displaySize.height);
	}
	m_progress = 25;


/* unused & slow
	float mean = 0.f, variance = 0.f, diff_mean = 0.f;

	int uniform = findUniform(&mean, &diff_mean, &variance);
	if(uniform >= 0) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : Process uniform zone detection => mean=%g, diff=%g, var=%g\n",
			__func__, __LINE__, mean, diff_mean, variance);
	}
*/

	// clear correction structs
	memset(&m_last_correction, 0, sizeof(t_correction));
	memset(&m_correct, 0, sizeof(t_correction));

	// Reset search position
	m_seed_x = m_seed_y = 0;
	// Reset current block position
	m_block_seed_x = m_block_seed_y = 0;

	m_progress = 25;
	originalSize = cvGetSize(originalImage);

	if(!g_mode_use_erodedilate) {
		// Smooth size depend on DPI - size of 9 is ok at 2400 dpi
		//4 is for median smooth
		m_smooth_size = 1 + 2*(int)roundf(4.f * m_options.dpi / 2400.f);
	} else {
		// 2 is for erode/dilate
		m_smooth_size = 1 + 2*(int)roundf(2.f * m_options.dpi / 2400.f);
	}

	if(m_smooth_size < 3) {
		m_smooth_size = 3;
	}



	// Process median image
	if(!m_preproc_status.median_done) {
		TMIMG_printf(TMLOG_DEBUG, "Process median filter")
		processMedian();
		m_preproc_status.median_done = 1;
	}
	m_progress = 45;

	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
				m_lock = false;
		return TMIMG_ERR_ABORT;
	}

	// FOR GRAINY IMAGES, SMOOTH ORIGINAL IMAGE TOO
	if(0 && !g_mode_use_erodedilate) {
		int orig_smooth_size = 1 + 2*(int)(4 * m_options.dpi / 2400);
		TMIMG_printf(TMLOG_DEBUG, "blur original color image with Gaussian blur=%dx%d",
					 orig_smooth_size, orig_smooth_size)

		if(!origBlurredImage) {
			origBlurredImage = tmCreateImage(cvGetSize(originalImage),
				originalImage->depth, originalImage->nChannels);
		}
		// First, blur the color image
		cvSmooth( originalImage, origBlurredImage,
				CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
				orig_smooth_size, orig_smooth_size); //int param1=3, int param2=0 );
	}

	m_progress = 40;

	// ********* DIFFERENCE BETWEEN ORIGINAL AND BLURRED ****************
	// Also compute variance image
	TMIMG_printf(TMLOG_DEBUG, "difference processing image...")
	cvZero(diffImage);

	// Difference histogram
	unsigned long diffHisto[256];
	memset(diffHisto, 0, sizeof(unsigned long)*256);

	int var_size = 1 + 2*(4 * m_options.dpi/2400);
	if(var_size < 3) var_size = 3;

	double cdgH = 0.;
	if(!g_mode_use_erodedilate) {
		tmProcessDiff(m_options.filmType,
				  grayImage, medianImage,
				  diffImage, varianceImage, diffHisto, var_size);
		if(g_debug_savetmp) {
			tmSaveImage(TMP_DIRECTORY "diffImage=gray-median" IMG_EXTENSION, medianImage);
		}
		// Process difference histogram analysis :
		unsigned long maxHisto = 0;
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
	}
	else {
		m_threshold = m_options.sensitivity;
	}

	m_progress = 45;
	unsigned long diffHisto2[256];
	memset(diffHisto2, 0, sizeof(unsigned long)*256);

	// If image is really noisy, for example with B&W grain (or High sensitivity films), pre-process a 3x3 blur filter
	if(!g_mode_use_erodedilate
		&&   cdgH >= /*3.0*/ 2.8 )
	{
		fprintf(logfile, "TamanoirImgProc::%s:%d : GRAIN ERASER : Process 3x3 blur filter on input image ...\n",
			__func__, __LINE__);
		cvSmooth(grayImage, diffImage,
			CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
			3, 3 );
		m_progress = 50;

		cvCopy(diffImage, grayImage);

		TMIMG_save("grayImage-smoothed", medianImage)


		// Of smooth size was already 3, increase smooth
		if(m_smooth_size == 3) {
			m_smooth_size += 4;
			cvSmooth(grayImage, medianImage,
				CV_MEDIAN, //CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
				m_smooth_size, m_smooth_size );
		}
		if(m_abortLoading) { // check if we need to abort this processing
			TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
			m_lock = false;
			return TMIMG_ERR_ABORT;
		}

		m_progress = 52;
		processMedian();
		m_progress = 56;

		if(m_abortLoading) { // check if we need to abort this processing
			TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
			m_lock = false;
			return TMIMG_ERR_ABORT;
		}

		// For debug, save image in temporary directory
		TMIMG_save("medianImage-resmoothed", medianImage)

		TMIMG_printf(TMLOG_INFO, "GRAIN ERASER : RE-process difference on blurred image ...")

		tmProcessDiff(m_options.filmType,
						  grayImage, medianImage,
						  diffImage, varianceImage,
						  diffHisto2, var_size);
		if(m_abortLoading) { // check if we need to abort this processing
			TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
			m_lock = false;
			return TMIMG_ERR_ABORT;
		}
	} else if(g_mode_use_erodedilate ) {

		TMIMG_printf(TMLOG_INFO, "Use erode/dilate method: first processMedian()...")
		if(!m_preproc_status.median_done) {
			TMIMG_printf(TMLOG_INFO, "median not done : processMedian()...")
			processMedian();
			m_preproc_status.median_done = 1;
		}
		m_progress = 50;
		if(m_abortLoading) { // check if we need to abort this processing
			TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
			m_lock = false;
			return TMIMG_ERR_ABORT;
		}
		m_threshold = m_options.sensitivity;

		m_progress = 55;
		/* BEST = dilate - erode; */
		TMIMG_printf(TMLOG_INFO, "Use erode/dilate method: diff with dilated-eroded...")
		tmProcessDilate_Erode(m_options.filmType,
					  dilatedImage, erodedImage,
					  diffImage,
					  diffHisto2, var_size);
		m_preproc_status.diff_done = 1;

		TMIMG_save("diffImage=erode-dilate", diffImage)

		if(m_abortLoading) { // check if we need to abort this processing
			TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
			m_lock = false;
			return TMIMG_ERR_ABORT;
		}


		// Normalize diffImage
		IplImage * erodedDiff = tmCreateImage(cvGetSize(diffImage),
											  IPL_DEPTH_8U, 1);
		// Erode to have the min of diff
		tmErodeImage(diffImage, erodedDiff, m_smooth_size*2+1, 1);
		TMIMG_save( "diffImage-eroded",	erodedDiff)

		if(m_abortLoading) { // check if we need to abort this processing
			TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
			m_lock = false;
			return TMIMG_ERR_ABORT;
		}

		// then store difference in diffImage
		for(int r=0; r<diffImage->height; r++) {
			u8 * diff = IPLLINE_8U(diffImage, r);
			u8 * erod = IPLLINE_8U(erodedDiff, r);
			for(int c = 0; c<diffImage->width; c++) {
				diff[c] -= erod[c];
			}
		}

		if(m_abortLoading) { // check if we need to abort this processing
			TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
			m_lock = false;
			return TMIMG_ERR_ABORT;
		}

		TMIMG_save("diffImage-normed", diffImage)

		tmReleaseImage(&erodedDiff);
	}

	m_progress = 60;

	if(m_threshold < 4) {
		m_threshold = 4;
	}

//	unsigned char * diffImageBuffer = (unsigned char *)diffImage->imageData;
//	unsigned char * grayImageBuffer = (unsigned char *)grayImage->imageData;
	//unused unsigned char * medianImageBuffer = (unsigned char *)medianImage->imageData;
	int width = diffImage->widthStep;
	int height = diffImage->height;
	int pos;

#if 0 // OLD VERSION WITH SEPARATED CRITERIONS => REPLACED WITH 1 CRITERION ONLY
	// THRESHOLD DIFF IMAGE
	switch(m_options.filmType) {
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
#else // NEW CRITERION : theshold only
	// Downscale (subsampling with max criterion) diffImage -> displayDiffImage
	if(!displayDiffImage) {
		displayDiffImage = tmCreateImage(cvSize(displayImage->width, displayImage->height),
										 IPL_DEPTH_8U, 1);
	} else {
		cvZero(displayDiffImage);
	}

	// Process downscaled image detection of dusts
	if(!m_dwsc_diffImage) {
		processDownscaledAnalysis();
	}

	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
		m_lock = false;
		return TMIMG_ERR_ABORT;
	}

	// scale image with max
	tmScaleMax(diffImage, displayDiffImage);
	//tmScaleMax(m_dwsc_diffImage, displayDiffImage);
	TMIMG_save("diffDisplayImage", displayDiffImage)

	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
		m_lock = false;
		return TMIMG_ERR_ABORT;
	}

	m_threshold = m_options.sensitivity;

//	float dwsc_x = (float)m_dwsc_diffImage->width / (float)diffImage->width;
//	float dwsc_y = (float)m_dwsc_diffImage->height / (float)diffImage->height;

	// Threshold diff image
	for(int r=0;r<height; r++) {

		u8 * diffline = IPLLINE_8U(diffImage, r);

//		int dwsc_r = (int)roundf((float)r * dwsc_y);
//		u8 * diffdwsc = IPLLINE_8U(m_dwsc_diffImage, dwsc_r);
		for( int c = 0; c< diffImage->width; c++ ) {
			u8 diff = diffline[c];

//			int dwsc_c = (int)roundf((float)c * dwsc_x);
//			u8 ref = diffdwsc[dwsc_c];

			if(//ref > m_threshold && // and level is visible on dwsc diff image
			   diff > m_threshold
			   ) {
				diffline[c] = DIFF_THRESHVAL;
			} else {
				// to many differences to have good results
//				if(diff >= tmmax(3, m_threshold-2)) {
//					diffImageBuffer[pos] = DIFF_CONTOUR;
//				}
			}
		}
	}
	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
		m_lock = false;
		return TMIMG_ERR_ABORT;
	}
#endif

	TMIMG_save("diffImage", diffImage)
	m_progress = 70;

	// Do a close operation on diffImage
	if(1) {
		cvCopy(diffImage, diffImageUnclosed);

		tmCloseImage(diffImage, medianImage,
					 growImage,  // tmp
					 1);

		// For debug, save image in temporary directory
		TMIMG_save(TMP_DIRECTORY "diffImage-closed" IMG_EXTENSION, medianImage);
		TMIMG_save(TMP_DIRECTORY "grayImage-final" IMG_EXTENSION, grayImage);

		cvCopy(medianImage, diffImage);

		// since the diffImage has been closes, the min size is pi*r^2
		//FIXME : adapt the size to the previously done operations:
		// the width/height of dust cannot be less than dilatation
		//m_dust_min = (int)(3.1416 * 3. * 3.);
	}



	m_progress = 80;
	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
		m_lock = false;
		return TMIMG_ERR_ABORT;
	}

	TMIMG_printf(TMLOG_DEBUG, "init dust detector...")


	// Difference
	TMIMG_printf(TMLOG_DEBUG, "create grow image...")
	if(!growImage) {
		growImage = tmCreateImage(cvGetSize(originalImage),
			IPL_DEPTH_8U, 1);
	}


	TMIMG_printf(TMLOG_DEBUG, "create crop images blockSize=%dx%d....",
		blockSize.width, blockSize.height)

	// Cropped image
	processingSize = cvSize( tmmin(blockSize.width, originalImage->width),
							tmmin(blockSize.height, originalImage->height));
	// Use processing size for block, and do not change after
	m_block_seed_width = processingSize.width;
	m_block_seed_height = processingSize.height;

	TMIMG_printf(TMLOG_DEBUG, "using m_block_width/height = %dx%d",
			m_block_seed_width, m_block_seed_height)

	allocCropped();
	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
		m_lock = false;
		return TMIMG_ERR_ABORT;
	}

	// process image depending on resolution
	processResolution(m_options.dpi);
	if(m_abortLoading) { // check if we need to abort this processing
		TMIMG_printf(TMLOG_INFO, "Aborted !! return err=%d", TMIMG_ERR_ABORT)
		m_lock = false;
		return TMIMG_ERR_ABORT;
	}

	TMIMG_save("diffImage-closed=final", diffImage);

	cvZero(growImage); // need to reset because it has been used for temporary image

	m_lock = false;


	// Then process
	m_progress = 100;
	m_preproc_status.preproc_done = true;

	TMIMG_printf(TMLOG_INFO, "Pre-processing done.")

	return 0;
}


void TamanoirImgProc::processDownscaledAnalysis() {

	if(!g_debug_dwsc) {

		return;
	}

	// Downscale eroded and dilates images to have the "summary" of image and dusts positions
	m_downscale_factor = 4 * m_smooth_size;
	m_dwscSize = cvSize(originalImage->width / m_downscale_factor,
						originalImage->height / m_downscale_factor);
	RESIZE_SIZECHANGED_8U(m_dwsc_smoothImage, m_dwscSize);
	RESIZE_SIZECHANGED_8U(m_dwsc_dustImage, m_dwscSize);
	RESIZE_SIZECHANGED_8U(m_dwsc_diffImage, m_dwscSize);
	RESIZE_SIZECHANGED_8U(m_dwsc_growImage, m_dwscSize);



	tmScaleMax(diffImage, m_dwsc_diffImage);
	switch(m_options.filmType) {
	 default:
	 case FILM_UNDEFINED:
			// use gray for smooth, since we don't know the real nature of dusts
			tmScaleMean(erodedImage, m_dwsc_smoothImage);

			// use gray for smooth, since we don't know the real nature of dusts
			tmScaleMean(erodedImage, m_dwsc_smoothImage);

			break;
	 case FILM_NEGATIVE:
			// Dusts appear as white in dilated images
			tmScaleMax(dilatedImage, m_dwsc_dustImage);

			// and are not present on eroded image
			tmScaleMean(erodedImage, m_dwsc_smoothImage);
			break;
	 case FILM_POSITIVE:
			// Dusts appear as dark in eroded images
			tmScaleMin(erodedImage, m_dwsc_dustImage);

			// and are not present on eroded image
			tmScaleMean(dilatedImage, m_dwsc_smoothImage);
			break;

	}

	if(g_debug_savetmp
	   || g_debug_dwsc
	   ) {
		// Save uded images
		TMIMG_save("dwsc-diffImage" , m_dwsc_diffImage);
		TMIMG_save("dwsc-dustImage" , m_dwsc_dustImage);
		TMIMG_save("dwsc-smoothImage" , m_dwsc_smoothImage);

		// Try some others
		IplImage * tmpScaledImage = tmCreateImage(m_dwscSize,
												  IPL_DEPTH_8U, 1);
		tmScaleMin(erodedImage, tmpScaledImage);
		TMIMG_save("erodedMinImage", tmpScaledImage);
		tmScaleMean(erodedImage, tmpScaledImage);
		TMIMG_save("erodedMeanImage", tmpScaledImage);
		tmScaleMax(dilatedImage, tmpScaledImage);
		TMIMG_save("dilatedMaxImage", tmpScaledImage);
		tmScaleMean(dilatedImage, tmpScaledImage);
		TMIMG_save("dilatedMeanImage", tmpScaledImage);

		tmReleaseImage(&tmpScaledImage);
	}
/*
	void tmFloodRegion(unsigned char * growIn, unsigned char * growOut,
		int swidth, int sheight,
		int c, int r,
		unsigned char seedValue,
		unsigned char threshold,
		unsigned char fillValue,
		CvConnectedComp * areaOut)
	*/

	// Check if every dust is not in textured area
	IplImage * smoothDiffImage = NULL, * floodImage = NULL, * diffNormImage = NULL;
	RESIZE_SIZECHANGED_8U(smoothDiffImage, m_dwscSize);
	RESIZE_SIZECHANGED_8U(diffNormImage, m_dwscSize);
	tmErodeImage(m_dwsc_diffImage, smoothDiffImage, 3, 1);

//	tmScaleMean(diffImage, smoothDiffImage);
	RESIZE_SIZECHANGED_8U(floodImage, m_dwscSize);

	IplImage * floodDebugImage = NULL;
	RESIZE_SIZECHANGED_8U(floodDebugImage, m_dwscSize);
	cvCopy(m_dwsc_dustImage, floodDebugImage);

	cvZero(m_dwsc_growImage);

	u8 fillvalue = 67; // .. 247 ... 2

	u8 threshold_diff = 5; //m_threshold/2;
	for(int r = 0; r<m_dwscSize.height; r++) {
		u8 * diffline = IPLLINE_8U(m_dwsc_diffImage, r);
		u8 * normline = IPLLINE_8U(diffNormImage, r);
		u8 * diffsmooth = IPLLINE_8U(smoothDiffImage, r);
		for(int c = 0; c<m_dwscSize.width; c++) {
			normline[c] = diffline[c]-diffsmooth[c];
		}
	}

	if(g_debug_savetmp || g_debug_dwsc) {
		TMIMG_save("dwsc-diffNormImage", diffNormImage);
	}

	// Only in debug for the moment
	cvCopy(diffNormImage, m_dwsc_diffImage); // use downscaled min image as mask

	if(!g_debug_dwsc) {

		return;
	}


	for(int r = 0; r<m_dwscSize.height; r++) {
//unused		u8 * diffline = IPLLINE_8U(m_dwsc_diffImage, r);
		u8 * grownline = IPLLINE_8U(m_dwsc_growImage, r);
//unused		u8 * diffsmooth = IPLLINE_8U(smoothDiffImage, r);
		u8 * diffnorm = IPLLINE_8U(diffNormImage, r);
		u8 * smoothline = IPLLINE_8U(m_dwsc_smoothImage, r);
//unused		u8 * dustline = IPLLINE_8U(m_dwsc_dustImage, r);
		for(int c = 1; c<m_dwscSize.width; c++) {
			//  if the difference image is big enough,
			if(!grownline[c] &&
					diffnorm[c] > threshold_diff) {
				// check if region is limited in difImage
				fillvalue += 20;
				if(fillvalue<47) fillvalue = 47;

				// Grow region
				CvConnectedComp dust_connect;
				// limit search
//				cvSetImageROI(diffNormImage, cvRect(c-8, r-8, 16,16));
				tmGrowRegion(//m_dwsc_diffImage,
						diffNormImage,
							 m_dwsc_growImage,
							 c, r,
							 threshold_diff, fillvalue,
							 &dust_connect
							 );

				// if dust is present
				if(dust_connect.area > 10
				   || dust_connect.area<1) {

					// Erase region in diff image
//					tmEraseRegion(m_dwsc_growImage,
//								  diffNormImage, //m_dwsc_diffImage,
//								  c,r,
//								  fillvalue
//						//		  , 0 // fill diffNormImage with 0
//								  );

					if(floodDebugImage) {
						cvRectangle(floodDebugImage,
									cvPoint((int)(dust_connect.rect.x+dust_connect.rect.width), (int)(dust_connect.rect.y+dust_connect.rect.height)),
									cvPoint((int)dust_connect.rect.x, (int)dust_connect.rect.y),
									cvScalarAll(0),
									1);
					}

				} else { // Dust is big enough
					// Check if region is bigger in smooth area
					cvZero(floodImage);

					// limit search
					cvSetImageROI(m_dwsc_smoothImage, cvRect(c-8, r-8, 16,16));

					CvConnectedComp flood_connect;
					tmFloodRegion(m_dwsc_smoothImage,
								 floodImage,
								 c, r, smoothline[c],
								 threshold_diff, 255,
								 &flood_connect
								 );

					// If the flooded area is big,
					fprintf(stderr, "%d,%d : dust=%dx%d:%d flood=%dx%d:%d",
							c,r,
							(int)dust_connect.rect.width, (int)dust_connect.rect.height, (int)dust_connect.area,
							(int)flood_connect.rect.width, (int)flood_connect.rect.height, (int)flood_connect.area
							);
					if(flood_connect.area > 8 ) {
						// Region is much bigger than std dust :
						// mark region
//						cvRectangle(floodDebugImage,
//									cvPoint((int)(flood_connect.rect.x+flood_connect.rect.width), (int)(flood_connect.rect.y+flood_connect.rect.height)),
//									cvPoint((int)flood_connect.rect.x, (int)flood_connect.rect.y),
//									cvScalarAll(255),
//									1);
						if(floodDebugImage) {
							cvRectangle(floodDebugImage,
										cvPoint((int)(dust_connect.rect.x+dust_connect.rect.width), (int)(dust_connect.rect.y+dust_connect.rect.height)),
										cvPoint((int)dust_connect.rect.x, (int)dust_connect.rect.y),
										cvScalarAll(255),
										1);
						}
					} else { // else grown is too small, eg, region is not empty
						if(floodDebugImage) {
							cvRectangle(floodDebugImage,
										cvPoint((int)(dust_connect.rect.x+dust_connect.rect.width), (int)(dust_connect.rect.y+dust_connect.rect.height)),
										cvPoint((int)dust_connect.rect.x, (int)dust_connect.rect.y),
										cvScalarAll(64),
										1);
						}

					}
				}
			}
		}
	}
	if(g_debug_savetmp|| g_debug_dwsc) {
		// Save used images
		TMIMG_save("dwsc-diffSmoothImage" , smoothDiffImage);
		TMIMG_save("dwsc-diffOutImage" , diffNormImage);
		TMIMG_save("dwsc-floodRectImage" , floodDebugImage);
		TMIMG_save("dwsc-growImage" , m_dwsc_growImage);

	}
	tmReleaseImage(&floodDebugImage);
	tmReleaseImage(&smoothDiffImage);
}


void TamanoirImgProc::allocCropped() {
	if(processingSize.width <= 0) return;
	if(processingSize.height <= 0) return;

	/* FIXME : whi this ? can't remember...
	while( (processingSize.width % 4) > 0) {
		processingSize.width--;
	}
	while( (processingSize.height % 4) > 0) {
		processingSize.height--;
	}
	*/
	TMIMG_printf(TMLOG_INFO, "(re)alloc cropped images for processing : processingSize= %d x %d\n",
		processingSize.width, processingSize.height)

	if(!cropImage) cropImage = tmCreateImage(processingSize,IPL_DEPTH_8U, 1);
	if(!tmpCropImage) tmpCropImage = tmCreateImage(processingSize,IPL_DEPTH_8U, 1);
	if(!cropColorImage) cropColorImage = tmCreateImage(processingSize,IPL_DEPTH_8U, originalImage->nChannels);
	if(!sobelImage) sobelImage = tmCreateImage(processingSize,IPL_DEPTH_16S, 1);
	if(!dilateImage) dilateImage = tmCreateImage(processingSize,IPL_DEPTH_8U, 1);
	if(!correctImage) correctImage = tmCreateImage(processingSize, IPL_DEPTH_8U, 1);

	if(originalImage->nChannels == 1) {
		correctColorImage = correctImage;
	} else {
		correctColorImage = tmCreateImage(processingSize, IPL_DEPTH_8U, originalImage->nChannels);
	}
}

IplImage * TamanoirImgProc::getDustMask() {

	// Difference must be allocated and filled
	if(!diffImage) {
		TMIMG_printf(TMLOG_INFO, "Cannot export dust mask layer : no diffImage")
		return NULL;
	}

	// alloc and clear before filling mask
	if(!dustMaskImage) {
		dustMaskImage = tmCreateImage(cvGetSize(diffImage),
									  IPL_DEPTH_8U, 1);
	} else {
		cvZero(dustMaskImage);
	}
	TMIMG_printf(TMLOG_INFO, "Exporting dust mask layer: %dx%d",
				 dustMaskImage->width, dustMaskImage->height)

	// Only fill mask with points where diffImage has been corrected
	for(int r=0; r<diffImage->height; r++) {
		u8 * diffline = (u8 *)(diffImage->imageData + r * diffImage->widthStep);
		u8 * dustline = (u8 *)(dustMaskImage->imageData + r * dustMaskImage->widthStep);

		for(int c = 0; c<diffImage->width; c++) {
			if(diffline[c] == DIFF_CORRECTED) {
				dustline[c] = 255;
			}
		}
	}

	TMIMG_printf(TMLOG_INFO, "export dust mask layer : done")
	return dustMaskImage;
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
				tmsleep(1); retry++;
		TMIMG_printf(TMLOG_DEBUG, "locked !!")
	}
	if(m_lock)
		return -1;

	m_lock = true;

	TMIMG_printf(TMLOG_INFO, "saving '%s'...", filename)

	TMIMG_save("growImage_final", growImage)
	TMIMG_save("diffImage_final", diffImage)

	if(originalImage) {
		tmSaveImage(filename, originalImage);
		m_lock = false;
		return 0;
	}

	m_lock = false;
	return -1;
}





/* Tamanoir settings/options *
typedef struct {
	bool trust;		/ *! Trust mode activated * /
	bool hotPixels;	/ *! Hot pixels detection activated * /
	int filmType;	/ *! Film type * /
	int dpi;		/ *! Scan resolution in dot per inch * /
	int sensitivity;
} tm_options;
*/
void fprintfOptions(FILE * f, tm_options * p_options) {
	if(!f) return;
	fprintf(f, "# Options : \n");
	fprintf(f, "Trust:%c\n", p_options->trust ? 'T' : 'F');
	fprintf(f, "HotPixels:%c\n", p_options->hotPixels ? 'T' : 'F');
	fprintf(f, "OnlyEmpty:%c\n", p_options->onlyEmpty ? 'T' : 'F');
	fprintf(f, "FilmType:%d\n", p_options->filmType);
	fprintf(f, "DPI:%d\n", p_options->dpi);
	fprintf(f, "Sensitivity:%d\n", p_options->sensitivity);

	fflush(f);
}


bool TamanoirImgProc::setOptions(tm_options opt) {
	bool rewind_to_start = false;
	bool rewind_to_previous = false;

	TMIMG_printf(TMLOG_INFO, "%p : current options : ", this)
	fprintfOptions(stderr, &m_options);
	TMIMG_printf(TMLOG_INFO, "new options : ")
	fprintfOptions(stderr, &opt);

	/*
	 * Film type change
	 */
	if(m_options.filmType != opt.filmType) {
		rewind_to_start = true;
		purgeDownscaled();
	}

	/*
	 * Activate/desactivate trust on good corrections
	 */
	if(m_options.trust != opt.trust) {
		// We changed to trust mode, return to last correction
		if(opt.trust) {
			rewind_to_previous = true;
		}
	}

	/*
	 * Activate/desactivate hot pixels filtering
	 */
	m_dust_area_min = DUST_MIN_SIZE * (m_options.dpi * m_options.dpi)/ (2400*2400);
	bool force_update_dpi = false;
	if(opt.hotPixels != m_options.hotPixels) {
		TMIMG_printf(TMLOG_INFO, "%s hot pixels filtering "
					 "=> change min size of dust : %d pix^2 => later call processResolution(dpi=%d)",
			(opt.hotPixels ? "ACTIVATE" : "DESACTIVATE" ),
			m_dust_area_min,
			opt.dpi)

		force_update_dpi = true;

		if(opt.hotPixels) {
			// we're looking for smaller dust (in pixel size)
			// so we have to go back to start
			rewind_to_start = true;

			m_dust_area_min = HOTPIXEL_MIN_SIZE;
		}
	}

	// If the sensitivity changed, clear also
	if(m_options.sensitivity != opt.sensitivity) {
		// We need to re-process median and difference
		m_preproc_status.median_done =
				m_preproc_status.diff_done =
				m_preproc_status.preproc_done =
				0;

		if(opt.sensitivity > m_options.sensitivity) {
			// We need less sensibility so we can resume from where we were
			rewind_to_previous = true;
			// need to preprocessing image again
			m_preproc_status.preproc_done = false;
		} else {
			rewind_to_start = true;
		}
	}

	if(m_options.onlyEmpty != opt.onlyEmpty) {
		if(opt.onlyEmpty) {
			// We need less sensibility  for the next dusts
			// so we can resume from where we were
			rewind_to_previous = true;
		} else {
			rewind_to_start = true;
		}
	}

	// Then set resolution, because we process the min size here
	if(force_update_dpi || m_options.dpi != opt.dpi) {
		if(opt.dpi < m_options.dpi ) {
			// we're looking for smaller dust (in pixel size)
			// so we have to go back to start
			rewind_to_start = true;
		} else {
			rewind_to_previous = true;
		}

		if(m_options.dpi != opt.dpi) {
			TMIMG_printf(TMLOG_DEBUG, "Resolution changed: process dwsc again")
			purgeDownscaled();
		}

		// Then process resolution change
		processResolution(opt.dpi);
	}

	// At last, store version
	m_options = opt;

	if(rewind_to_start && originalImage) {
		TMIMG_printf(TMLOG_INFO, "rewind to START "
				"last found dust : %d,%d, => call preProcessImage\n",
				m_seed_x, m_seed_y)

		while(m_lock) {
			m_abortLoading = true;
			// Wait for unlock
			tmsleep(1); TMIMG_printf(TMLOG_DEBUG, "Wait for unlock...")
		}
		m_abortLoading = false;
		if(!m_preproc_status.preproc_done) {
			TMIMG_printf(TMLOG_DEBUG, "!m_preproc_status.preproc_done => call preProcessImage()...")
			preProcessImage();
		}

		firstDust();

	} else if(rewind_to_previous) {
		if(m_block_seed_width<=0) { m_block_seed_width = 1; }
		if(m_block_seed_height<=0) { m_block_seed_height = 1; }

		// use previous values
		m_seed_x = m_last_correction.crop_x + m_last_correction.rel_seed_x;
		m_seed_y = m_last_correction.crop_y + m_last_correction.rel_seed_y;

		// Recompute block
		m_block_seed_x = (m_seed_x / m_block_seed_width)*m_block_seed_width;
		m_block_seed_y = (m_seed_y / m_block_seed_height)*m_block_seed_height;

		fprintf(stderr, "[TamanoiImgProc]::%s:%d : rewind to LAST "
				"corrected dust : %d,%d block=%d,%d\n",
				__func__, __LINE__, m_seed_x, m_seed_y,
				m_block_seed_x, m_block_seed_y);fflush(stderr);
		if(growImage) {
			cvZero(growImage);
		}

		// Then search next
		nextDust();
	} else {

		/*
		 * Mode auto
		 */
		if(opt.mode_auto) {
			// We need to reprocess al image
			// Reset search position
			m_seed_x = m_seed_y = 0;
			// Reset current block position
			m_block_seed_x = m_block_seed_y = 0;

			TMIMG_printf(TMLOG_INFO, "MODE AUTO => rewind to START "
				" : %d,%d",
				m_seed_x, m_seed_y)

			if(growImage) { // Clear grown image to precess again the whole image
				cvZero(growImage);
			}

			// Re-process diffImage, because it may change the diffImage
			preProcessImage();

			// Look for first dust
			nextDust();
		}
	}

	m_options = opt;
	return rewind_to_start;
}


int TamanoirImgProc::processResolution(int dpi) {
	if(m_options.dpi == dpi) {
		TMIMG_printf(TMLOG_DEBUG, "ALREADY SET scan resolution = %d dpi", dpi)
//		return 0;
	}

	// Then re-process file
	TMIMG_printf(TMLOG_DEBUG, "set scan resolution to %d dpi", dpi)

	if(originalImage && m_preproc_status.preproc_done) {
		// need to pre-process again the image because the blur radius changed
		if(m_options.dpi != dpi && m_preproc_status.preproc_done) {
			m_options.dpi = dpi;
			TMIMG_printf(TMLOG_DEBUG, "need to pre-process again the image because the blur radius changed / %d dpi", dpi)

			m_preproc_status.preproc_done = false;
		}
	}

	// We just reset the dust seeker
	// Allocate region growing structs
	float coef_sensitivity[4] = { 1.f, 1.5f, 3.f, 5.f};
	int l_sensitivity = m_options.sensitivity; // FIXME : not adapted here
	l_sensitivity = 1; // FIXME : force to be sensitive
	if(l_sensitivity < 0 ) {
		l_sensitivity = 0; }
	else if(l_sensitivity > 3 ) {
		l_sensitivity = 1; }

	m_dust_area_min = DUST_MIN_SIZE * (dpi * dpi)/ (2400*2400)
					  * coef_sensitivity[ l_sensitivity ] ;
	if(m_options.hotPixels
	   || m_dust_area_min<HOTPIXEL_MIN_SIZE) {
		m_dust_area_min = HOTPIXEL_MIN_SIZE;
		TMIMG_printf(TMLOG_DEBUG, "hot pixels => FORCE dust min area %d pixels^2\n",
			m_dust_area_min)
	}

	TMIMG_printf(TMLOG_DEBUG, "=> dust min area %d pixels^2\n",
		m_dust_area_min)

	m_dust_area_max = 800 * dpi / 2400;


	TMIMG_printf(TMLOG_DEBUG, " scan resolution = %d dpi. Done.", dpi)
	m_options.dpi = dpi;

	return 0;
}


int TamanoirImgProc::setBlockSize(int w, int h) {
	if(w == blockSize.width && h == blockSize.height) {
		TMIMG_printf(TMLOG_DEBUG, "same size (w=%d, h=%d) => return 0", w, h)
		return 0;
	}

	TMIMG_printf(TMLOG_INFO, "(w=%d, h=%d) => recall firstDust()", w, h)
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

void TamanoirImgProc::undo() {
	fprintf(stderr, "TamanoirImgProc::%s:%d : UNDO\n", __func__, __LINE__);
	if(undoImage) {
		// Copy undo image into original image
		tmInsertImage(undoImage, originalImage, undoImage_x, undoImage_y);
		// Copy diff image into original image
		tmInsertImage(undoDiffImage, diffImage, undoImage_x, undoImage_y);

		// Update images and clear masks
		recropImages(cvSize(undoImage->width, undoImage->height),
					 undoImage_x, undoImage_y);
	} else {
		fprintf(stderr, "TamanoirImgProc::%s:%d : ERROR: NO UNDO\n", __func__, __LINE__);
	}
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

	// Clear block index
	m_block_seed_x = m_block_seed_y = 0;

	// clear cache for clone mode
	m_findDust_last_seed_x = m_findDust_last_seed_y = -1;

	memset(&m_correct, 0, sizeof(t_correction));
	memset(&m_lastDustComp, 0, sizeof(CvConnectedComp));


	// Clear buffer of known dusts
	if(growImage) {
		memset(growImage->imageData, 0, growImage->widthStep * growImage->height);
	}

	// Disable trust for first image
	bool old_trust = m_options.trust;
	int ret = nextDust();
	TMIMG_printf(TMLOG_INFO, "Rewind to first dust : ret of first durst search = %d", ret);
	m_options.trust = old_trust;

	return ret;
}

int TamanoirImgProc::nextDust() {
	if(!diffImage || !growImage) return -1;
	if(!diffImage->imageData || !growImage->imageData) return -1;

	int retry = 0;
	while(retry < 10 && m_lock) {
		tmsleep(1); retry++;
		fprintf(stderr, "[imgproc]::%s:%d : locked !!\n", __func__, __LINE__);
	}

	if(m_lock) {
		return -1;
	}

	m_lock = true;

	if(g_debug_imgverbose) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : searching seed from %d,%d ...\n",
						__func__, __LINE__, m_seed_y, m_seed_x);
	}

	memset(&m_correct, 0, sizeof(t_correction));
	memset(&m_lastDustComp, 0, sizeof(CvConnectedComp));

	m_correct.copy_width = -1; // Clear stored correction
	int width = diffImage->width;
	int height = diffImage->height;
	int x, y;

	do {
		int ymax = tmmin(height-1, m_block_seed_y+m_block_seed_height);
		int xmin = m_block_seed_x;
		int xmax = tmmin(width-1, m_block_seed_x+m_block_seed_width);

		TMIMG_printf(TMLOG_DEBUG, "Searching from %d,%d to %d,%d",
					 xmin, m_block_seed_y, xmax, ymax);

		for(y = m_block_seed_y; //search only from last dust
			y<ymax; y++) {
			u8 * growline = IPLLINE_8U(growImage, y);
			u8 * diffline = IPLLINE_8U(diffImage, y);
			for(x = xmin; x<xmax; x++) {
				if(!growline[x]) {
					if(diffline[x] == DIFF_THRESHVAL ) {
						// Grow region here if the region is big enough
						if(//diffImageBuffer[pos+pitch] == DIFF_THRESHVAL
								1 || m_options.hotPixels)
						{

							int return_now = findDust(x,y);
//TMIMG_printf(TMLOG_INFO, "seed %d,%d => return = %d", x,y,return_now);
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
		}

		// If we did not found a seed, in this block, change block :
		// increase block x
		m_block_seed_x += m_block_seed_width;
		m_seed_x = 0;
		if(m_block_seed_x >= width) {
			TMIMG_printf(TMLOG_DEBUG, "changed seed block y=%d => %d",
						 m_block_seed_y, m_block_seed_y + m_block_seed_height)
			// block moved to far on right => rewind to left, and go down one block
			m_block_seed_x = 0;
			m_block_seed_y += m_block_seed_height;
		}

		TMIMG_printf(TMLOG_DEBUG, "changed block => %d,%d +%dx%d / img size=%dx%d",
					 m_block_seed_x, m_block_seed_y,
					 m_block_seed_width, m_block_seed_height,
					 width, height)		
	} while(m_block_seed_x<width && m_block_seed_y<height);

	m_lock = false;

	return 0;
}


int TamanoirImgProc::findDust(int x, int y) {

	return findDust(x,y, &m_correct);
}

int TamanoirImgProc::findDust(int x, int y,
							  t_correction * pcorrection,
							  int force_mode) {
	if(!growImage) return -1; // Loading is not finished

	if(x<0 || x>=diffImage->width
	   || y<0 || y>=diffImage->height ) {
		TMIMG_printf(TMLOG_WARNING, "invalid x,y=%d,%d / diffImage = %dx%d",
					 x,y, diffImage->width, diffImage->height)
		return -1;
	}

	if(!cropImage) {
		TMIMG_printf(TMLOG_WARNING, "no cropImage => Loading is not finished : abort dust searching")
		return -1; // Loading is not finished
	}

	MUTEX_LOCK(&mutex);

	// Porperties of current dust
	memset(&m_dust_detection_props, 0, sizeof(tm_dust_detection_props));
	m_dust_detection_props.seed_x = x;
	m_dust_detection_props.seed_y = y;

	CvConnectedComp connect;
//	u8 * diffImageBuffer = (u8 *)diffImage->imageData;
//	u8 * growImageBuffer = (u8 *)growImage->imageData;

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
	m_dust_detection_props.force_search = force_search;

	u8 diff_threshold = (force_search ? DIFF_NOT_DUST : DIFF_THRESHVAL);

	//int diffpitch = diffImage->widthStep;

	// If forced search, find another seed if the current one is not different enough from background
	if(force_search
	   && IPLPIX_8U(diffImage, x,y) < diff_threshold
	   ) { //
		int lw = growImage->width;
		int lpitch = growImage->widthStep;
		int lh = growImage->height;

		int search_width = 8; //tmmin(16, tmmax(pcorrection->copy_width, pcorrection->copy_height))/2;

		// search a dust for region growing in
		int xleft = x - search_width ;
		if(xleft<0) xleft = 0;
		else if(xleft >= lw) {
			MUTEX_UNLOCK(&mutex)
			return 0;
		}
		int xright = x + search_width ;
		if(xright<0) {
			MUTEX_UNLOCK(&mutex)
			return 0;
		} else if(xright >= lw) { xright = lw-1; }

		if(xright <= xleft) {
			MUTEX_UNLOCK(&mutex)
			return 0;
		}

		int ytop  = y - search_width ;
		if(ytop<0) ytop = 0;
		else if(ytop >= lh) {
			MUTEX_UNLOCK(&mutex)
			return 0;
		}

		int ybottom = y + search_width ;
		if(ybottom<0) {
			MUTEX_UNLOCK(&mutex)
			return 0;
		} else if(ybottom >= lh) { ybottom = lh-1; }


		// No difference at this point
		// return 0;
		int closest_dist = 20*20 ;
		int xin = x, yin = y;
		for(int ly=ytop; ly<=ybottom; ly++) {
			int dy = yin - ly;

			u8 * diffline = IPLLINE_8U(diffImage, ly);
			for(int lx=xleft; lx<=xright; lx++) {
				if(diffline[lx] >= diff_threshold) {
					int dx = xin - lx;
					int dist = (dx*dx+dy*dy);
					if(dist < closest_dist) {
						closest_dist = dist;
						x = lx; y = ly;
						if(g_debug_imgverbose > 1 || g_debug_dust_seek) {
							fprintf(logfile, "TamanoirImgProc::%s:%d : changed seed at %d,%d => %d,%d "
									"=> diffImage[%d,%d]=%d / diff_threshold=%d / dist=%d\n",
									__func__, __LINE__,
									xin, yin,
									x, y, x, y,
									(int)IPLPIX_8U(diffImage, x, y),
									(int)diff_threshold, dist);
						}
					}
				}
			}
		}

		// Process a region growing
		/*tmGrowRegion( growImageBuffer, growImageBuffer,
			diffImage->widthStep, diffImage->height,
			x, y,
			1,
			0,
			&connect );*/
		// Clear grown buffer to enable second region growing
		for(int ly=ytop; ly<=ybottom; ly++) {
			memset(growImage->imageData + ly*lpitch + xleft,
				   0, xright - xleft);
		}
	}

	// Check if we can continue
	if( IPLPIX_8U(diffImage, x, y) < diff_threshold) {
		TMIMG_printf(TMLOG_TRACE, "diff(%d,%d)<threshold=%d",
					 x, y, diff_threshold)

		MUTEX_UNLOCK(&mutex)
		return 0;
	}

	if(m_findDust_last_seed_x == x && m_findDust_last_seed_y == y) {
		TMIMG_printf((g_debug_dust_seek ? TMLOG_INFO : TMLOG_DEBUG), "same as last seed %d,%d",
					 m_findDust_last_seed_x, m_findDust_last_seed_y)
		MUTEX_UNLOCK(&mutex)
		return 0;
	}


	if(g_debug_imgverbose > 1 || g_debug_dust_seek) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : found seed at %d,%d "
				"=> diffImage[%d,%d]=%d / DIFF_THRESHVAL=%d\n",
				__func__, __LINE__, x, y, x, y,
				(int)IPLPIX_8U(diffImage, x, y),
				DIFF_THRESHVAL);
	}

	// Process a region growing from the original seed to find the dust in original image
	tmGrowRegion(
			diffImage, growImage,
			x, y,
			diff_threshold,
			255,
			&connect
			);

	if(connect.rect.x == m_findDust_last_connect.rect.x
	   && connect.rect.y == m_findDust_last_connect.rect.y
	   && connect.rect.width == m_findDust_last_connect.rect.width
	   && connect.rect.height == m_findDust_last_connect.rect.height
	   && connect.area == m_findDust_last_connect.area ) {
		TMIMG_printf(TMLOG_DEBUG, "same as grown region %d,%d+%dx%d",
					(int)connect.rect.x, (int)connect.rect.y,
					(int)connect.rect.width, (int)connect.rect.height
					)
		MUTEX_UNLOCK(&mutex)
		return 0;
	}

	memcpy(&m_findDust_last_connect, &connect, sizeof(CvConnectedComp ));

	m_dust_detection_props.abs_grown_conn = connect;
	m_dust_detection_props.connect_area = connect.area;

	if(g_debug_imgverbose > 1 || g_debug_dust_seek) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : seed at %d,%d => area=%d,%d+%dx%d surf=%d  min=%d <? %d <? max=%d...\n",
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
	// this is a dust if the size if big enough or the force mode is active
	bool is_a_dust = ( (connect.area >= m_dust_area_min &&
							 connect.area < m_dust_area_max)
					   //do not search for very small dusts any more: || force_search
					   );

	m_dust_detection_props.big_enough = is_a_dust;

	// If we search manually, force criterion on size
	is_a_dust |= force_search;

	// Crop image and search for a dust on cropped images
	if( is_a_dust ) {
		int connect_area = (int)connect.area;
		int connect_width = connect.rect.width;
		int connect_height = connect.rect.height;


		if(!force_search) {
			// There a seed here !!! append to stats
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


		if(crop_x < 0) { crop_x = 0; }
		else if(crop_x + crop_width >= originalImage->width) {
			crop_x = tmmax(0, originalImage->width - crop_width-1);
		}
		if(crop_y < 0) { crop_y = 0; }
		else if(crop_y + crop_height >= originalImage->height) {
			crop_y = tmmax(0, originalImage->height - crop_height-1);
		}


		// Real position of dust in cropped image
		int crop_center_x = x - crop_x;
		int crop_center_y = y - crop_y;

		if(g_debug_imgverbose > 1 || g_debug_dust_seek) {
			fprintf(logfile, "TamanoirImgProc::%s:%d : grown dust at %d,%d+%dx%d "
					"=> %d,%d in cropped image (crop +%d,%d)...\n",
				__func__, __LINE__,
				connect.rect.x,connect.rect.y,
				connect.rect.width, connect.rect.height,
				crop_center_x, crop_center_y,
				crop_x, crop_y
				);
		}

		CvConnectedComp crop_connect;
		int best_correl = 0;
		float sum_dust = 0.f, sum_neighbour = 0.f;


		// Dilate dust around the seed
		is_a_dust = dilateDust(crop_x, crop_y,
							   crop_center_x, crop_center_y,
							   &crop_connect,
							   is_a_dust, force_search,
							   dilateImage, &best_correl,
							   &sum_dust, &sum_neighbour
							   );

		m_dust_detection_props.rel_grown_conn = crop_connect;
		m_dust_detection_props.dilateDust = is_a_dust;

/* CROPPED IMAGES :
	- cropImage = crop(diffImage)
	- correctImage = grown(cropImage/diffImage)
	- tmpCropImage = crop(grayImage)
	- dilateImage = dilate(correctImage)
	*/
		if(!is_a_dust) { // We had a problem with second dust growing

			if(g_debug_imgverbose > 1 || g_debug_dust_seek)
			{
				fprintf(logfile, "TamanoirImgProc::%s:%d : dust regrow "
						"whole => %d,%d+%dx%d "
						"=> crop(%d,%d) :%d,%d+%dx%d "
					"=> %d,%d in cropped image => PROBLEM : NOT FOUND WITH SAME THRESHOLD...\n",
					__func__, __LINE__,
					connect.rect.x, connect.rect.y,
					connect.rect.width, connect.rect.height,

					crop_x, crop_y,
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
		} else
		{

			// Use more strict search condition
			m_dust_detection_props.best_correl_dilate = best_correl;
			if(force_search && !g_debug_dust_seek) {
				best_correl = (int)(2.f * fabsf(sum_dust - sum_neighbour));
			}
			m_dust_detection_props.best_correl_max = best_correl;

			// Process search around the dust in original image
			// Use center x,y - width,height
			connect_width = crop_connect.rect.width;
			connect_height = crop_connect.rect.height;



			int connect_center_x = crop_connect.rect.x + connect_width/2;
			int connect_center_y = crop_connect.rect.y + connect_height/2;

			// If the search if forced, limit search to input size
			if( force_search ) {
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

					// FIXME : Maybe not a good idea to limit position of search
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
			int ret = 0;

			// If we try only to use inpainting, we don't need to find a replacement
			if(force_search &&
				force_mode == TMMODE_INPAINT) {
				// no need for replacement
				ret = 0;
				copy_width = connect_width;
				copy_height = connect_height;
				copy_dest_x = copy_src_x = connect_center_x;
				copy_dest_y = copy_src_y = connect_center_y;
			}
			else {
				// Find best proposal in originalImage
				ret = tmSearchBestCorrelation(
						correctColorImage, dilateImage,
						connect_center_x, connect_center_y,
						connect_width, connect_height,
						&copy_dest_x, &copy_dest_y,
						&copy_src_x, &copy_src_y,
						&copy_width, &copy_height,
						&best_correl);
			}

			m_dust_detection_props.searchBestCorrelation = ret;

			// Debug
			if(g_debug_imgverbose > 1
#ifndef SIMPLE_VIEWER
				|| force_search
#endif
				) {

				int maxdiff = 100, nbdiff =  0;
				float l_dist = tmCorrelation(correctColorImage, correctColorImage,
						dilateImage,
						connect_center_x, connect_center_y,
						pcorrection->rel_src_x, pcorrection->rel_src_y,
						connect_width*2, connect_height*2,
						100,
						&maxdiff,
						&nbdiff
						);
				m_dust_detection_props.correl_dust_src = l_dist;

/* CROPPED IMAGES :
	- cropImage = crop(diffImage)
	- correctImage = grown(cropImage/diffImage)
	- tmpCropImage = crop(grayImage)
	- dilateImage = dilate(correctImage)
	*/
				// Display the min/max/mean of the proposal
				u8 propmin = 255, propmax = 0;
				float propmean = 0.f;
				for(int r=copy_src_y; r<copy_src_y+copy_height; r++) {
					u8 * grayline = (u8 *)(tmpCropImage->imageData+r*tmpCropImage->widthStep);
					for(int c=copy_src_x; c<copy_src_x+copy_width; c++) {
						u8 src = grayline[c];
						propmean += (float)src;
						if(src < propmin) { propmin = src; }
						if(src > propmax) { propmax = src; }
					}
				}
				propmean /= (float)(copy_width*copy_height);

				if(g_debug_imgoutput || g_debug_dust_seek) {
					tmSaveImage(TMP_DIRECTORY "dilateImage.pgm", dilateImage);

					fprintf(stderr, "\t::%s:%d : grown:%d,%d+%dx%d "
							"=> ret=%d best_correl=%d copy from %d,%d => to %d,%d %d x %d\n"
							"\tcandidate: min/max/mean = %d/%d / %g\n"
							"\tdiff (force) : dist=%g / maxdiff= %d / nbdiff=%d\n",
							__func__, __LINE__,
							connect_center_x, connect_center_y,
							connect_width, connect_height,
							ret, best_correl,
							copy_src_x, copy_src_y,
							copy_dest_x, copy_dest_y,
							copy_width, copy_height,
							(int)propmin, (int)propmax, propmean,
							l_dist, maxdiff, nbdiff);
				}
			}


			// we found a dust and good replacement ===================================
			// but let's check some other things
			if(ret>0) {
				u8 return_now = 1;
				m_lastDustComp = connect;

				/** Update stats */
				m_dust_stats.nb_grown_replaced++;
				if(connect_area<STATS_MAX_SURF) {
					m_dust_stats.grown_size_replaced[connect_area]++;
				}



				// ====== THEN WE FOUND A REPLACEMENT ==============
				// Use a bigger region for cloning
				copy_width = copy_width * 3/2;
				copy_height = copy_height * 3/2;

				// Store correction in full image buffer
				if(1 ||  // FIXME : remove this for normal clone mode
				   !force_search)
				{
//					pcorrection->dest_x = crop_x + copy_dest_x;
//					pcorrection->dest_y = crop_y + copy_dest_y;

					pcorrection->copy_width = copy_width;
					pcorrection->copy_height = copy_height;
				} else {
					pcorrection->copy_width = copy_width;
					pcorrection->copy_height = copy_height;

					// Force copy dest to be where we forced the seed
					// Centered dest determined by correlation
					int centered_dest_x = copy_dest_x;
					int centered_dest_y = copy_dest_y;

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

				// copy vector = dest - src
				m_dust_detection_props.copy_dx =  copy_dest_x - copy_src_x;
				m_dust_detection_props.copy_dy =  copy_dest_y - copy_src_y;

				// Update dest
//				pcorrection->src_x = pcorrection->crop_x + pcorrection->rel_src_x;
//				pcorrection->src_y = pcorrection->crop_y + pcorrection->rel_src_y;
				pcorrection->area = connect_area;
				pcorrection->grown_dust = crop_connect;


				// Fill size statistics
				pcorrection->area = (int)connect.area;
				pcorrection->width_mm = INCH_IN_MM * connect.rect.width / m_options.dpi;
				pcorrection->height_mm = INCH_IN_MM * connect.rect.height / m_options.dpi;

				pcorrection->bg_diff = (sum_dust - sum_neighbour);
				pcorrection->proposal_diff = best_correl;

				// If we search for inpainting, the grown region is enough
				if(force_search
				   && force_mode == TMMODE_INPAINT) {
					// stop processing now
					MUTEX_UNLOCK(&mutex);
					return 1;
				}

// ====================================================================================
// NOW WE CHECK IF THIS REGION IS LIKELY TO BE A REAL DUST
				// Crop median image
				tmCropImage(medianImage,
							tmpCropImage,
							crop_x, crop_y);
/* CROPPED IMAGES :
	- cropImage = crop(diffImage)
	- correctImage = grown(cropImage/diffImage)
	- dilateImage = dilate(correctImage)

	- tmpCropImage = crop(medianImage)
	*/

				// check the source and the destination
				// are in the same extended grown region,
				// (tree branch, building edge...)
				// FIXME : improve this connection search
				m_dust_detection_props.src_not_connected_to_dest = is_a_dust =
							srcNotConnectedToDest(pcorrection);

/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = dilate(correctImage)

- tmpCropImage = crop(medianImage)
- correctImage = grown(cropImage/diffImage)
*/

				// Dust must be visible faced to other different regions
				//		count pixels over this level
				u8 diffref = (u8)fabsf(sum_dust - sum_neighbour);
				bool diff_from_neighbour = differentFromNeighbourHood(pcorrection,
																	  diffref);
				m_dust_detection_props.diff_from_neighbour = diff_from_neighbour;
				if(is_a_dust) {
					is_a_dust = diff_from_neighbour;
				}


				// Dust and correction must be different (for avoiding periodical patterns)
				if(is_a_dust) {
		// FIXME : to be improved for windows edges, building windows...
					m_dust_detection_props.diff_from_dest = is_a_dust =
															srcDifferentFromDest(pcorrection);

/* CROPPED IMAGES :
	- cropImage = crop(diffImage)
	- dilateImage = 0

	- tmpCropImage = crop(medianImage)
	- correctImage = smooth(tmpCropImage)
	*/
				}


				// Then check that correction is better than original
				// e.g. that there's less difference after correction
				// between the corrected image and the median/blurred image
				bool is_correctedBetterThanOriginal = is_a_dust;

				if(is_a_dust || force_search) {
					m_dust_detection_props.correctedBetterThanOriginal =
					is_correctedBetterThanOriginal =
							correctedBetterThanOriginal(pcorrection);
				}


				is_a_dust &= is_correctedBetterThanOriginal;
/* CROPPED IMAGES :
- cropImage = crop(diffImage)

- tmpCropImage = crop(grayImage).corrected
- dilateImage = tmpCropImage-correctImage
- correctImage = grown(dilateImage)
*/


				// =========== OPTION : ONLY DETECTION IN EMPTY AREA =============
				// test if region is in empty area (sky)
				if(is_a_dust
				   && m_options.onlyEmpty
				   && !force_search  // only when we're not forcing to search a specific dust
				   ) {
					m_dust_detection_props.neighbourhoodEmpty =
						is_a_dust = neighbourhoodEmpty( pcorrection );

					TMIMG_printf(TMLOG_DEBUG, "onlyEmpty: neighbourhood is %s",
								 m_dust_detection_props.neighbourhoodEmpty ?"empty":"filled")

/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = tmpCropImage-correctImage

- tmpCropImage = crop(grayImage)
- correctImage = smooth(tmpCropImage)
*/
				}

// END OF DUST LIKELYHOOD TESTS : is_a_dust is conclusion
// ===================================================================================



				// If not a dust, do not return now
				if(!is_a_dust) {
					return_now = 0;
					// statistics
//					if(connect_area<STATS_MAX_SURF) {
//						m_dust_stats.grown_size_replaced[connect_area]++;
//					}
				}

				// Trust mode : check if neighbour is empty enough to correct by default
				if( (m_options.trust //|| m_options.mode_auto
					 && !force_search
					 )// || m_options.onlyEmpty)
				   && return_now) {
					// Check if correction area is in diff image
					/* OBSOLETE : use percentage of dusts pixels in copy+src bounding box
					int left =   tmmin(copy_src_x - copy_width/2, copy_dest_x - copy_width/2);
					int right =  tmmax(copy_src_x + copy_width/2, copy_dest_x + copy_width/2);
					int top =    tmmin(copy_src_y - copy_height/2, copy_dest_y - copy_height/2);
					int bottom = tmmax(copy_src_y + copy_height/2, copy_dest_y + copy_height);
					float fill_failure = tmNonZeroRatio(diffImage,
											crop_x + left, crop_y + top,
												right - left, bottom - top,
											crop_x + copy_dest_x, crop_y + copy_dest_y,
											copy_width, copy_height
											);*/
					//	crop_x + left, crop_y + top,
					//	right - left, bottom - top);

					//fprintf(stderr, "[imgproc] %s:%d : fill_failure=%g\n",
					//	__func__, __LINE__, fill_failure);

					// If we can trust the correction proposal, let's correct know !
					if( (best_correl < TRUST_CORREL_MAX
						&& pcorrection->area < TRUST_AREA_MAX
						//&& fill_failure == 0
						&& pcorrection->equivalent_diff < TRUST_EQUIV_DIFF
						)
						// or if mode auto, apply now !
						//|| m_options.mode_auto

						) {

						if(g_debug_imgverbose || g_debug_dust_seek) {
							fprintf(stderr, "[imgproc]::%s:%d : m_options.trust=%c & fill_failure=%g\n",
								__func__, __LINE__,
								m_options.trust?'T':'F',
								pcorrection->equivalent_diff //fill_failure
								);
						}

						// We can correct and return later
						forceCorrection(*pcorrection, true);

						// With auto mode, we need to refresh display
						if(m_options.mode_auto) {

						}

						return_now = 0;
					}
				}


				// If we must return from this function now, we store the dust in statistics array
				if(return_now) {

					if(g_evaluate_mode) {
						// Evaluate if dust is already known
						bool known = testKnownDust(m_correct, originalImage->width, originalImage->height);

						fprintf(logfile, "\t[imgproc]::%s:%d : Evaluate : "
								"dust at %d,%d+%dx%d "
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

					//fprintf(stderr, "\t[imgproc] %s:%d : return 1\n", __func__, __LINE__);
					MUTEX_UNLOCK(&mutex);
					return 1;
				}

				if((g_debug_imgoutput || g_debug_savetmp)
					&& displayImage) {
					int disp_x = (crop_x + crop_connect.rect.x) * displayImage->width / grayImage->width;
					int disp_y = (crop_y + crop_connect.rect.y) * displayImage->height / grayImage->height;
					int disp_w = crop_connect.rect.width * displayImage->width / grayImage->width;
					int disp_h = crop_connect.rect.height * displayImage->height / grayImage->height;
					tmMarkFailureRegion(displayImage,
							disp_x, disp_y, disp_w, disp_h, COLORMARK_FAILED);
					tmMarkFailureRegion(displayBlockImage,
							disp_x, disp_y, disp_w, disp_h, COLORMARK_FAILED);
				}


			} else {
				if(!force_search) m_perf_stats.no_proposal++;
				// DEBUG FUNCTIONS
				if(g_debug_imgverbose > 1 || g_debug_dust_seek) {
					fprintf(logfile, "TamanoirImgProc::%s:%d : dust at %d,%d+%dx%d "
						"=> %d,%d in cropped image => no replace candidate (best=%d)...\n",
						__func__, __LINE__,
						connect.rect.x,connect.rect.y,
						connect.rect.width, connect.rect.height,
						crop_center_x, crop_center_y, best_correl
						);
					if(g_debug_savetmp || g_debug_dust_seek)
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
					//tmMarkFailureRegion(diffImage,
					//	crop_x + crop_connect.rect.x, crop_y + crop_connect.rect.y,
					//	crop_connect.rect.width, crop_connect.rect.height, 120);
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
		}
	}

	if(!is_a_dust
	   && !force_search
	   ) {
		if(g_debug_dust_seek) {
			fprintf(stderr, "TamanoirImgProc::%s:%d : conclusion : not a dust @ %d,%d area=%g\n", __func__, __LINE__,
				x, y, connect.area);
		}

//		if(g_debug_dust_seek ) {
//			tmSaveImage(TMP_DIRECTORY "erase-growImage-in" IMG_EXTENSION,
//						growImage);
//			tmSaveImage(TMP_DIRECTORY "erase-diffImage-in" IMG_EXTENSION,
//						diffImage);
//		}
		// Clear grown region with neutral mask
		tmEraseRegion( growImage, diffImage,
			x, y,
			255,
			DIFF_NOT_DUST
			);
//		if(g_debug_dust_seek ) {
//			tmSaveImage(TMP_DIRECTORY "erase-growImage-out" IMG_EXTENSION,
//						growImage);
//			tmSaveImage(TMP_DIRECTORY "erase-diffImage-out" IMG_EXTENSION,
//						diffImage);
//		}
	}

	MUTEX_UNLOCK(&mutex);
	return 0;
}

bool TamanoirImgProc::srcNotConnectedToDest(t_correction * pcorrection)
{
	// check the source and the destination
	// are in the same extended grown region,
	// (tree branch, building edge...)

	// Re-grow region in cropped image
	tmCropImage(diffImage, cropImage,
				pcorrection->crop_x, pcorrection->crop_y);

	// clear grow buffer
	cvZero(correctImage);

	// Grow again region in cropped image
	CvConnectedComp ext_connect;
	tmGrowRegion(
		cropImage,
		correctImage,
		pcorrection->rel_seed_x, pcorrection->rel_seed_y,
		DIFF_CONTOUR,
		242,
		&ext_connect);
	
	
/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = dilate(correctImage)

- tmpCropImage = crop(medianImage)
- correctImage = grown(cropImage/diffImage)
*/

	// If source and destination are connected,
	// the source would be filled with 242
	float src_failure = tmNonZeroRatio(
			correctImage,
			pcorrection->rel_src_x-pcorrection->copy_width/2,
				pcorrection->rel_src_y-pcorrection->copy_height/2,
			pcorrection->copy_width,
				pcorrection->copy_height,
				correctImage->width,correctImage->height,0,0,// No exclusion ROI
			242);

	if(g_debug_savetmp) {
		tmSaveImage(TMP_DIRECTORY "crop7-srcNotConnectedToDest-1-in" IMG_EXTENSION, cropImage);

		/*
		IPLPIX_8U(correctImage, pcorrection->rel_seed_x, pcorrection->rel_seed_y ) = 32;
		IPLPIX_8U(correctImage, pcorrection->rel_src_x, pcorrection->rel_src_y ) = 132;

		tmMarkCloneRegion(correctImage,
						  pcorrection->rel_dest_x, pcorrection->rel_dest_y,
						  pcorrection->rel_src_x, pcorrection->rel_src_y,
						  pcorrection->copy_width, pcorrection->copy_height,
						  true);*/
		tmSaveImage(TMP_DIRECTORY "crop7-srcNotConnectedToDest-2-grownContour" IMG_EXTENSION, correctImage);
	}

	if(src_failure > 0.2f) {
		if(g_debug_imgverbose || g_debug_dust_seek) {
			fprintf(stderr, "\t[imgproc]::%s:%d : ratio=%g => src and dest are connected : this is an edge, not a dust\n",
				__func__, __LINE__, src_failure);
		}

		//return false;
	}

	return true;
}




/*
 * test if the dust gray level is different enough from background
 */
bool TamanoirImgProc::differentFromNeighbourHood(
		t_correction * pcorrection,
		u8 diffref)
{
	// Analysis of the repartition of difference in region
	int rect_cmin = tmmax(0,
						  tmmin(pcorrection->rel_src_x - pcorrection->copy_width,
							pcorrection->rel_dest_x - pcorrection->copy_width)),
		rect_rmin = tmmax(0,
						  tmmin(pcorrection->rel_src_y - pcorrection->copy_height,
							pcorrection->rel_dest_y - pcorrection->copy_height)),
		rect_cmax = tmmin(cropImage->height,
						  tmmax(pcorrection->rel_src_x + pcorrection->copy_width,
							pcorrection->rel_dest_x + pcorrection->copy_width)),
		rect_rmax = tmmin(cropImage->height,
						  tmmax(pcorrection->rel_src_y + pcorrection->copy_height,
							pcorrection->rel_dest_y + pcorrection->copy_height));
	if(rect_cmin>=rect_cmax
	   || rect_rmin>=rect_rmax) {
		pcorrection->equivalent_diff = -1;
		return false;
	}

	int diffcount = 0, r, c, nb=0;
	for(r=rect_rmin;r<rect_rmax; r++) {
		u8 * diffImageLine = (u8 *)(cropImage->imageData + r*cropImage->widthStep);
		u8 * dilateImageLine = (u8 *)(dilateImage->imageData + r*dilateImage->widthStep);
		for(c=rect_cmin; c<rect_cmax; c++) {
			if(!dilateImageLine[c]) {
				nb++;
				if(diffImageLine[c] > diffref ) {
					diffcount ++;
				}
			}
		}
	}

	// dust must be visible faced to other different regions
	// count pixels over this level
	if(g_debug_imgverbose || g_debug_dust_seek) {
		fprintf(stderr, "TamanoirImgProc::%s:%d : count=%d>diffref=%d / nbpix=%d / area=%d"
				" => eqdiff=(%g)/(%g)\n",
				__func__, __LINE__,
				diffcount, (int)diffref, nb, pcorrection->area,
				(float)(diffcount - pcorrection->area ),
				(float)((rect_rmax - rect_rmin)*(rect_cmax - rect_cmin))
				);
	}

	pcorrection->equivalent_diff = (float)(diffcount - pcorrection->area ) /
						(float)((rect_rmax - rect_rmin)*(rect_cmax - rect_cmin));

	// If more than 15% of the area has difference with median, then
	// it's a textured area, not a dust
	if(pcorrection->equivalent_diff > 0.2f) {

		if(g_debug_imgverbose || g_debug_dust_seek) {
			fprintf(stderr, "[imgproc] %s:%d : TEXTURED AREA : equivalent_diff=%g\n",
				__func__, __LINE__,
				pcorrection->equivalent_diff);
		}

		//FIXMEdisabled because it does not work very well:		return false;
	}

	return true;
}











bool TamanoirImgProc::correctedBetterThanOriginal(t_correction * pcorrection) {
	// Now check if the correction is better than original ==
	// see if after correction, there is no more dust
	//	clone image region
	tmCropImage(grayImage, tmpCropImage,
			pcorrection->crop_x, pcorrection->crop_y);

/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = 0

- tmpCropImage = crop(grayImage)
- correctImage = smooth(tmpCropImage)
*/
	// Apply found correction in cropped image
	tmCloneRegion(tmpCropImage,
		pcorrection->rel_dest_x, pcorrection->rel_dest_y, // dest
		pcorrection->rel_src_x, pcorrection->rel_src_y, // src
		pcorrection->copy_width, pcorrection->copy_height,
		tmpCropImage
		);

	// Then blur and process diff
	cvSmooth(tmpCropImage, correctImage,
		CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
		m_smooth_size, m_smooth_size );
/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = 0

- tmpCropImage = crop(grayImage).corrected
- correctImage = smooth(tmpCropImage)
*/

	tmProcessDiff(m_options.filmType,
				  tmpCropImage,// gray
				  correctImage, // median
				  dilateImage // difference
				  );
/* CROPPED IMAGES :
- cropImage = crop(diffImage)

- tmpCropImage = crop(grayImage).corrected
- correctImage = smooth(tmpCropImage)
- dilateImage = tmpCropImage-correctImage
*/

	// Threshold in difference
	CvConnectedComp after_connect;
	cvZero(correctImage);
	tmGrowRegion(
		dilateImage,
		correctImage,
		pcorrection->rel_seed_x, pcorrection->rel_seed_y,
		DIFF_CONTOUR,
		242,
		&after_connect);

/* CROPPED IMAGES :
- cropImage = crop(diffImage)

- tmpCropImage = crop(grayImage).corrected
- dilateImage = tmpCropImage-correctImage
- correctImage = grown(dilateImage)
*/
	if(after_connect.area > m_dust_area_min) {
		if(g_debug_dust_seek) {
			fprintf(stderr, "\t::%s:%d : NOT SO GOOD : AFTER CORRECTION "
					"%d,%d+%dx%d=>%d,%d "
				"area=%d => AFTER= STILL A DUST => grown:%d,%d+%dx%d area=%d > area_min=%d\n",
				__func__, __LINE__,
				pcorrection->rel_src_x, pcorrection->rel_src_y, // src
				pcorrection->copy_width, pcorrection->copy_height,
					pcorrection->rel_dest_x, pcorrection->rel_dest_y, // dest

				pcorrection->area,
				(int)after_connect.rect.x, (int)after_connect.rect.y,
				(int)after_connect.rect.width, (int)after_connect.rect.height,
				(int)after_connect.area, m_dust_area_min
				);
		}

		//FIXME : bad region growing return false;
	}

	return true;
}

/*
 * Return true if neighbourhood is empty
 */
bool TamanoirImgProc::neighbourhoodEmpty(t_correction * pcorrection)
{
	// test blurred input image
	tmCropImage(grayImage, tmpCropImage,
			pcorrection->crop_x, pcorrection->crop_y);

/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = tmpCropImage-correctImage

- tmpCropImage = crop(grayImage)
- correctImage = grown(dilateImage)
*/


	int l_smooth_size = 2*m_smooth_size + 1;
	cvSmooth(tmpCropImage, //gray
				correctImage,
				CV_GAUSSIAN, //int smoothtype=CV_GAUSSIAN,
				l_smooth_size, l_smooth_size );

/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = tmpCropImage-correctImage

- tmpCropImage = crop(grayImage)
- correctImage = smooth(tmpCropImage)
*/
	int rect_cmin = tmmax(0,
						  tmmin(pcorrection->rel_src_x - pcorrection->copy_width,
							pcorrection->rel_dest_x - pcorrection->copy_width)),
		rect_rmin = tmmax(0,
						  tmmin(pcorrection->rel_src_y - pcorrection->copy_height,
							pcorrection->rel_dest_y - pcorrection->copy_height)),
		rect_cmax = tmmin(cropImage->width,
						  tmmax(pcorrection->rel_src_x + pcorrection->copy_width,
							pcorrection->rel_dest_x + pcorrection->copy_width)),
		rect_rmax = tmmin(cropImage->height,
						  tmmax(pcorrection->rel_src_y + pcorrection->copy_height,
							pcorrection->rel_dest_y + pcorrection->copy_height));

	int smooth_cmin = tmmax(l_smooth_size+1,
							rect_cmin - l_smooth_size);
	int smooth_rmin = tmmax(l_smooth_size+1,
							rect_rmin - l_smooth_size);
	int smooth_cmax = tmmin(cropImage->width - l_smooth_size-1,
							rect_cmax + l_smooth_size-1);
	int smooth_rmax = tmmin(cropImage->height - l_smooth_size-1,
							rect_rmax + l_smooth_size-1);
	u8 smooth_pixmin = 255, smooth_pixmax = 0;
	// Check it's smoothness
	for(int r=smooth_rmin ; r<smooth_rmax; r++) {
		if(r<rect_rmin || r>rect_rmax) {
			u8 * smoothLine = (u8 *)(correctImage->imageData
									 + r * correctImage->widthStep);
			//u8 * dilateImageLine = (u8 *)(dilateImage->imageData
			//							  + r*dilateImage->widthStep);
			for(int c=smooth_cmin; c<smooth_cmax; c++) {
				if(c<rect_cmin || c>rect_cmax) {
					//
					u8 pix = smoothLine[c];
					if(pix) {
						if(pix > smooth_pixmax) {
							smooth_pixmax = pix;
						}
						if(pix < smooth_pixmin) {
							smooth_pixmin = pix;
						}
					}
				}
			}
		}
	}

	int smooth_diff = (int)(smooth_pixmax - smooth_pixmin);
	pcorrection->smooth_diff = smooth_diff;

	if(smooth_diff < 0) {
		fprintf(stderr, "\t%s:%d : ERROR: smooth "
				"in %d,%d - %d,%d  (rect=%d,%d - %d,%d smooth_size=%d)"
				" copy : %dx%d"
				"=> min/max=%d / %d => diff=%d\n",
			__func__, __LINE__,
			smooth_cmin, smooth_rmin,
				smooth_cmax, smooth_rmax,
			rect_cmin, rect_rmin, rect_cmax, rect_rmax,
				l_smooth_size,
				pcorrection->copy_width, pcorrection->copy_height,
			(int)smooth_pixmin, (int)smooth_pixmax,
			smooth_diff);
	}
	if(smooth_diff > 20) {
		/*
		fprintf(stderr, "\t%s:%d : smooth min/max=%d / %d => diff=%d\n",
			__func__, __LINE__,
			(int)smooth_pixmin, (int)smooth_pixmax,
			smooth_diff);
		*/

		// Mark failure on displayImage
		if( g_debug_imgoutput &&
			displayImage) {
			// just one dot
			int disp_x = (pcorrection->crop_x + pcorrection->rel_seed_x)
						 * displayImage->width / grayImage->width;
			int disp_y = (pcorrection->crop_y + pcorrection->rel_seed_y)
						 * displayImage->height / grayImage->height;
			int disp_w = 0; //crop_connect.rect.width * displayImage->width / grayImage->width;
			int disp_h = 0; //crop_connect.rect.height * displayImage->height / grayImage->height;
			tmMarkFailureRegion(displayImage,
					disp_x, disp_y, disp_w, disp_h, COLORMARK_FAILED);
			tmMarkFailureRegion(displayBlockImage,
					disp_x, disp_y, disp_w, disp_h, COLORMARK_FAILED);
		}

		return false;
	}

	return true;
}

/* Test if source and destination are different */
bool TamanoirImgProc::srcDifferentFromDest(t_correction * pcorrection)
{
// Dust and correction must be different (for avoiding periodical patterns)
	// process correlation without mask
	cvZero(dilateImage);

	int maxdiff = 100, nbdiff =  0;
	float l_dist = tmCorrelation(correctColorImage, correctColorImage,
			dilateImage,
			pcorrection->rel_dest_x, pcorrection->rel_dest_y,
			pcorrection->rel_src_x, pcorrection->rel_src_y,
			pcorrection->copy_width, pcorrection->copy_height,
			100,
			&maxdiff,
			&nbdiff
			);

	pcorrection->srcdest_correl = l_dist;
	pcorrection->srcdest_maxdiff = maxdiff;
/* CROPPED IMAGES :
- cropImage = crop(diffImage)
- dilateImage = 0

- tmpCropImage = crop(medianImage)
- correctImage = smooth(tmpCropImage)
*/
	if(l_dist < tmmin(10, fabsf(pcorrection->bg_diff))
		|| maxdiff < tmmin(10, fabsf(pcorrection->bg_diff))
		) {
		if(g_debug_imgverbose) {
			fprintf(stderr, "\t::%s:%d : SRC/DEST are similar : PERIODICAL PATTERN ?? "
				"\t seed(rel):%d,%d + copy=%dx%d "
				"=> dist=%g maxdif=%d / bgdiff=%g between src and dest\n",
				__func__, __LINE__,
				pcorrection->rel_seed_x, pcorrection->rel_seed_y,
					pcorrection->copy_width, pcorrection->copy_height,
				l_dist, maxdiff, pcorrection->bg_diff);
		}

		return false;
	}
	return true;
}



/*
 * Dilate Dust to have a better segmentation
 */
bool TamanoirImgProc::dilateDust(
	int crop_x, int crop_y,
	int crop_center_x, int crop_center_y,
	CvConnectedComp *pcrop_connect,
	bool is_a_dust, bool force_search,
	IplImage * dilateImageOut,
	int * pbest_correl,
	float * pmean_dust, float * pmean_neighbour)
{
	CvConnectedComp crop_connect = *pcrop_connect;
	// ------- Cropped images are used to lower the memory needs

/*	if(g_debug_savetmp) {
		tmCropImage(medianImage, cropImage,
					crop_x, crop_y);
		tmSaveImage(TMP_DIRECTORY "crop1-medianImage" IMG_EXTENSION,
					cropImage);
	}
*/
	tmCropImage(grayImage, cropImage,
				crop_x, crop_y);
	if(g_debug_savetmp) {
		tmSaveImage(TMP_DIRECTORY "crop1-grayImage" IMG_EXTENSION,
					cropImage);
	}
	CvConnectedComp flood_connect;
	memset(&flood_connect, 0, sizeof(CvConnectedComp));

	unsigned char seedValue = *(u8 *)(
			(cropImage->imageData + crop_center_y * cropImage->widthStep)
				+ crop_center_x);

	tmFloodRegion(
		cropImage,
		dilateImage,
		crop_center_x, crop_center_y,
		seedValue,
		20,
		255,
		&flood_connect);

	TMIMG_printf(TMLOG_DEBUG, "Seed=%d,%d is_a_dust=%c => flood=%d,%d+%dx%d",
				 crop_center_x, crop_center_y,
				 is_a_dust?'T':'F',
				 flood_connect.rect.x, flood_connect.rect.y,
				 flood_connect.rect.width, flood_connect.rect.height
				 );
/* CROPPED IMAGES :
	- cropImage = crop(grayImage)
	- dilateImage = grown(cropImage)
	*/

	// Grow again region in cropped image
	tmCropImage(diffImage, cropImage,
				crop_x, crop_y);
	/* CROPPED IMAGES :
		- cropImage = crop(diffImage)
		- dilateImage = grown(cropImage)
		*/
	if(g_debug_savetmp) {
		tmSaveImage(TMP_DIRECTORY "crop1-diffImage" IMG_EXTENSION,
					cropImage);
		tmSaveImage(TMP_DIRECTORY "crop1-diffImage-uncropped" IMG_EXTENSION,
					diffImage);
	}

	// clear grow buffer
	cvZero(correctImage);

	// Grow again region in cropped image
	tmGrowRegion(
		cropImage,
		correctImage,
		crop_center_x, crop_center_y,
		(force_search ? DIFF_NOT_DUST : DIFF_THRESHVAL),
		255,
		&crop_connect
		);
	if(g_debug_savetmp) {
		tmSaveImage(TMP_DIRECTORY "crop1-grownImage" IMG_EXTENSION,
					cropImage);
	}
	TMIMG_printf(TMLOG_DEBUG, "Seed=%d,%d => grow=%d,%d+%dx%d",
			 crop_center_x, crop_center_y,
			 crop_connect.rect.x, crop_connect.rect.y,
			 crop_connect.rect.width, crop_connect.rect.height
			 )

	/* CROPPED IMAGES :
	- cropImage = crop(diffImage)
	- correctImage = grown(cropImage)
	*/
	*pcrop_connect = crop_connect;

	// Check if the region is just a singular point (a corner...)
// FIXME :
//	cvZero(dilateImage);
	/* Do a contrained region growing around a seed with similar value *
	void tmFloodRegion(unsigned char * growIn, unsigned char * growOut,
		int swidth, int sheight,
		int c, int r,
		unsigned char seedValue,
		unsigned char threshold,
		unsigned char fillValue,
		CvConnectedComp * areaOut);
	*/



	if(g_debug_imgverbose > 1 || g_debug_dust_seek) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : seedValue=%d "
				"=> grown dust at %d,%d+%dx%d surf=%d "
				"=> flooded at %d,%d+%dx%d surf=%d\n",
			__func__, __LINE__,
			(int)seedValue,
			crop_connect.rect.x, crop_connect.rect.y,
			crop_connect.rect.width, crop_connect.rect.height,
			(int)crop_connect.area,
			flood_connect.rect.x, flood_connect.rect.y,
			flood_connect.rect.width, flood_connect.rect.height,
			(int)flood_connect.area
			);
	}

	TMIMG_printf(TMLOG_DEBUG, "seed %d,%d Value=%d "
				 "=> grown dust at %d,%d+%dx%d surf=%d "
				 "=> flooded at %d,%d+%dx%d surf=%d\n",
				 crop_center_x, crop_center_y,
				 (int)seedValue,
				 crop_connect.rect.x, crop_connect.rect.y,
				 crop_connect.rect.width, crop_connect.rect.height,
				 (int)crop_connect.area,
				 flood_connect.rect.x, flood_connect.rect.y,
				 flood_connect.rect.width, flood_connect.rect.height,
				 (int)flood_connect.area
				 );

	m_dust_detection_props.flood_area = flood_connect.area;

	if( (crop_connect.area < m_dust_area_min
		 || crop_connect.area > m_dust_area_max)
	   && !( force_search && crop_connect.area>0)
		)
	{
		TMIMG_printf(TMLOG_DEBUG, "Too big>max=%d or too small<min=%d",
					 m_dust_area_min, m_dust_area_max)
		is_a_dust = false;
	}
	else {
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

		bool is_a_fiber =
				(crop_connect.area < crop_connect.rect.width *
									crop_connect.rect.height / 3);
		if(is_a_fiber && force_search)
		{
			TMIMG_printf(TMLOG_DEBUG, "force_search => FIBER !! "
						 "area=%g = %g %% of rect\n",
						crop_connect.area,
						100.f * crop_connect.area
							/( crop_connect.rect.width * crop_connect.rect.height )
						)
		}


		m_dust_detection_props.is_fiber = is_a_fiber;

		// Check if copy is not along a contour
		//int dx = abs(copy_src_x - copy_dest_x);
		//int dy = abs(copy_src_y - copy_dest_y);
		//if(dx < dy) // Check if we are not copying while following a border
		//	cvSobel(correctImage, sobelImage, 1, 0, 5);
		//else
		//	cvSobel(correctImage, sobelImage, 0, 1, 5);

		if(g_debug_savetmp) {

			tmCropImage(erodedImage,
				tmpCropImage,
				crop_x, crop_y);
			tmSaveImage(TMP_DIRECTORY "crop2-eroded" IMG_EXTENSION, tmpCropImage);
			tmCropImage(dilatedImage,
				tmpCropImage,
				crop_x, crop_y);
			tmSaveImage(TMP_DIRECTORY "crop2-dilated" IMG_EXTENSION, tmpCropImage);
		}

		// Crop gray image into cropImage
		tmCropImage(grayImage,
			tmpCropImage,
			crop_x, crop_y);
/* CROPPED IMAGES :
	- cropImage = crop(diffImage)
	- correctImage = grown(cropImage/diffImage)
	- tmpCropImage = crop(grayImage)

	*/
		if(g_debug_savetmp) {
			tmSaveImage(TMP_DIRECTORY "crop3-in" IMG_EXTENSION, cropImage);
			tmSaveImage(TMP_DIRECTORY "crop4-grown" IMG_EXTENSION, correctImage);
		}

		tmCropImage(diffImageUnclosed,
			cropImage,
			crop_x, crop_y);

		IplImage * crop_diffImageUnclosed = cropImage;

/* CROPPED IMAGES :
	- cropImage = crop(diffImageUnclosed)
	- correctImage = grown(diffImage)
	- tmpCropImage = crop(grayImage)
	- dilateImage = dilate(correctImage)
	*/

		if(!g_mode_use_erodedilate) {
			// Do a dilatation around the grown region (cropped from diffImage)
			int kernel_size = 2 * (m_options.dpi / 2400);
			//if(tmmax(crop_connect.width, crop_connect.height) > 3.f) {
				// use proportional
			//}
			kernel_size = kernel_size*2 + 1;
			if(kernel_size<3) { kernel_size = 3; }
			if(g_debug_dust_seek) {
				fprintf(stderr, "\t::%s:%d : dilate: kernel size=%d\n",
						__func__, __LINE__, kernel_size);
			}

			tmDilateImage(cropImage, dilateImageOut,
						  kernel_size,
						  1 // iteration
						  );

			if(g_debug_savetmp) {
				tmSaveImage(TMP_DIRECTORY "crop5-grown-dilated" IMG_EXTENSION, dilateImageOut);
			}
		} else {
			// copy only
			cvCopy(correctImage, dilateImageOut);
			if(g_debug_savetmp) {
				tmSaveImage(TMP_DIRECTORY "crop5-dilateImageOut" IMG_EXTENSION, dilateImageOut);
			}
		}

		if(g_debug_savetmp) {
			tmSaveImage(TMP_DIRECTORY "crop5-diffUnclosed" IMG_EXTENSION, dilateImageOut);
		}

		// => this dilated image will be used as mask for correlation search
		// but we have to fill the center of the dust
		//	A full circle will only give us a empty circle : O

		int r,c, histoDust[512];
		memset(histoDust, 0, 512*sizeof(int));

		IplImage * crop_grayImage = tmpCropImage;
		if(m_options.filmType == FILM_POSITIVE) {
			// Dark dusts are visible on eroded image
			tmCropImage(erodedImage,
				tmpCropImage,
				crop_x, crop_y);
		} else if(m_options.filmType == FILM_NEGATIVE) {
			// Dark dusts are visible on eroded image
			tmCropImage(dilatedImage,
				tmpCropImage,
				crop_x, crop_y);
		}

		int dust_rmin = tmmax(crop_connect.rect.y, 0);
		int dust_rmax = tmmin(crop_connect.rect.y+crop_connect.rect.height, dilateImageOut->height);
		int dust_cmin = tmmax(crop_connect.rect.x, 0);
		int dust_cmax = tmmin(crop_connect.rect.x+crop_connect.rect.width, dilateImageOut->width);




		for(r=dust_rmin;r<dust_rmax; r++) {
			u8 * diffUnclosed = IPLLINE_8U(crop_diffImageUnclosed, r);
			u8 * cropGrayBuffer = IPLLINE_8U(crop_grayImage, r);

			for(c=dust_cmin; c<dust_cmax; c++) {
				if( diffUnclosed[c] == DIFF_THRESHVAL) {
				//if( dilateImageBuffer[crop_pos] == 255) { // In dilated region
					int gray = (int)cropGrayBuffer[c];

					// Difference between image and median
					histoDust[ gray ]++;
				}
			}
		}

		// get min/max of pixel values
		int min_dust = 255;
		int max_dust = 0;
		double sum_dust = 0.;
		int nb_dust = 0;
		for(int h=0; h<256; h++) {
			if(histoDust[h]>0) {
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
		if(g_debug_dust_seek) {
			fprintf(stderr, "::%s:%d : fill min/max = %d / %d : mean=%g\n",
					__func__, __LINE__,
					min_dust, max_dust,
					sum_dust);
		}

		/* CROPPED IMAGES :
			- cropImage = crop(diffImageUnclosed)
			- correctImage = grown(cropImage/diffImage)
			- tmpCropImage = crop(grayImage)
			- dilateImage = dilate(correctImage)
			*/
		for(r=dust_rmin;r<dust_rmax; r++) {
			u8 * cropGrayBuffer = IPLLINE_8U(crop_grayImage, r);
			u8 * dilateImageBuffer = IPLLINE_8U(dilateImageOut, r);
			//int crop_pos = r*tmpCropImage->widthStep + dust_cmin;
			u8 active = 0;
			for(c=dust_cmin; c<dust_cmax; c++) {
				if(dilateImageBuffer[c]==255) { active = 1; }
				else if(active //&& !dilateImageBuffer[c]
				   && cropGrayBuffer[c]>=min_dust
				   && cropGrayBuffer[c]<=max_dust) {
					dilateImageBuffer[c] = 10;
				}
			}

			active = 0;
			//crop_pos = r*tmpCropImage->widthStep + dust_cmax-1;
			for(c=dust_cmax-1; c>=dust_cmin; c--) {
				if(dilateImageBuffer[c]==255) { active = 1; }

				if(active && dilateImageBuffer[c]==10
				   && cropGrayBuffer[c]>=min_dust
				   && cropGrayBuffer[c]<=max_dust) {
					dilateImageBuffer[c] = 20;
				}
			}
		}

		int pitch = tmpCropImage->widthStep;
		for(c=dust_cmin; c<dust_cmax; c++) {
			//int crop_pos = dust_rmin*pitch + c;
			u8 active = 0;
			for(r=dust_rmin; r<dust_rmax; r++) {
				u8 * dilateImageBuffer = IPLLINE_8U(dilateImageOut, r);
				if(dilateImageBuffer[c]==255) { active = 1; }

				u8 * cropGrayBuffer = IPLLINE_8U(tmpCropImage, r);
				if(active && dilateImageBuffer[c]==20
				   && cropGrayBuffer[c]>=min_dust
				   && cropGrayBuffer[c]<=max_dust) {
					dilateImageBuffer[c] = 30;
				}
			}

			//crop_pos = (dust_rmax-1)*pitch + c;
			active = 0;
			for(r=dust_rmin; r<dust_rmax; r++) {
				u8 * dilateImageBuffer = IPLLINE_8U(dilateImageOut, r);
				u8 * cropGrayBuffer = IPLLINE_8U(tmpCropImage, r);

				if(dilateImageBuffer[c]==255) { active = 1; }

				if(active && dilateImageBuffer[c]==30
				   && cropGrayBuffer[c]>=min_dust
				   && cropGrayBuffer[c]<=max_dust) {
					dilateImageBuffer[c] = 235;
				}
			}
		}

		if(g_debug_savetmp) {
			tmSaveImage(TMP_DIRECTORY "crop6-dilate-filled" IMG_EXTENSION, dilateImageOut);
		}

		int border = tmmax(tmmax(crop_connect.rect.width, crop_connect.rect.height),
						16 * m_options.dpi / 2400);
		int neighbour_rmin = tmmax(crop_connect.rect.y - border, 0);
		int neighbour_rmax = tmmin(crop_connect.rect.y+crop_connect.rect.height+border, dilateImageOut->height);
		int neighbour_cmin = tmmax(crop_connect.rect.x - border, 0);
		int neighbour_cmax = tmmin(crop_connect.rect.x+crop_connect.rect.width+border, dilateImageOut->width);
		double sum_neighbour = 0.;

		int best_correl = 100;
		// Check if this region is not the film grain by cheching the local
		// variance and difference
		if(is_a_dust
	//			   && !force_search
		   ) {
			int histoNeighbour[512];
			// Update dust info
			memset(histoNeighbour, 0, 512*sizeof(int));
			int histoDustDil[512];
			memset(histoDustDil, 0, 512*sizeof(int));
			if(force_search
			   && g_debug_dust_seek) {
				fprintf(stderr, "TamanoirImgProc::%s:%d : Dust: %d,%d + %dx%d\n",
						__func__, __LINE__,
						(int)crop_connect.rect.x, (int)crop_connect.rect.y,
						(int)crop_connect.rect.width, (int)crop_connect.rect.height
						);
				fprintf(stderr, "\tDust: min/max / mean = %d-%d / %g (nb=%d)\n",
						min_dust, max_dust, sum_dust, nb_dust);
			}


			int diff_min = 255;
			int diff_max = -255;
			double diff_sum = 0.;
			int diff_nb = 0;

			for(r=neighbour_rmin;r<neighbour_rmax; r++) {
				//int crop_pos = r*tmpCropImage->widthStep + neighbour_cmin;
				u8 * cropGrayBuffer = IPLLINE_8U(tmpCropImage, r);
				u8 * dilateImageBuffer = IPLLINE_8U(dilateImageOut, r);

				for(c=neighbour_cmin; c<neighbour_cmax; c++) {
					int gray = (int)cropGrayBuffer[c];

					if(dilateImageBuffer[c]>=235) {
						// Difference between image and median
						histoDustDil[ gray ]++;
					} else {

						// outside the grown dust area
						histoNeighbour[ gray ]++;
//dilateImageBuffer[c] = 42;
						// outside the dust,
						//if( grownImageBuffer[crop_pos] == 255 )
						{
							int diff = (int)roundf(sum_dust) - gray;
							if(diff < diff_min) diff_min = diff;
							if(diff > diff_max) diff_max = diff;

							diff_sum += diff;
							diff_nb++;
						}
					}
				}
			}
			if(g_debug_savetmp) {
				tmSaveImage(TMP_DIRECTORY "crop7-neighbour" IMG_EXTENSION, dilateImageOut);
			}
			// diff_sum is the mean of difference between dust and it's neighbourhood
			if(diff_nb) diff_sum /= (double)diff_nb;


			int min_dustDil = 255;
			int max_dustDil = -255;
			double sum_dustDil = 0.;
			int nb_dustDil = 0;
			for(int h=0; h<256; h++) {
				if(histoDustDil[h]>0)
				{
					sum_dustDil += (double)(histoDustDil[h]*h);
					nb_dustDil += histoDustDil[h];
					if(h<min_dustDil) { min_dustDil=h; }
					if(h>max_dustDil) { max_dustDil=h; }
				}
			}
			if(nb_dustDil>0) {
				sum_dustDil /= (double)nb_dustDil;
			}

			int min_neighbour = 255;
			int max_neighbour = 0;
			sum_neighbour = 0.;
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

			best_correl = 5 + max_neighbour - min_neighbour;
			// Limit best_correl because it cas go real high (ex: 114 )
			if(best_correl>50) {
				best_correl = 50;
			}
			if(pbest_correl ) {
				*pbest_correl = best_correl;
			}
			float contrast = (sum_dust - sum_neighbour )
							 / (sum_neighbour + sum_dust);

			if(force_search) {
#ifdef SIMPLE_VIEWER
				// Force search to be more selective
				if(!g_debug_dust_seek) {
					best_correl = 20;
				}

#endif
			}
			if(g_debug_dust_seek) {
				fprintf(stderr, "\tDust(Dilated): min-max / mean = %d..%d / %g (nb=%d)\n",
						min_dustDil, max_dustDil, sum_dustDil, nb_dustDil);
				fprintf(stderr, "\tNeighbour: min-max / mean = %d..%d / %g / tolerance=%d\n",
						min_neighbour, max_neighbour, sum_neighbour, best_correl);
				fprintf(stderr, "\tDiff/Neighbour: min/max / mean = %d..%d / %g\n",
						diff_min, diff_max, diff_sum);
				fprintf(stderr, "\tContrast = %g / %g = %g\n",
						(sum_dust - sum_neighbour), (sum_neighbour + sum_dust), contrast);
			}



			m_dust_detection_props.mean_neighbour = sum_neighbour;
			m_dust_detection_props.mean_dust = sum_dust;
			m_dust_detection_props.contrast = contrast;

			m_dust_detection_props.visible_enough = 0;

			if(1) {// not effective since we use dilated images as inputs
				if(g_debug_dust_seek) {
					fprintf(stderr, "%s:%d\t ===> is a dust = %c dust=%g / neigh=%g |diff=%g| > 10 ?/ C=%g >0.2\n",
						__func__, __LINE__,
							(is_a_dust ? 'T' : 'F'),
							sum_dust, sum_neighbour,
							fabsf(sum_dust-sum_neighbour),
							contrast
							);
				}
#define DIFFMIN_BG2DUST	10.f
				// test if colour is different enough
				switch(m_options.filmType) {
				default:
				case FILM_UNDEFINED:
					if( fabs(sum_neighbour - sum_dust) <= DIFFMIN_BG2DUST
					   || fabsf(contrast)<0.02f) { //m_threshold
						// no difference visible => not a dust
						is_a_dust = false;
					} else {
						m_dust_detection_props.visible_enough = 1;
					}
					break;
				case FILM_POSITIVE:
					if( sum_dust >= sum_neighbour - DIFFMIN_BG2DUST
						|| contrast>-0.02f) { //m_threshold
						// not darker than neighbourhood  => not a dust
						is_a_dust = false;
					} else {
						m_dust_detection_props.visible_enough = 1;
					}
					break;
				case FILM_NEGATIVE:
					if( sum_dust <= sum_neighbour + DIFFMIN_BG2DUST
						|| contrast < 0.02f) { //m_threshold
						// not lighter than neighbourhood  => not a dust
						is_a_dust = false;
					} else {
						m_dust_detection_props.visible_enough = 1;
					}
					break;
				}
			}

			if(force_search && !is_a_dust) {
				if(g_debug_dust_seek) {
					fprintf(stderr, "%s:%d\t ===> is a dust = %c => force true here\n",
						__func__, __LINE__,
							(is_a_dust ? 'T' : 'F')
							);
				}

				is_a_dust = true;
			}

		}

		if(pmean_dust) {
			*pmean_dust = sum_dust;
		}
		if(pmean_neighbour) {
			*pmean_neighbour = sum_neighbour;
		}
	}


	return is_a_dust;
}






void TamanoirImgProc::cropViewImages() {
	cropCorrectionImages(m_correct);
}

void TamanoirImgProc::lockInpaintDrawing(bool lock) {
	m_inpainting_lock = lock;
}

void TamanoirImgProc::drawInpaintCircle(t_correction correction) {
	if(!undoImage) {
		setCopySrc(&correction, correction.rel_seed_x, correction.rel_seed_y);
	}

	if(!undoImage) {
		TMIMG_printf(TMLOG_ERROR, "no undoImage ! return")
		return;
	}

	if(	m_last_mode != TMMODE_INPAINT) {
		// we switched mode, so reupdate undoImage because we will use undoImage to perform inpainting
		recropImages(cvSize(undoImage->width, undoImage->height), correction.crop_x, correction.crop_y);
	}

	m_last_mode = TMMODE_INPAINT;
	CvSize cropSize = cvSize(undoImage->width, undoImage->height);




	int radius = correction.copy_width / 2 ;
	TMIMG_printf(TMLOG_DEBUG, "draw in dilate : %d,%d r=%d ",
			correction.rel_seed_x, correction.rel_seed_y,
			radius
			)

	// Set the rendered flag to false, to force rendering later
	m_inpainting_rendered = false;

	// Draw in dilate image
	cvCircle(inpaintMaskImage,
			 cvPoint(correction.rel_seed_x, correction.rel_seed_y),
			 radius, cvScalarAll(255), -1);

	// and draw in cropped image
	cvCircle(disp_cropColorImage,
			 cvPoint(correction.rel_seed_x, correction.rel_seed_y),
			 radius,
			 getFakeColor(disp_cropColorImage->nChannels, COLORMARK_CURRENT),
			 -1);
}


void TamanoirImgProc::recropImages(CvSize cropSize, int crop_x, int crop_y) {
	// purge if size has changed
	PURGE_SIZECHANGED( disp_cropImage, cropSize)
	PURGE_SIZECHANGED( disp_cropColorImage, cropSize)
	PURGE_SIZECHANGED( disp_cropOrigImage, cropSize)
	PURGE_SIZECHANGED( disp_correctColorImage, cropSize)
	PURGE_SIZECHANGED( disp_dilateImage, cropSize)

	PURGE_SIZECHANGED( undoImage, cropSize)
	PURGE_SIZECHANGED( undoDiffImage, cropSize)
	PURGE_SIZECHANGED( inpaintMaskImage, cropSize)
	PURGE_SIZECHANGED( inpaintRenderImage, cropSize)

	// Crop original, undo, ...
	// Allocate images
	if(!disp_cropOrigImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc disp_cropOrigImage %dx%d", cropSize.width, cropSize.height)
		disp_cropOrigImage = tmCreateImage(cropSize, IPL_DEPTH_8U, originalImage->nChannels);
	}
	if(!disp_cropColorImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc disp_cropColorImage %dx%d", cropSize.width, cropSize.height)
		disp_cropColorImage = tmCreateImage(cropSize, IPL_DEPTH_8U, originalImage->nChannels);
	}

	// Crop original image for display in GUI
	tmCropImage(originalImage, disp_cropOrigImage,
				crop_x, crop_y,
				true // threshold for not having highlights
				);
	cvCopy( disp_cropOrigImage, disp_cropColorImage);

	if(!disp_correctColorImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc disp_correctColorImage %dx%d", cropSize.width, cropSize.height)
		disp_correctColorImage = tmCreateImage(cropSize,IPL_DEPTH_8U, originalImage->nChannels);
	}
	cvCopy( disp_cropOrigImage, disp_correctColorImage);

	if(!disp_dilateImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc disp_dilateImage %dx%d", cropSize.width, cropSize.height)
		disp_dilateImage = tmCreateImage(cropSize,IPL_DEPTH_8U, 1);
	}


	TMIMG_printf(TMLOG_DEBUG, "x,y+wxh=%d,%d + %dx%d",
				  crop_x, crop_y, cropSize.width, cropSize.height)

	// Crop undo images
	undoImage_x = crop_x;
	undoImage_y = crop_y;
	if(!undoImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc undoImage %dx%d", cropSize.width, cropSize.height)
		undoImage = tmCreateImage(cvSize(cropSize.width, cropSize.height),
							  originalImage->depth, originalImage->nChannels);
	}

	if(!undoDiffImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc undoDiffImage %dx%d", cropSize.width, cropSize.height)
		undoDiffImage = tmCreateImage(cvSize(cropSize.width, cropSize.height),
							  IPL_DEPTH_8U, 1);
	}

	tmCropImage(originalImage, undoImage,
				undoImage_x, undoImage_y);
	tmCropImage(diffImage, undoDiffImage,
				undoImage_x, undoImage_y);

	if(!inpaintMaskImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc inpaintMaskImage %dx%d", cropSize.width, cropSize.height)
		inpaintMaskImage = tmCreateImage(cvSize(undoImage->width, undoImage->height), IPL_DEPTH_8U, 1);
	} else {
		cvZero(inpaintMaskImage);
	}
	if(!inpaintRenderImage) {
		TMIMG_printf(TMLOG_TRACE, "realloc inpaintRenderImage %dx%d", cropSize.width, cropSize.height)
		inpaintRenderImage = tmCreateImage(cvSize(undoImage->width, undoImage->height),
										   //originalImage->depth,
										   // cvInpaintImage only supports IPL_DEPTH_8U
										   IPL_DEPTH_8U,
										   originalImage->nChannels);
	}
}

/** @brief Approximate a correction using params from last crop

Check if last crop is still ok for new crop*/
t_correction TamanoirImgProc::approxCorrection(t_correction correct_in) {
	if(	!undoImage ) return correct_in;

	int crop_max_x = undoImage_x + undoImage->width;
	if(correct_in.crop_x > crop_max_x) { return correct_in; }
	int crop_max_y = undoImage_y + undoImage->height;
	if(correct_in.crop_y > crop_max_y) { return correct_in; }

	// Use a margin
	int crop_min_x = undoImage_x + correct_in.copy_width;
	int crop_min_y = undoImage_y + correct_in.copy_height;
	crop_max_x -= correct_in.copy_width;
	crop_max_y -= correct_in.copy_height;

	// Check if last crop is still ok for new crop
	// we need to have bove dest and src in range
	int src_x = correct_in.crop_x + correct_in.rel_src_x ;
	int src_min_x = src_x - correct_in.copy_width;
	if(src_min_x < crop_min_x) { return correct_in; }
	int src_max_x = src_x + correct_in.copy_width;
	if(src_max_x > crop_max_x) { return correct_in; }

	int src_y = correct_in.crop_y + correct_in.rel_src_y ;
	int src_min_y = src_y - correct_in.copy_height;
	if(src_min_y < crop_min_y) { return correct_in; }
	int src_max_y = src_y + correct_in.copy_height;
	if(src_max_y > crop_max_y) { return correct_in; }

	// same check with dest
	int dest_x = correct_in.crop_x + correct_in.rel_dest_x ;
	int dest_min_x = dest_x - correct_in.copy_width;
	if(dest_min_x < crop_min_x) { return correct_in; }
	int dest_max_x = dest_x + correct_in.copy_width;
	if(dest_max_x > crop_max_x) { return correct_in; }

	int dest_y = correct_in.crop_y + correct_in.rel_dest_y ;
	int dest_min_y = dest_y - correct_in.copy_height;
	if(dest_min_y < crop_min_y) { return correct_in; }
	int dest_max_y = dest_y + correct_in.copy_height;
	if(dest_max_y > crop_max_y) { return correct_in; }

	// Ok, keep last undo
	int dx = undoImage_x - correct_in.crop_x;
	int dy = undoImage_y - correct_in.crop_y;
	correct_in.crop_x += dx;
	correct_in.crop_y += dy;

	correct_in.rel_src_x -= dx;
	correct_in.rel_src_y -= dy;
	correct_in.rel_dest_x -= dx;
	correct_in.rel_dest_y -= dy;
	correct_in.rel_seed_x -= dx;
	correct_in.rel_seed_y -= dy;

	return correct_in;
}

void TamanoirImgProc::cropCorrectionImages(
		t_correction correction,
		int correct_mode
		)
{
	if(correction.copy_width <= 0) {
		return;
	}

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

	if(processingSize.width <=0 || processingSize.height <= 0) {
		fprintf(logfile, "TamanoirImgProc::%s:%d : error in processingSize %dx%d\n",
				__func__, __LINE__, processingSize.width, processingSize.height);
		return;
	}

	CvSize cropSize = processingSize;
	if(displayCropSize.width > 0 && displayCropSize.height > 0
	   && (cropSize.width != displayCropSize.width
			|| cropSize.height != displayCropSize.height) ) {
		TMIMG_printf(TMLOG_DEBUG, "cropSize=%dx%d != displayCropSize=%dx%d => purgeDisplay and resize",
					 cropSize.width, cropSize.height,
					 displayCropSize.width, displayCropSize.height)

		cropSize = displayCropSize;
	}

	if(correction.crop_width > 0 && correction.crop_height > 0) {
		if(cropSize.width != correction.crop_width
		   || cropSize.height != correction.crop_height) {
			TMIMG_printf(TMLOG_DEBUG, "cropSize=%dx%d != correction.crop_widthxheight=%dx%d => purgeDisplay and resize",
						 cropSize.width, cropSize.height,
						 correction.crop_width, correction.crop_height)

			// maybe not a good idea...
			displayCropSize = cropSize = cvSize( correction.crop_width, correction.crop_height );
		}
	}

	if(disp_dilateImage) {
		if(disp_dilateImage->width != cropSize.width
			|| disp_dilateImage->height != cropSize.height) {
/*
			fprintf(stderr, "[imgproc]::%s:%d : size does not fit : "
					"disp_dilateImage=%dx%d != "
					"cropSize=%dx%d\n",
					__func__, __LINE__,
					disp_dilateImage->width, disp_dilateImage->height,
					cropSize.width, cropSize.height);
*/
			// maybe not a good idea...
			displayCropSize = cropSize = cvSize( correction.crop_width, correction.crop_height );
		}
	}

	if(disp_cropOrigImage
	   && (cropSize.width != disp_cropOrigImage->width || cropSize.height!=disp_cropOrigImage->height)
	   ) {
		tmReleaseImage(&disp_cropOrigImage);
	}

	if(!disp_cropOrigImage) {
		// some size changed, purge every cropped image
		recropImages(cropSize, correction.crop_x,  correction.crop_y);
	}

	if(	undoImage && m_last_mode != correct_mode) {
		TMIMG_printf(TMLOG_TRACE, "undoImage && m_last_mode != correct_mode => recropImages")
		// we switched mode, so reupdate undoImage because we will use undoImage to perform inpainting
		recropImages(cvSize(undoImage->width, undoImage->height), correction.crop_x, correction.crop_y);
	}

	if(	undoImage && (undoImage_x != correction.crop_x
					|| undoImage_y != correction.crop_y)) {
		TMIMG_printf(TMLOG_TRACE, "undoImage && undoImage size does not fit => recropImages")
		recropImages(cvSize(undoImage->width, undoImage->height), correction.crop_x, correction.crop_y);
	}

	m_last_mode = correct_mode;

	// Now we should have the right size of images


	if(g_debug_savetmp) {// force debug
		m_show_crop_debug = true;
	}

	if(!disp_cropImage && m_show_crop_debug) {
		disp_cropImage = tmCreateImage(cropSize,IPL_DEPTH_8U, 1);
	}
	CvConnectedComp ext_connect;

	if(m_show_crop_debug
	   && correct_mode != TMMODE_INPAINT
	   )
	{	// Grown region
		if(disp_dilateImage) {
			if(disp_dilateImage->width != cropSize.width
				|| disp_dilateImage->height != cropSize.height) {
				fprintf(stderr, "[imgproc]::%s:%d : size does not fit : %dx%d != %dx%d\n",
						__func__, __LINE__,
						disp_dilateImage->width, disp_dilateImage->height,
						cropSize.width, cropSize.height);
				tmReleaseImage(&disp_dilateImage);
			}
		}

		if(!disp_dilateImage) {
			disp_dilateImage = tmCreateImage(cropSize, IPL_DEPTH_8U, 1);
		} else {
			memset( disp_dilateImage->imageData, 0,
					disp_dilateImage->widthStep * disp_dilateImage->height );
		}

		// Get grown region
		if(correction.grown_dust.area > 0 && correctImage) {
			CvConnectedComp copy_rect = correction.grown_dust;

			// test to protect processing crop images
#ifndef SIMPLE_VIEW	// only in debug GUI
			if(disp_dilateImage->width == correctImage->width &&
				disp_dilateImage->height == correctImage->height ) {

				dilateDust(correction.crop_x, correction.crop_y,
						correction.rel_seed_x, correction.rel_seed_y,
						&copy_rect,
						false, false,
						disp_dilateImage);
			}
			else
			{
				fprintf(stderr, "[imgproc]::%s:%d : size does not fit : disp_dilateImage:%dx%d != correctImage:%dx%d\n",
						__func__, __LINE__,
						disp_dilateImage->width, disp_dilateImage->height,
						correctImage->width, correctImage->height);
#else
			{
#endif
				if(!disp_cropImage) {
					disp_cropImage = tmCreateImage(cropSize,IPL_DEPTH_8U, 1);
				}
				tmCropImage(diffImage, disp_cropImage,
					correction.crop_x, correction.crop_y);

				// Do our own region growing
				tmGrowRegion(
						disp_cropImage,
						disp_dilateImage,
						correction.rel_seed_x, correction.rel_seed_y,
						DIFF_THRESHVAL,
						255,
						&ext_connect);
			}
		} else { // Do our own region growing
			if(!disp_cropImage) {
				disp_cropImage = tmCreateImage(cropSize,IPL_DEPTH_8U, 1);
			}

			tmCropImage(diffImage, disp_cropImage,
					correction.crop_x, correction.crop_y);

			tmGrowRegion(
					disp_cropImage,
					disp_dilateImage,
					correction.rel_seed_x, correction.rel_seed_y,
					DIFF_THRESHVAL,
					255,
					&ext_connect);
		}
	}

	// Bottom-right: Diff image for display in GUI
	if(disp_cropImage) {

		tmCropImage(diffImage,
			disp_cropImage,
			correction.crop_x, correction.crop_y);

		// Display rect on diffimage to present the source and destination
		cvRectangle(disp_cropImage,
					cvPoint( tmmin(correction.rel_src_x - correction.copy_width,
								   correction.rel_dest_x - correction.copy_width),
							 tmmin(correction.rel_src_y - correction.copy_height,
								   correction.rel_dest_y - correction.copy_height) ),
					cvPoint( tmmax(correction.rel_src_x + correction.copy_width,
								   correction.rel_dest_x + correction.copy_width),
							 tmmax(correction.rel_src_y + correction.copy_height,
								   correction.rel_dest_y + correction.copy_height) ),
					cvScalarAll(254),
					1);
	}

	if(!disp_cropOrigImage) {
		disp_cropOrigImage = tmCreateImage(cropSize, IPL_DEPTH_8U, originalImage->nChannels);

		// Top-left on GUI : Original image for display in GUI
		tmCropImage(originalImage, disp_cropOrigImage,
				correction.crop_x, correction.crop_y,
				true
				);
		cvCopy(disp_cropOrigImage, disp_cropColorImage);
	}





	// Cloning : copy src->dest with an elliptic pattern
	if(correct_mode != TMMODE_INPAINT) {
		// Bottom-Left
		// If we use a blurred version for searching,
		//	update cropped color with original this time
		cvCopy(disp_cropOrigImage, disp_cropColorImage);
		cvCopy(disp_cropOrigImage, disp_correctColorImage);

		// For grayscaled images, limit color to 251
		if(disp_cropColorImage->nChannels == 1) {
			for(int r=0; r<disp_cropColorImage->height; r++) {
				u8 * buf = (u8 *)(disp_cropColorImage->imageData + disp_cropColorImage->widthStep * r);
				for(int c=0; c<disp_cropColorImage->width; c++)
					if(buf[c]>251) buf[c]=251;
			}
		}


		// Clone image region
		tmCloneRegion(disp_cropOrigImage,
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

	} // else, we are in inpaint mode, we mark the grown region in image
	else {
		// The function has already drawn the mask into disp_cropColorImage will be displayed

	//
	/*
	  Inpaint
Inpaints the selected region in the image

void cvInpaint( const CvArr* src, const CvArr* mask, CvArr* dst,
			int flags, double inpaintRadius );

src
The input 8-bit 1-channel or 3-channel image.
mask
The inpainting mask, 8-bit 1-channel image. Non-zero pixels indicate the area that needs to be inpainted.
dst
The output image of the same format and the same size as input.
flags
The inpainting method, one of the following:
CV_INPAINT_NS - Navier-Stokes based method.
CV_INPAINT_TELEA - The method by Alexandru Telea [Telea04]
inpaintRadius
The radius of circlular neighborhood of each point inpainted that is considered by the algorithm.

The function cvInpaint reconstructs the selected image area from the pixel near the area boundary. The function may be used to remove dust and scratches from a scanned photo, or to remove undesirable objects from still images or video.
	  */
		if(//IPLPIX_8U(grownImage, current_dust.rel_seed_x, current_dust.rel_seed_y)
			disp_dilateImage //m_draw_on==TMMODE_INPAINT
			) {
//			IplImage * dilatedGrownImage = cvCreateImage(
//					cvSize(disp_dilateImage->width, disp_dilateImage->height),
//					IPL_DEPTH_8U, 1);
//			cvCopy(disp_dilateImage, dilatedGrownImage);
//			tmDilateImage(dilatedGrownImage, disp_dilateImage, 5, 1);

/* already done in drawInpaintCircle() *
			if(!m_inpainting_rendered) {

				fprintf(stderr, "TmImgProc::%s:%d : insert inpainting mask...",
							__func__, __LINE__);
				for(int r=0; r<disp_dilateImage->height; r++) {
					u8 * dilateline = (u8 *)(disp_dilateImage->imageData
											 + r*disp_dilateImage->widthStep);

					for(int c=0; c<disp_dilateImage->width; c++) {
						if(dilateline[c] == 255) {
							cvLine(disp_cropColorImage,
								   cvPoint(c,r), cvPoint(c,r),
								   getFakeColor(disp_cropOrigImage->nChannels, COLORMARK_FAILED),
								   1);
						}
					}
				}
			}
* already done in drawInpaintCircle() */


			if(!m_inpainting_lock // locked while drawing
			   && !m_inpainting_rendered // and only if not already rendered
			   ) {

				if(!undoImage) { setCopySrc(&correction, correction.rel_seed_x, correction.rel_seed_y); }
				if(!undoImage) {
					TMIMG_printf(TMLOG_ERROR, "no undoImage !")
					return;
				}
				TMIMG_printf(TMLOG_DEBUG, "render inpainting"
							 " undoImage=%dx%d / crop mask=%dx%d...",
							 undoImage->width, undoImage->height,
							 inpaintMaskImage->width, inpaintMaskImage->height
							 )
				if(inpaintRenderImage
				   && (inpaintRenderImage->width != undoImage->width || inpaintRenderImage->height != undoImage->height)) {
					tmReleaseImage(&inpaintRenderImage);
				}

				if(!inpaintRenderImage) {
					TMIMG_printf(TMLOG_DEBUG, "create inpaintRenderImage(%dx%d)",
								undoImage->width, undoImage->height)
					// cvInpaintImage only supports IPL_DEPTH_8U
					inpaintRenderImage = tmCreateImage(
							cvSize(undoImage->width, undoImage->height),
							IPL_DEPTH_8U, //originalImage->depth,
							originalImage->nChannels);
				}

				// cvInpaintImage only supports IPL_DEPTH_8U
				IplImage * l_srcImage = undoImage;
				if(undoImage->depth != IPL_DEPTH_8U) {
					l_srcImage = tmCreateImage(cvSize(undoImage->width, undoImage->height),
											   IPL_DEPTH_8U, undoImage->nChannels);
					tmCropImage(undoImage, l_srcImage, 0, 0);
				}

				// extrapolate the dust marked in inpaintMaskImage with inpainting
				cvInpaint(l_srcImage, inpaintMaskImage,
					  inpaintRenderImage,
					  correction.copy_width+9,
						// CV_INPAINT_TELEA
					  CV_INPAINT_NS
					  );

				tmCropImage(inpaintRenderImage,
							disp_correctColorImage, 0, 0, false);

				m_inpainting_rendered = true;

				// Apply render image in original image
				// fixme: how to reinsert image ? since the user can draw the mask in several times, and at last we must not
				tmInsertImage(inpaintRenderImage, originalImage,
							  undoImage_x, undoImage_y);
				if(l_srcImage != undoImage) {
					tmReleaseImage(&l_srcImage);
				}
			}

//			tmReleaseImage(&dilatedGrownImage);
		}
	}

	// Main windows
	if(displayBlockImage && displayImage) {
		if(displayBlockImage->width == displayImage->width
			&& displayBlockImage->height == displayImage->height
			) {
			cvCopy(displayImage, displayBlockImage);
		} else {
			fprintf(stderr, "TamanoirImgProc::%s:%d : error : different image size : displayImage %dx%d != displayBlockImage %dx%d\n",
					__func__, __LINE__,
					displayImage->width, displayImage->height,
					displayBlockImage->width, displayBlockImage->height);

		}

		// Return when we can propose something for correction
		//	mark current on displayImage
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

	if(pcorrection->crop_x < 0) return;
	if(!diffImage) return;

	bool resnap = false;
	if(undoImage) {
		if(pcorrection->crop_x != undoImage_x
		   || pcorrection->crop_y != undoImage_y) {
			resnap = true;
			//fprintf(stderr, "[imgproc]::%s:%d : resnap undoImage\n", __func__, __LINE__);
		}
	}

	if(!undoImage && cropImage) {
		resnap = true;
	}

	if(resnap) {
		undoImage_x = pcorrection->crop_x;
		undoImage_y = pcorrection->crop_y;
		int crop_width = pcorrection->crop_width;
		int crop_height = pcorrection->crop_height;
		if(crop_width <= 0 || crop_height<=0) {
			crop_width = processingSize.width;
			crop_height = processingSize.height;
		}

/*
		if(displaySize.width >0 && displaySize.height>0) {
			 crop_width = displaySize.width;
			 crop_height = displaySize.height;
		 }
*/
		// recrop display images
		recropImages(cvSize(crop_width, crop_height),
					 undoImage_x, undoImage_y);

		TMIMG_printf(TMLOG_DEBUG, "resnap undoImage : top-left=%d,%d + %dx%d\n",
				undoImage_x, undoImage_y,
				crop_width, crop_height)
	}

	/*
	if(g_debug_imgverbose) {
		fprintf(stderr, "[imgproc] %s:%d : rel_x=%d rel_y=%d  %dx%d\n", __func__, __LINE__,
			rel_x, rel_y, cropImage->width, cropImage->height);
	}*/

	// Store correction in full image buffer
	pcorrection->rel_src_x = rel_x;
	pcorrection->rel_src_y = rel_y;

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

int TamanoirImgProc::applyInpainting(t_correction , bool )
{
	fprintf(stderr, "TamanoirImgProc::%s:%d : NOT IMPLEMENTED !!\n", __func__, __LINE__);
	return 0;
}

/* Apply a former correction */
int TamanoirImgProc::applyCorrection(t_correction correction, bool force)
{
	if(correction.copy_width <= 0) {
		TMIMG_printf(TMLOG_DEBUG, "copy_width<0 => do nothing")

		return -1; // no available correction
	}

	m_last_mode = TMMODE_CLONE;

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

	// Apply in display
	if(disp_cropColorImage) {
		tmCloneRegion(  disp_cropOrigImage,
						correction.rel_dest_x, correction.rel_dest_y,
						correction.rel_src_x, correction.rel_src_y,
						correction.copy_width, correction.copy_height);

	}

	// Delete same region in diff image to never find it again, even if we
	// use the rewind function
	/*
	tmFillRegion(  diffImage,
				   correction_dest_x, correction_dest_y,
				   correction.copy_width, correction.copy_height,
				   DIFF_CORRECTED);
	*/
	cvEllipse(diffImage,
				cvPoint(correction_dest_x, correction_dest_y),
				cvSize( (correction.copy_width+1)/2, (correction.copy_height+1)/2),
				0, 0, 360,
				cvScalarAll(DIFF_CORRECTED),
				-1);


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
	if(g_dataset_mode) { return false; }

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



/** @brief Return film type by its W/H ratio */
tm_film_size getFilmType(int w, int h, int * dpi) {
	tm_film_size retval;
	memset(&retval, 0, sizeof(tm_film_size));
	// search for closest H/W or W/H ratio
	int ifound = -1;
	bool best_is_wh = false;
	float ratio_in = (float)w/(float)h;
	float ratio_min = 0.5f;
	for(int i=0; g_film_types[i].width_mm>0; i++) {
		float ratio_wh = g_film_types[i].width_mm / g_film_types[i].height_mm;
		if(fabs(ratio_wh - ratio_in)< ratio_min) {
			ratio_min = fabs(ratio_wh - ratio_in);
			ifound = i;
			best_is_wh = true;
		}
		float ratio_hw =g_film_types[i].height_mm/g_film_types[i].width_mm;
		if(fabs(ratio_hw - ratio_in) < ratio_min) {
			ratio_min = fabs(ratio_hw - ratio_in);
			ifound = i;
			best_is_wh = false;
		}
	}
	if(ifound >= 0) {
		retval = g_film_types[ifound];
	}
	//
	TMIMG_printf(TMLOG_INFO, "img=%dx%d pix => ratio w/h=%g %s"
				 "=> best is film[%d]=%gx%g mm = '%s' ratio=%g",
					w,h,  (float)w/(float)h,
					best_is_wh ? "portrait":"landscape",
				 ifound,
				 retval.width_mm, retval.height_mm, retval.format,
					best_is_wh ? retval.width_mm/retval.height_mm: retval.height_mm/retval.width_mm
				 )
	if(dpi && ifound >= 0) {
		int olddpi = *dpi;
		int dpi_w = (int)roundf((float)w /( (best_is_wh ? retval.width_mm: retval.height_mm)/ 25.4 ));
		int dpi_h = (int)roundf((float)h / ((best_is_wh ? retval.height_mm: retval.width_mm) / 25.4 ));

		// round to 50 dpi
		*dpi = (int)roundf(tmmax(dpi_w, dpi_h)/50.f)*50;

		TMIMG_printf(TMLOG_INFO, "old=%d dpi => dpi guess = %d dpi x %d dpi => %d dpi",
					 olddpi,
					 dpi_w, dpi_h, *dpi)
	}


	return retval;
}


