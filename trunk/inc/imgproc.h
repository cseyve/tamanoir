/***************************************************************************
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
 

#ifndef IMGPROC_H

#define IMGPROC_H


#include "imgutils.h"


#define COLORMARK_FAILED	252
#define COLORMARK_REFUSED	253
#define COLORMARK_CORRECTED	254
#define COLORMARK_CURRENT	255


#define STATS_MAX_SURF	1000


/** @brief Statistics on dust replacement and sizes*/
typedef struct dust_stats_t_ {
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

/** @brief Process then print statistics */
void processAndPrintStats(dust_stats_t * dust_stats, FILE * f = NULL);


/** @brief Local correction storage structure */
typedef struct t_correction_ {
	/// Destination of copy
	int dest_x;
	/// Destination of copy
	int dest_y;
	/// Source of copy
	int src_x;
	/// Source of copy
	int src_y;
	int copy_width;
	int copy_height;
	
	/// top-left of Crop image
	int crop_x;
	/// top-left of Crop image
	int crop_y;
	
	/// relative origin of clone
	int rel_src_x;
	/// relative origin of clone
	int rel_src_y;
	
	/// relative destination of clone
	int rel_dest_x;
	int rel_dest_y;
	
	/// relative seed of region growing
	int rel_seed_x;
	int rel_seed_y;
	
	
	// Dust size statistics
	int area;
	float width_mm;
	float height_mm;
	
} t_correction;


/** @brief Already known dust for evaluation mode */
typedef struct _known_dust {
	double dest_x;		/*! Dust position in relative coordinate [0..1[ */
	double dest_y;		/*! Dust position in relative coordinate [0..1[ */
	double dest_width;	/*! Dust size in relative coordinate [0..1[ */
	double dest_height;	/*! Dust size in relative coordinate [0..1[ */

	
	double seed_x;	/*! Dust seed point in relative coordinate [0..1[ */
	double seed_y;	/*! Dust seed point in relative coordinate [0..1[ */
	
} t_known_dust;

/** @brief Test if dust is already knwon */
bool testKnownDust(t_correction, int img_w, int img_h); 


/** \brief main image processing class


*/
class TamanoirImgProc {
public:
	TamanoirImgProc();
	~TamanoirImgProc();


	/** @brief Set film type \param type : 0 for undefined, 1 for negative, 2 for positive */
	void setFilmType(int type);
	
	/** @brief Load input image file */
	int loadFile(const char * filename);
	
	/** @brief Save corrected image file */
	int saveFile(const char * filename);
	
	
	/** @brief Perform image pre-processing */
	int preProcessImage();

	/** @brief Go to first dust, @return -1 if error, 0 if no more dust, and 1 if still */
	int firstDust();
	
	/** @brief Go to next dust, @return -1 if error, 0 if no more dust, and 1 if still */
	int nextDust();


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

	/** @brief Mark a former correction proposal as refused by user*/
	int skipCorrection(t_correction correction);

	/** @brief Get progress in %age */
	int getProgress();
	
	/** @brief original version of input image */
	IplImage * getOriginal() { return originalImage; };
	
	/** @brief 8bit grayscale version of input image */
	IplImage * getGrayscale() { return grayImage; };
	
	/** 8@brief bit grayscale version of input image scaled for display */
	IplImage * getGrayscaleForDisplay() { return grayImage; };
	
	/** @brief Set available size for display : it will scale the 8bit grayscale version
	 * of input image for this rectangle */
	void setDisplaySize(int w, int h);
	IplImage * getDisplayImage() { return displayImage; };
	
	// Cropped images
	IplImage * getCorrectedCrop() { return disp_correctColorImage; };
	IplImage * getCrop() { return disp_cropColorImage; };
	IplImage * getDiffCrop() { return disp_cropImage; };
	IplImage * getMask() { return disp_dilateImage; };
	
	CvConnectedComp getDustComp() { return m_lastDustComp; };
	dust_stats_t getDustStats() { return m_dust_stats; };
	
	/** @brief Activate/desactivate hot pixels filtering */
	bool setHotPixelsFilter(bool on);
	/** @brief Activate/desactivate trust on good corrections */
	bool setTrustCorrection(bool on);
	/** S@brief et the scan resolution to use the statistics on dust sizes */
	int setResolution(int dpi);
	
	/** @brief return correction proposal */
	t_correction getCorrection() { return m_correct; };
	
	/** @brief Crop images for detailled view arround a previous correction */
	void cropCorrectionImages(t_correction correction);
	
private:
	/** Initialize buffers */
	void init();
	
	/** Lock flag for long tasks (such as load and save) */
	bool m_lock;
	
	/** Purge buffers */
	void purge();
	
	/** Film type */
	int m_FilmType;
	
	/** Scan resolution */
	int m_dpi;
	
	/** opened file name */
	char m_filename[512];

	/** Hot pixels filtering */
	bool m_hotPixels;
	
	/** Trust good correction proposals */
	bool m_trust;
	
	/** Dust statistics */
	dust_stats_t m_dust_stats;
	/** Last correct area size */
	int m_correct_area;
	
	/** @brief Find an uniform place in image (x, y) to perform grain qualification
		@return -1 if error, 0 if found
	*/
	int findUniform(float * p_mean, float  * p_diff_mean, float * p_variance);
	
	
	/** Perform image processing */
	int processImage();
	
	/** @brief Progress value */
	int m_progress;

	/** Original image size */
	CvSize originalSize;


	// Image buffers
	IplImage * originalImage;
	
	IplImage * origBlurredImage;
	
	IplImage * grayImage;
	IplImage * medianImage;
	IplImage * diffImage;
	IplImage * growImage;
	
	/// image buffer for display
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
	IplImage * disp_cropColorImage;
	IplImage * disp_correctColorImage;
	IplImage * disp_dilateImage; 
	IplImage * disp_cropImage;
	
	
	/** @brief Crop images to create cropped views for GUI */
	void cropViewImages();
	
	int m_seed_x;
	int m_seed_y;
	
	/** @brief Find a dust from (x, y) seed
		@return 1 if must return now
	*/
	int findDust(int x, int y);
	
	
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
