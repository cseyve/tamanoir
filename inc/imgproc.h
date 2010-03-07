/** @file imgproc.h
  @brief Image processing class for dust removing

  This class contains the image processing functions for detecting dusts,
	and proposing a correction with a clone operation

	@author Christophe Seyve <cseyve@free.fr>
*/

/*************************************************************************
 *            imgproc.h
 *
 *  Tue Oct 23 21:26:10 2007
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





/** @mainpage

  @section intro_sec Introduction

  Tamanoir is an interactive dust removing tool. It's designed for the users of
  flatbed scanners which do not provide an Optical ICE. Indeed most flatbed
  scanners do not embed real optical dust detection with infra-red sensors (dust
  are opaque in IR). So the detection is software, slow and often generates
  weird artefacts.

  @section spirit_sec Spirit of Tamanoir

  Tamanoir's goal is to detect the dusts by image processing, then to propose a
  correction to the user, who can validate or refuse the proposal. The correction
  is made with a "clone" operation: the source if copied on the destination,
  as we do when we correct dusts with image manipulation softwares (Gimp,
  Photoshop...).

  Thus the spirit is to correct the picture interactively, not to have a global
  filter which will correct anything it *thinks* to be a dust without asking
  permission.

  @section usecase_sec Typical use case

  The typical use of Tamanoir is to process in 2 passes :

  1) use the semi-auto detection to detect dusts, let it propose a correction,
	skip or apply with shortcuts. You can go fast with the skip key (right arrow),
	and go back if you skipped unintentionaly a real dust.

	When this pass is done, 80 to 90 % of the dusts are removed. The remaining
	dusts are big fibers or small dusts in textured regions.

  2) Do a manual pass to check and complete the correction. Use the "Home" key
	to go to the top-left of image, correct with the Clone tool the picture block
	then go to the next block with the page down.


  @section otherlibs_sec External dependencies

  Tamanoir uses :
  - Qt4 for GUI portability under Linux/MacOSX/Windows
  - OpenCV for image processing basic functions
  - LibTIFF for 16bit importation (not supported in OpenCV)

  @ref imgproc.h

	@line

  @section Algorithm Dust detection algorithm

  Dusts are detected using a difference between the original image (originalImage) and its
	slightly blurred version (medianImage).
	The difference (diffImage) is computed depending on the type of film :
	since the dusts are opaque, they appear brighter than the background for negative films
	and darker as the background for positive films (slides...).
	Therefore in scans of negative, the diffImage is filled only if original-blurred > 0

	There are 2 threshold levels in diffImage :
	- dust candidates are filled with DIFF_THRESHVAL
	- contour of lower differences are filled with DIFF_CONTOUR

	Then the algorithm scans the diffImage for points with pixel value = DIFF_THRESHVAL
	When found, the point is used as a "seed" for constrained region growing.
	The region must be big enough but not too much.
	The min area is m_dust_area_min = DUST_MIN_SIZE * (m_options.dpi * m_options.dpi)/ (2400*2400) * coef[sensitivity];
		- the value of DUST_MIN_SIZE has been tested fron an Epson 4990 scanner @ 2400 dpi
		- the coefficient depending on sensitivity is >= 1 : 1 = highly sensitive ... 5 = low


	If the region is big enough (or if the search is forced by clone mode), we process many check steps:
	We work in cropped region to use smaller images (for memory saving and ebanling the undo operation)
	- The function dilateDust() dilate the grown region and checks:
		- Corner test : maybe the dust is only a corner in a texture, and the blur operation only showed it
				NOT FULLY IMPLEMENTED = NOT ACTIVE
		- Check if the dust is a fiber : NOT FULLY IMPLEMENTED : no action when it's a fiber
		- Contrast check : the dust must be visible, e.g. its value must be different from neighbour : |mean-neighbour|>10 && |contrast|>0.02
	- search a source candidate for a clone operation : tmSearchBestCorrelation
		If a candidate is found (by correlation), we check its ability to be a real dust :
			- the source candidate must not be linked to the dust by a DIFF_CONTOUR area in diffImage : srcNotConnectedToDest()
			- the dust must be different from neighbourhood (like a pattern in textured area) : differentFromNeighbourhood()
			- the source and the dust must be different (to prevent for copying periodical patterns such as windows on a building, ...) : srcDifferentFromDest()
			- after the cloning, the difference between the corrected image and the median must be lower than the original compared to the median
			- if the option "empty only" is active, check if the box bounding both dust and source is an empty area of image
  */

#ifndef IMGPROC_H

#define IMGPROC_H

#include <stdlib.h>

#include "imgutils.h"


#define STATS_MAX_SURF	1000

/** @brief Statistics on dust replacement and sizes */
typedef struct _dust_stats_t {
	/* Cumul variables */
	unsigned long nb_grown;				/*! grown regions */
	unsigned long grown_size[STATS_MAX_SURF];		/*! grown size histogram */
	unsigned long nb_grown_replaced;	/*! grown regions which have been replaced */
	unsigned long grown_size_replaced[STATS_MAX_SURF];		/*! grown size histogram */
	unsigned long nb_grown_validated;	/*! grown regions which have been validated */
	unsigned long grown_size_validated[STATS_MAX_SURF];		/*! grown size histogram */

	/* Statistics variables */
	float ratio_replaced_grown;
	float ratio_validated_grown;
	float ratio_validated_replaced;

} dust_stats_t;


/** @brief Local correction storage structure

This structure contains the information needed to clone a part of the original image into the corrected image
*/
typedef struct t_correction_ {
	/// Width of copy
	int copy_width;
	/// Height of copy
	int copy_height;

	/// top-left of Crop image
	int crop_x;
	/// top-left of Crop image
	int crop_y;

	/// relative origin of clone, x coordinate
	int rel_src_x;
	/// relative origin of clone, y coordinate
	int rel_src_y;

	/// relative destination of clone, x coordinate
	int rel_dest_x;
	/// relative destination of clone, y coordinate
	int rel_dest_y;

	/// relative seed of region growing, x coordinate
	int rel_seed_x;
	/// relative seed of region growing, y coordinate
	int rel_seed_y;

	/// Crop size for display, width
	int crop_width;
	/// Crop size for display, height
	int crop_height;

	// Dust size statistics
	/// Area of dust in image (pixel^2)
	int area;
	/// Dust grown by image processing
	CvConnectedComp grown_dust;

	/// physical/real width of dust in millimeters
	float width_mm;
	/// physical/real height of dust in millimeters
	float height_mm;

	/// Accuracy
	float bg_diff;
	float proposal_diff;
	float equivalent_diff;
	float srcdest_correl; /*! Correlation between source and destination, _including_ dust */
	float srcdest_maxdiff; /*! Max difference between source and destination, _including_ dust */
	int smooth_diff;	/*! Max difference in neighbourhood of (copy+src) bounding box */
} t_correction;


/** @brief Properties of dust extracted by image processing

  Aimed for debug / optimization of criterions
  */
typedef struct {
	int seed_x, seed_y;		/*! Original seed */
	CvConnectedComp abs_grown_conn;	/*! Absolute grown region */
	CvConnectedComp rel_grown_conn;	/*! Relative grown region */
	u8 force_search;				/*! Forced search flag */

	u8 big_enough;					/*! Size if big enough to be a dust (dpeend on dpi) */

	float connect_area;				/*! Area of region growing on diff level */
	float flood_area;				/*! Area of region growing on gray level */
	u8 is_fiber;					/*! Fiber (surf of rect >> surf of diff region growing */
	float mean_neighbour;			/*! Mean value of neighbouring */
	float mean_dust;				/*! Mean value of dust */
	float contrast;					/*! Contrast between neighbouring and dust */
	u8 visible_enough;				/*! Different enough from neighbour */
	u8 dilateDust ;					/*! => Still a dust after function dilateDust() */
	float best_correl_dilate;		/*! Best correlation threshold determined by dilateDust */
	float best_correl_max ;			/*! Best correlation threshold after force_search override */

	int searchBestCorrelation;		/*! Result of search best correlation */

	int copy_dx, copy_dy;			/*! Copy proposal vector=(dest-src) */
	float correl_dust_src;			/*! Correlation between dust (seed growing) and source proposal */
	u8 src_not_connected_to_dest;	/*! is source connected to dest ? e.g. same region, fiber, cable ... return of @see srcNotConnectedToDest(pcorrection); */

	u8 diff_from_neighbour;			/*! return of @see differentFromNeighbourHood(pcorrection, diffref); */
	u8 diff_from_dest;				/*! return of @see srcDifferentFromDest(pcorrection); */
	u8 correctedBetterThanOriginal; /*! return of @see correctedBetterThanOriginal(pcorrection); */
	u8 neighbourhoodEmpty;			/*! return of @see neighbourhoodEmpty(pcorrection); */
} t_dust_detection_props;

/** @brief Already known dust for evaluation mode */
typedef struct _known_dust {
	double dest_x;		/*! Dust position in relative coordinate [0..1[ */
	double dest_y;		/*! Dust position in relative coordinate [0..1[ */
	double dest_width;	/*! Dust size in relative coordinate [0..1[ */
	double dest_height;	/*! Dust size in relative coordinate [0..1[ */


	double seed_x;	/*! Dust seed point in relative coordinate [0..1[ */
	double seed_y;	/*! Dust seed point in relative coordinate [0..1[ */

} t_known_dust;


/** @brief Pre-processing status
  used to do faster option changes
  */
typedef struct {
	u8 grayscaled_blurred;	/*! grayscaled image has been blurred during preprocessing,
								so it's reconverted from color in preProcessing() */
	u8 median_done;		/*!	Median processing ( processMedian() )is fully done */
	u8 diff_done;		/*!	Difference is done */
	u8 preproc_done;	/*!	Pre-processing is fully done */

} tm_preproc_status;



/** @brief Detection performance stats */
typedef struct _perf_stats {
	int true_positive;
	int no_proposal;

	int false_positive;
	int false_negative;
} t_perf_stats;


/** @brief Process then print statistics */
void processAndPrintStats(dust_stats_t * dust_stats, FILE * f = NULL);

/** @brief Test if dust is already knwon */
bool testKnownDust(t_correction, int img_w, int img_h);


/** @brief Film size format */
typedef struct {
	float width_mm;
	float height_mm;
	char format[32];
} tm_film_size;

const tm_film_size g_film_types[] = {
	{6, 8, "4/3 digital or 6x8 film"},
	{24, 36, "135/24x36 or 120/6x9"},
	{24, 55, "135/24x55 (pano)"},
	{56, 56, "120/6x6"},
	{56, 68, "120/6x7"},
	{56, 166, "120/6x17"},
	{4*2.54f, 5*2.45, "4x5 or 8x10"},
	{-1.f, -1.f, ""}
};

/** @brief Return film type by its W/H ratio */
tm_film_size getFilmType(int w, int h, int * dpi);

/** @brief Tamanoir image processing settings/options */
typedef struct {
	bool trust;				/*! Trust mode activated */
	bool hotPixels;			/*! Hot pixels detection activated */
	bool onlyEmpty;			/*! Return for dusts only in empty regions */
	int filmType;			/*! Film type */
	int dpi;				/*! Scan resolution in dot per inch */
	int sensitivity;		/*! Sensitivity (0 means highly sensitive to small dust, bigger needs bigger dusts) */
	bool mode_auto;			/*! Automatic mode : not saved in preferences */
} tm_options;

/** @brief Print processing options to a text file or std output/error */
void fprintfOptions(FILE * f, tm_options * p_options);

#define TMIMG_ERR_LOCKED	-2	/*! Loading or pre-procesing escaped because locked */
#define TMIMG_ERR_ABORT		-9	/*! Loading or pre-procesing aborted on request */


/** @brief main image processing class

	This class is used to load image, detect dust then search for correction proposal

	@author Christophe Seyve <cseyve@free.fr>
*/
class TamanoirImgProc {
public:
	/** @brief Constructor */
	TamanoirImgProc( int bw = 200, int bh = 200);
	/** @brief Destructor */
	~TamanoirImgProc();

	/** @brief allow cropping of debug images */
	void allowDebugCrop(bool on) { m_show_crop_debug = on; };


	/** @brief Set all options in one single call */
	int setOptions(tm_options opt);

	/** @brief Get all options in one single call */
	tm_options getOptions() { return m_options; };

	/** @brief Set film type \param type : 0 for undefined, 1 for negative, 2 for positive */
	void setFilmType(int type);

	/** @brief Load input image file */
	int loadFile(const char * filename);
	/** @brief Abort preprocessing or loading */
	int abortLoading(bool abort = true);

	/** @brief Save corrected image file */
	int saveFile(const char * filename);

	/** @brief Set if @see cropCorrectionImages must crop debug image too */
	void showDebug(bool on) { m_show_crop_debug = on; };

	/** @brief Perform image pre-processing (difference, conversion to grayscale, ...) */
	int preProcessImage();


	/** @brief Go to first dust
		@return -1 if error, 0 if no more dust, and 1 if still */
	int firstDust();

	/** @brief Go to next dust, @return -1 if error, 0 if no more dust, and 1 if still */
	int nextDust();

	/** @brief Get dust detection properties */
	t_dust_detection_props getDustProps() { return m_dust_detection_props; };

	/** @brief Change the origin of current correction */
	void setCopySrc(int x, int y);
	/** @brief Change the origin of a former correction */
	void setCopySrc(t_correction * pcorrection, int rel_x, int rel_y);

	/** @brief Apply proposed correction */
	int applyCorrection();

	/** @brief Apply proposed correction  and specify if correction has been forced by user */
	int forceCorrection(t_correction correction, bool force);

	/** @brief Apply a former correction and specify if correction has been forced by user */
	int applyCorrection(t_correction correction, bool force = false);

	/** @brief Apply a former correction by inpainting and specify if correction has been forced by user */
	int applyInpainting(t_correction correction, bool force = false);

	/** @brief Mark a former correction proposal as refused by user*/
	int skipCorrection(t_correction correction);


	/** @brief Get progress in %age */
	int getProgress();
	/** @brief Pre-processed done flag */
	bool getPreProcessedDone() { return m_preproc_status.preproc_done; };

	/** @brief Get performance statistics */
	t_perf_stats getPerfs() { return m_perf_stats; };

	/** @brief original version of input image */
	IplImage * getOriginal() { return originalImage; };

	/** @brief 8bit grayscale version of input image */
	IplImage * getGrayscale() { return grayImage; };

	/** 8@brief bit grayscale version of input image scaled for display */
	IplImage * getGrayscaleForDisplay() { return grayImage; };

	/** @brief Approximate a correction using params from last crop

		Check if last crop is still ok for new crop
		*/
	t_correction approxCorrection(t_correction correct_in);

	/** @brief Set available size for display : it will scale the 8bit grayscale version
	 * of input image for this rectangle */
	void setDisplaySize(int w, int h);
	/** @brief return small image for global view display */
	IplImage * getDisplayImage() { return displayBlockImage; };

	/** @brief return the resident version of small image for global view display */
	IplImage * getStillDisplayImage() { return displayImage; };
	/** @brief return the resident version of small image for global view display */
	IplImage * getDiffDisplayImage() { return displayDiffImage; };

	// Cropped images
	IplImage * getCorrectedCrop() { return disp_correctColorImage; };
	/** @brief Get cropped original image */
	IplImage * getCropOrig() { return disp_cropOrigImage; };
	/** @brief Get cropped original image with correction display */
	IplImage * getCropArrow() { return disp_cropColorImage; };
	IplImage * getDiffCrop() { return disp_cropImage; };
	/** @brief Get cropped mask of dust grown area */
	IplImage * getMask() { return disp_dilateImage; };
	/** @brief Get cropped corrected image */
	IplImage * getCorrectImage() { return correctImage; };

	/** @brief Get cropped mask of dust automatically erased */
	IplImage * getDustMask();

	CvConnectedComp getDustComp() { return m_lastDustComp; };
	dust_stats_t getDustStats() { return m_dust_stats; };

	/** @brief set the scan resolution to use the statistics on dust sizes */
	int processResolution(int dpi);
	/** @brief Set analysis block size, for display which do not use vertical scanlines */
	int setBlockSize(int w, int h);

	/** @brief Return the display (crop) block size, used for display and processing */
	CvSize getDisplayCropSize();
	/** @brief Undo last cloning */
	void undo();

	/** @brief return correction proposal */
	t_correction getCorrection() { return m_correct; };
#define TMMODE_NOFORCE	0
#define TMMODE_CLONE	1
#define TMMODE_INPAINT	2
	/** @brief Crop images for detailled view arround a previous correction */
	void cropCorrectionImages(t_correction correction, int correct_mode = TMMODE_CLONE);

	/** @brief get the cropped original image around the proposed correction */
	void cropOriginalImage(t_correction correction);
	/** @brief get the cropped corrected image around the proposed correction */
	void cropCorrectImage(t_correction correction);

	/** @brief Draw a circle in crop dilate mask for later inpainting */
	void drawInpaintCircle(t_correction correction);

	/** @brief Lock inpainting search : true when the user is drawing, and goes ot false when the mouse is released */
	void lockInpaintDrawing(bool lock);

	/** @brief Find a dust from (x, y) seed
		  @param force search is forced by user, and its mode :
			TMMODE_NOFORCE : no force,
			TMMOD_CLONE : cloning, so do not apply the trust mode !
			TMMOD_INPAINT : use inpainting, so just search for dilated area
		@return 1 if must return now
	*/
	int findDust(int x, int y, t_correction * pcorrection, int force = 0);


private:
	/** @brief Initialize buffers */
	void init();

	/** @brief Lock flag for long tasks (such as load and save) */
	bool m_lock;

	/// Abort request during preprocessing or loading
	bool m_abortLoading;

	/// Pre-processing progression and status
	tm_preproc_status m_preproc_status;

	/** Mutex to prevent clone search to perturbate normal background search */
	Mutex_t mutex;

	/** @brief Dust detection properties */
	t_dust_detection_props m_dust_detection_props;

	/** @brief Purge allocated buffers */
	void purge();
	/** @brief Purge display buffers */
	void purgeDisplay();
	/** @brief Purge cropped processing buffers */
	void purgeCropped();
	/** @brief Purge downscaled processing buffers */
	void purgeDownscaled();
	/** @brief Allocate cropped processing buffers */
	void allocCropped();

	/** @brief Options : Film type, resolution, ... */
	tm_options m_options;

	/** opened file name */
	char m_filename[512];

	/** Dust statistics */
	dust_stats_t m_dust_stats;

	/** Last correct area size */
	int m_correct_area;

	/** @brief Find an uniform place in image (x, y) to perform grain qualification
		@return -1 if error, 0 if found
	*/
	int findUniform(float * p_mean, float  * p_diff_mean, float * p_variance);

	/** @brief Performance statistics */
	t_perf_stats m_perf_stats;

	/** Perform image processing */
	int processImage();

	/** @brief Progress value */
	int m_progress;

	/** @brief Original image size */
	CvSize originalSize;

	/** @brief currently scanned block */
	CvRect scannedBlock;

	/** @brief block size */
	CvSize blockSize;

	/** @brief block size */
	CvSize displayCropSize;

	// Image buffers ==============================================================

	// FULL SIZE BUFFERS : same size as input ============

	/// Original image
	IplImage * originalImage;

	/// Original image, blurred for grain removal
	IplImage * origBlurredImage;

	/// Grayscale version of input image
	IplImage * grayImage;

	// DISPLAY SIZE BUFFERS : size for display in global view ============
	/// Downscaled version of originalImage for display in GUI
	IplImage * originalSmallImage;

	// DOWNSCALED BUFFERS : used for faster processing
	/// Scale factor of the downscaled buffers
	int m_downscale_factor;
	/// Downscaled size
	CvSize	m_dwscSize;

	/** @brief Smooth reference image (without the dusts appearing)
		This image contains the smoothed version of image, as a reduced version of either eroded or dilated images :
		- when the film is negative, dusts appear on dilated image, so the smooth reference is the eroded, scaled with mean computation on all the pixels of the block
		- when the film is positive, dusts appear on eroded images, so the smooth reference is the dilated, scaled with mean computation on all the pixels of the block
	  */
	IplImage * m_dwsc_smoothImage;
	/** @brief Dust highlighted image (with the dusts appearing)
		This image contains the exagerated version of dust appearance in image, as a reduced version of either eroded or dilated images :
		- when the film is negative, dusts appear on dilated image, so the smooth reference is the eroded, scaled with mean computation on all the pixels of the block
		- when the film is positive, dusts appear on eroded images, so the smooth reference is the dilated, scaled with mean computation on all the pixels of the block
	  */
	IplImage * m_dwsc_dustImage;

	/** @brief Diff image for dust detection */
	IplImage * m_dwsc_diffImage;

	/** @brief Temporary grow image for dust detection */
	IplImage * m_dwsc_growImage;

	/** @brief Process downscaled image detection of dusts */
	void processDownscaledAnalysis();


	// CROPPED BUFFERS : size of cropped displays ============

	/// Undo image
	IplImage * undoImage;
	/// Undo difference image
	IplImage * undoDiffImage;

	/// Undo image crop position (top-left)
	int undoImage_x, undoImage_y;

	/// Inpainting rendered image
	IplImage * inpaintRenderImage;
	/// Inpainting mask image
	IplImage * inpaintMaskImage;

	/// Drawing mode
	int m_last_mode;

	/// Purge and crop images for display since size changed
	void recropImages(CvSize cropSize, int crop_x, int crop_y);

	/// Lock inpainting search : true when the user is drawing, and goes ot false when the mouse is released
	bool m_inpainting_lock;
	/// Rendered flag for inpainting
	bool m_inpainting_rendered;


	/// Process median filter
	void processMedian();


	IplImage * medianImage;
	IplImage * erodedImage;
	IplImage * dilatedImage;

	/// Image of difference, thresholded with different values
	IplImage * diffImage;

	/// Downscaled version of diffImage, used to show dusts when changing the sensitivity
	IplImage * displayDiffImage;

	/// Mask of corrected dusts
	IplImage * dustMaskImage;

	/// Variance of difference
	IplImage * varianceImage;

	/// Region growing output image
	IplImage * growImage;

	/// image buffer for display in cropped displays
	IplImage * displayBlockImage;
	/// image buffer for display (without the crop mark)
	IplImage * displayImage;
	CvSize displaySize;


	/** Processed imge size */
	CvSize processingSize;
	IplImage * dilateImage;
	IplImage * cropImage;
	IplImage * tmpCropImage;
	IplImage * correctImage;
	IplImage * sobelImage;

	IplImage * cropColorImage;
	IplImage * correctColorImage;

	// Same images for display
	IplImage * disp_cropOrigImage; /*! Original image cropped for display */
	IplImage * disp_cropColorImage;	/*! Overlay of original with src->dest clone arrow */
	IplImage * disp_correctColorImage; /*! Cropped image after correction */
	IplImage * disp_dilateImage;	/*! Dilated image with dust as grown region */
	IplImage * disp_cropImage;
	bool m_show_crop_debug;

	/** @brief Find a dust from (x, y) seed
		@return 1 if must return now
	*/
	int findDust(int x, int y);

	/** @brief Crop images to create cropped views for GUI */
	void cropViewImages();
	/** @brief Dilate Dust to have a better segmentation
		@param[in] crop_x top-left position of crop in original image
		@param[in] crop_y top-left position of crop in original image
		@param[in] crop_center_x seed position of crop in original image
		@param[in] crop_center_y seed position of crop in original image
		@param[in,out] pcrop_connect grown region
		@param[in] is_a_dust tell function if this region is considered as a dust
		@param[in] force_search true if search is forced by user (clone tool)
		@param[out] dilateImageOut dilate image destination, used either in findDust=>dilateImage or in cropCorrectionImages for display
		@param[out] pbest_correl search distance tolerance
		@param[out] pmean_dust mean value of dust
		@param[out] pmean_neighbour mean value of dust's neighbour
		@return true if region is a dust, false if not
	*/
	bool dilateDust(int crop_x, int crop_y,
					int crop_center_x, int crop_center_y,
					CvConnectedComp * pcrop_connect,
					bool is_a_dust, bool force_search,
					IplImage * dilateImageOut,
					int * pbest_correl = NULL,
					float * pmean_dust = NULL, float * pmean_neighbour = NULL);

	// ============= DUST LIKELYHOOD FUNCTIONS =============
	/*
	Dust are characterized by :
	- we detect them by the difference between the grayscaled image
		and the blurred grayscale image (@see dilateDust)
	- we look for a replacement candidate with (@see tmSearchBestCorrelation)
	- if we found a replacement, but we have to filter real high frequency pattern from
		real dusts:
		o source and destination must not be connected (@see srcNotConnectedToDest)
			=> filter : branches, edges, ...
		o dilated dust level must have a mean gray level different enough from the gray level of the neighbourhood
			(@see differentFromNeighbourHood)
			=> filter small grains
		o after the correction, the corrected image must be less different from the
			blurred image than the detected dust (@see correctedBetterThanOriginal)

	*/
	/** @brief Test if source and destination are connected (by an edge, a branch...)
		@return false if they are connected
	*/
	bool srcNotConnectedToDest(t_correction * pcorrection);

	/** @brief Test if the dust gray level is different enough from the neighbourhood
	*/
	bool differentFromNeighbourHood(
		t_correction * pcorrection,
		u8 diffref);

	/** @brief check if correction is better than original
		 e.g. that there's less difference after correction
		 between the corrected image and the median/blurred image */
	bool correctedBetterThanOriginal(t_correction * pcorrection);

	/* @brief Test if source and destination are different */
	bool srcDifferentFromDest(t_correction * pcorrection);

	// ============= DUST LIKELYHOOD FUNCTIONS (end) =============



	/** @brief is neighbour of dust empty (isolated dust in sky) */
	bool neighbourhoodEmpty(t_correction * pcorrection);





	/** @brief Last seed position (used for searching next) */
	int m_seed_x;
	/** @brief Last seed position (used for searching next) */
	int m_seed_y;

	/** @brief Last grown connected comp to prevent from doing the same operations many times in findDust */
	CvConnectedComp m_findDust_last_connect;
	/** @brief Last seed to prevent from doing the same operations many times in findDust */
	int m_findDust_last_seed_x;
	/** @brief Last seed to prevent from doing the same operations many times in findDust */
	int m_findDust_last_seed_y;


	/** @brief Last seed position (used for searching next) */
	int m_block_seed_x;
	/** @brief Last seed position (used for searching next) */
	int m_block_seed_y;
	/** @brief Last seed position (used for searching next) */
	int m_block_seed_width;
	/** @brief Last seed position (used for searching next) */
	int m_block_seed_height;



	u8 m_threshold;
	int m_smooth_size; // For smoothed processed images

	int m_dust_area_min;
	int m_dust_area_max;

	/** Last detected dust */
	CvConnectedComp m_lastDustComp;

	/** Correction data */
	t_correction m_correct;

	/** Last applied ou refused correction */
	t_correction m_last_correction;

};



#endif // IMGPROC_H
