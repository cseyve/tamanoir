/* $Id: raw2tiff.c,v 1.23 2006/03/23 14:54:02 dron Exp $
 *
 * Project:  libtiff tools
 * Purpose:  Convert raw byte sequences in TIFF images
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_IO_H
# include <io.h>
#endif



// OpenCV
#include "imgutils.h"

extern "C" {
#include "tif_config.h"

#include <tiffio.h>
}


#include "imgutils.h"


u8 g_debug_cv2tiff = TMLOG_DEBUG;


#ifndef HAVE_GETOPT
extern int getopt(int, char**, char*);
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

typedef enum {
	PIXEL,
	BAND
} InterleavingType;

static	uint16 compression = (uint16) -1;
static	int jpegcolormode = JPEGCOLORMODE_RGB;
static	int quality = 75;		/* JPEG quality */
static	uint16 predictor = 0;

static void swapBytesInScanline(void *, uint32, TIFFDataType);
static int guessSize(char *, long, TIFFDataType, off_t, uint32, int,
		     uint32 *, uint32 *);
static double correlation(void *, void *, uint32, TIFFDataType);
static	int processCompressOptions(char*);

//int main(int argc, char* argv[])
int saveIplImageAsTIFF(IplImage* img, const char * outfilename, char * compressionarg)
{
	uint32	width = 0, length = 0, linebytes, bufsize;
	uint32	nbands = 1;		    /* number of bands in input image*/
	off_t	hdr_size = 0;		    /* size of the header to skip */
	TIFFDataType dtype = TIFF_BYTE;
	int16	depth = 1;		    /* bytes per pixel in input image */
	int	swab = 0;		    /* byte swapping flag */
	InterleavingType interleaving = PIXEL;  /* interleaving type flag */
	uint32  rowsperstrip = (uint32) -1;
	uint16	photometric = PHOTOMETRIC_MINISBLACK;
	uint16	config = PLANARCONFIG_CONTIG;
	uint16	fillorder = FILLORDER_LSB2MSB; // => With this setting, problem when opening files in Adobe Lightroom
							// Though other software don't mind (Xee, Apercu, GQView...)
	
	if(img->depth == IPL_DEPTH_16U)
		fillorder = FILLORDER_MSB2LSB; // also bad : RGB -> BGR with lightroom

	TIFF	*out;

	uint32 row, col, band;
	unsigned char *buf = NULL, *buf1 = NULL;
	
	
	unsigned char *buffer = (unsigned char *)img->imageData;
	
	//while ((c = getopt(argc, argv, "c:r:H:w:l:b:d:LMp:si:o:h")) != -1) {
	//	switch (c) {
	//	case 'c':		/* compression scheme */
	if (!processCompressOptions(compressionarg)) {
	//	usage();
	}

	//case 'r':		/* rows/strip */
	rowsperstrip = img->height; //atoi(optarg);
	//	case 'H':		/* size of input image file header */
	//		hdr_size = atoi(optarg);
	hdr_size = 0;
	//	case 'w':		/* input image width */
			width = img->width; //atoi(optarg);
	//	case 'l':		/* input image length */
			length = img->height; //atoi(optarg);
	//	case 'b':		/* number of bands in input image */
			nbands = img->nChannels; //atoi(optarg);
	//	case 'd':		/* type of samples in input image */
	switch(img->depth) {
	case IPL_DEPTH_8U:		//if (strncmp(optarg, "byte", 4) == 0)
		dtype = TIFF_BYTE;
		break;
	case IPL_DEPTH_16U:	//		else if (strncmp(optarg, "short", 5) == 0)
		dtype = TIFF_SHORT;
		break;
	//case IPL_DEPTH_32U:	//		else if  (strncmp(optarg, "long", 4) == 0)
	//	dtype = TIFF_LONG;
	//	break;
	case IPL_DEPTH_8S:	//		else if  (strncmp(optarg, "sbyte", 5) == 0)
		dtype = TIFF_SBYTE;
		break;
	case IPL_DEPTH_16S:	//		else if  (strncmp(optarg, "sshort", 6) == 0)
		dtype = TIFF_SSHORT;
		break;
	case IPL_DEPTH_32S:	//		else if  (strncmp(optarg, "slong", 5) == 0)
		dtype = TIFF_SLONG;
		break;
	case IPL_DEPTH_32F:	//		else if  (strncmp(optarg, "float", 5) == 0)
		dtype = TIFF_FLOAT;
		break;
	case IPL_DEPTH_64F:	//		else if  (strncmp(optarg, "double", 6) == 0)
		dtype = TIFF_DOUBLE;
		break;
	default:	//		else
		dtype = TIFF_BYTE;
		break;
	}
	depth = TIFFDataWidth(dtype);
	
	
	fprintf(stderr, "%s %s:%d => TIFF depth=%d\n", __FILE__, __func__, __LINE__, depth);
	
	
	//	case 'L':		/* input has lsb-to-msb fillorder */
	//		fillorder = FILLORDER_LSB2MSB;
	//	case 'M':		/* input has msb-to-lsb fillorder */
	//		fillorder = FILLORDER_MSB2LSB;
	//		break;
	//	case 'p':		/* photometric interpretation */
	//		if (strncmp(optarg, "miniswhite", 10) == 0)
	//			photometric = PHOTOMETRIC_MINISWHITE;
	//		else if (strncmp(optarg, "rgb", 3) == 0)
	if(img->nChannels > 1)
				photometric = PHOTOMETRIC_RGB;
	else //if (strncmp(optarg, "minisblack", 10) == 0)
				photometric = PHOTOMETRIC_MINISBLACK;
	//		else if (strncmp(optarg, "cmyk", 4) == 0)
	//			photometric = PHOTOMETRIC_SEPARATED;
	//		else if (strncmp(optarg, "ycbcr", 5) == 0)
	//			photometric = PHOTOMETRIC_YCBCR;
	//		else if (strncmp(optarg, "cielab", 6) == 0)
	//			photometric = PHOTOMETRIC_CIELAB;
	//		else if (strncmp(optarg, "icclab", 6) == 0)
	//			photometric = PHOTOMETRIC_ICCLAB;
	//		else if (strncmp(optarg, "itulab", 6) == 0)
	//			photometric = PHOTOMETRIC_ITULAB;
	//		else
	//			photometric = PHOTOMETRIC_MINISBLACK;
	//		break;
	//	case 's':		/* do we need to swap bytes? */
	swab = 0;//		swab = 1;
	//		break;
	//	case 'i':		/* type of interleaving */
	//		if (strncmp(optarg, "pixel", 4) == 0)
	//			interleaving = PIXEL;
	//		else if  (strncmp(optarg, "band", 6) == 0)
	//			interleaving = BAND;
	//		else
				interleaving = PIXEL; //0;
	//		break;
	//	case 'o':
	//		outfilename = optarg;
	//		break;
	//	case 'h':
	//		usage();
	//	default:
	//		break;
	//	}
        
	
       hdr_size = 0;
       
	if (guessSize(img->imageData, img->widthStep * img->height,
		dtype, hdr_size, nbands, swab, &width, &length) < 0) {
		return 1;
	}
	fprintf(stderr, "[%s] %s:%d :dtype=%d, hdr_size=%d, nbands=%d, swab=%d, width=%d, length=%d\n",
		__FILE__, __func__, __LINE__,
		dtype, (int)hdr_size, nbands, swab, width, length);
	
	out = TIFFOpen(outfilename, "w");
	if (out == NULL) {
		fprintf(stderr, "%s: %s:%d : Cannot open file '%s' for output.\n",
			__FILE__, __func__, __LINE__, outfilename);
		return (-1);
	}
	
	TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, length);
	TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, nbands);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, depth * 8);
	TIFFSetField(out, TIFFTAG_FILLORDER, fillorder);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, config);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, photometric);
	switch (dtype) {
	case TIFF_BYTE:
	case TIFF_SHORT:
	case TIFF_LONG:
		TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
		break;
	case TIFF_SBYTE:
	case TIFF_SSHORT:
	case TIFF_SLONG:
		TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_INT);
		break;
	case TIFF_FLOAT:
	case TIFF_DOUBLE:
		TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
		break;
	default:
		TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_VOID);
		break;
	}
	if (compression == (uint16) -1)
		compression = COMPRESSION_PACKBITS;
	TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
	switch (compression) {
	case COMPRESSION_JPEG:
		if (photometric == PHOTOMETRIC_RGB
		    && jpegcolormode == JPEGCOLORMODE_RGB)
			photometric = PHOTOMETRIC_YCBCR;
		TIFFSetField(out, TIFFTAG_JPEGQUALITY, quality);
		TIFFSetField(out, TIFFTAG_JPEGCOLORMODE, jpegcolormode);
		break;
	case COMPRESSION_LZW:
	case COMPRESSION_DEFLATE:
		if (predictor != 0)
			TIFFSetField(out, TIFFTAG_PREDICTOR, predictor);
		break;
	}
	switch(interleaving) {
	case BAND:				/* band interleaved data */
		linebytes = width * depth;
		buf = (unsigned char *)_TIFFmalloc(linebytes);
		break;
	case PIXEL:				/* pixel interleaved data */
	default:
		linebytes = width * nbands * depth;
		break;
	}
	bufsize = width * nbands * depth;
	buf1 = (unsigned char *)_TIFFmalloc(bufsize);

	rowsperstrip = TIFFDefaultStripSize(out, rowsperstrip);
	if (rowsperstrip > length) {
		rowsperstrip = length;
	}
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip );
	
	
	fprintf(stderr, "[%s] %s:%d :rowsperstrip=%d, bufsize=%d, linebytes=%d, nbands=%d, bufsize=%d, width=%d, length=%d\n",
			__FILE__, __func__, __LINE__,
			rowsperstrip, bufsize, linebytes, nbands, bufsize, width, length);

	//lseek(fd, hdr_size, SEEK_SET);		/* Skip the file header */
	for (row = 0; row < length; row++) {
		switch(interleaving) {
		case BAND:			/* band interleaved data */
			for (band = 0; band < nbands; band++) {
				/*lseek(fd,
				      hdr_size + (length*band+row)*linebytes,
				      SEEK_SET);
				if (read(fd, buf, linebytes) < 0) {
					fprintf(stderr,
					"%s: %d: scanline %lu: Read error.\n",
					__func__, __LINE__, 
					(unsigned long) row);
				*/
				memcpy(buf, buffer + (length*band+row)*linebytes, linebytes);
				fprintf(stderr,
					"\r%s: %d: BAND scan line %lu...",
					__func__, __LINE__, 
					(unsigned long) row);
				if (swab)	/* Swap bytes if needed */
					swapBytesInScanline(buf, width, dtype);
				for (col = 0; col < width; col++)
					memcpy(buf1 + (col*nbands+band)*depth,
					       buf + col * depth, depth);
			}
			break;
		case PIXEL:			/* pixel interleaved data */
		default:
			/*if (read(fd, buf1, bufsize) < 0) {
				fprintf(stderr,
					"%s:%d: scanline %lu: Read error.\n",
					__func__, __LINE__,
					(unsigned long) row);
				break;
			}*/
			memcpy(buf1, buffer + row*img->widthStep, bufsize);
			if (swab)		/* Swap bytes if needed */
				swapBytesInScanline(buf1, width, dtype);
			/* else if(img->nChannels == 3 ) {
				unsigned short * ushortbuf = (unsigned short *)buf1;
				for(int col=0; col<width; col++, ushortbuf+=3) {
					unsigned short tmp = ushortbuf[0];
					ushortbuf[0] = ushortbuf[2];
					ushortbuf[2] = tmp;
				}
			}*/
			break;
		}
				
		if (TIFFWriteScanline(out, buf1, row, 0) < 0) {
			fprintf(stderr,	"%s:%d : %s: scanline %lu: Write error.\n",
				__func__, __LINE__, outfilename, (unsigned long) row);
			break;
		}
	}
	if (buf)
		_TIFFfree(buf);
	if (buf1)
		_TIFFfree(buf1);
	TIFFClose(out);
	return (0);
}

static void
swapBytesInScanline(void *buf, uint32 width, TIFFDataType dtype)
{
	switch (dtype) {
	case TIFF_SHORT:
	case TIFF_SSHORT:
		TIFFSwabArrayOfShort((uint16*)buf,
				     (unsigned long)width);
		break;
	case TIFF_LONG:
	case TIFF_SLONG:
		TIFFSwabArrayOfLong((uint32*)buf,
				    (unsigned long)width);
		break;
	/* case TIFF_FLOAT: */	/* FIXME */
	case TIFF_DOUBLE:
		TIFFSwabArrayOfDouble((double*)buf,
				      (unsigned long)width);
		break;
	default:
		break;
	}
}

static int
guessSize(char * buffer, long buffer_size, 
	TIFFDataType dtype, off_t /*hdr_size*/, uint32 nbands,
	  int swab, uint32 *width, uint32 *length)
{
	const float longt = 40.0;    /* maximum possible height/width ratio */
	char	    *buf1, *buf2;
	uint32	    w, h, scanlinesize, imagesize;
	uint32	    depth = TIFFDataWidth(dtype);
	float	    cor_coef = 0, tmp;


	imagesize = buffer_size //(filestat.st_size - hdr_size)
			/ nbands / depth;

	if (*width != 0 && *length == 0) {
		fprintf(stderr,	"Image height is not specified.\n");

		*length = imagesize / *width;
		
		fprintf(stderr, "Height is guessed as %lu.\n",
			(unsigned long)*length);

		return 1;
	} else if (*width == 0 && *length != 0) {
		fprintf(stderr, "Image width is not specified.\n");

		*width = imagesize / *length;
		
		fprintf(stderr,	"Width is guessed as %lu.\n",
			(unsigned long)*width);

		return 1;
	} else if (*width == 0 && *length == 0) {
		fprintf(stderr,	"Image width and height are not specified.\n");

		for (w = (uint32) sqrt(imagesize / longt);
		     w < sqrt(imagesize * longt);
		     w++) {
			if (imagesize % w == 0) {
				scanlinesize = w * depth;
				buf1 = (char *)_TIFFmalloc(scanlinesize);
				buf2 = (char *)_TIFFmalloc(scanlinesize);
				h = imagesize / w;
				
				/*
				lseek(fd, hdr_size + (int)(h/2)*scanlinesize,
				      SEEK_SET);
				read(fd, buf1, scanlinesize);
				read(fd, buf2, scanlinesize);
				*/
				memcpy(buf1, buffer + (int)(h/2)*scanlinesize, scanlinesize);
				memcpy(buf2, buffer + ((int)(h/2) + 1)*scanlinesize, scanlinesize);
				
				if (swab) {
					swapBytesInScanline(buf1, w, dtype);
					swapBytesInScanline(buf2, w, dtype);
				}
				tmp = (float) fabs(correlation(buf1, buf2,
							       w, dtype));
				if (tmp > cor_coef) {
					cor_coef = tmp;
					*width = w, *length = h;
				}

				_TIFFfree(buf1);
				_TIFFfree(buf2);
			}
		}

		fprintf(stderr,
			"Width is guessed as %lu, height is guessed as %lu.\n",
			(unsigned long)*width, (unsigned long)*length);

		return 1;
	} 

	return 1;
}



/* Calculate correlation coefficient between two numeric vectors */
static double
correlation(void *buf1, void *buf2, uint32 n_elem, TIFFDataType dtype)
{
	double	X, Y, M1 = 0.0, M2 = 0.0, D1 = 0.0, D2 = 0.0, K = 0.0;
	uint32	i;

	switch (dtype) {
	case TIFF_BYTE:
	default:
		for (i = 0; i < n_elem; i++) {
			X = ((unsigned char *)buf1)[i];
			Y = ((unsigned char *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	case TIFF_SBYTE:
		for (i = 0; i < n_elem; i++) {
			X = ((signed char *)buf1)[i];
			Y = ((signed char *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	case TIFF_SHORT:
		for (i = 0; i < n_elem; i++) {
			X = ((uint16 *)buf1)[i];
			Y = ((uint16 *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	case TIFF_SSHORT:
		for (i = 0; i < n_elem; i++) {
			X = ((int16 *)buf1)[i];
			Y = ((int16 *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	case TIFF_LONG:
		for (i = 0; i < n_elem; i++) {
			X = ((uint32 *)buf1)[i];
			Y = ((uint32 *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	case TIFF_SLONG:
		for (i = 0; i < n_elem; i++) {
			X = ((int32 *)buf1)[i];
			Y = ((int32 *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	case TIFF_FLOAT:
		for (i = 0; i < n_elem; i++) {
			X = ((float *)buf1)[i];
			Y = ((float *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	case TIFF_DOUBLE:
		for (i = 0; i < n_elem; i++) {
			X = ((double *)buf1)[i];
			Y = ((double *)buf2)[i];
			M1 += X, M2 += Y;
			D1 += X * X, D2 += Y * Y;
			K += X * Y;
		}
		break;
	}

	M1 /= n_elem;
	M2 /= n_elem;
	D1 -= M1 * M1 * n_elem;
	D2 -= M2 * M2 * n_elem;
	K = (K - M1 * M2 * n_elem) / sqrt(D1 * D2);

	return K;
}

// in none, packbits,jpeg,lzw,zip

static int
processCompressOptions(char* opt)
{
	if (strcmp(opt, "none") == 0)
		compression = COMPRESSION_NONE;
	else if (strcmp(opt, "packbits") == 0)
		compression = COMPRESSION_PACKBITS;
	else if (strncmp(opt, "jpeg", 4) == 0) {
		char* cp = strchr(opt, ':');

		compression = COMPRESSION_JPEG;
		while( cp )
		{
			if (isdigit((int)cp[1]))
				quality = atoi(cp+1);
			else if (cp[1] == 'r' )
				jpegcolormode = JPEGCOLORMODE_RAW;
			//else
			//    usage();

			cp = strchr(cp+1,':');
		}
	} else if (strncmp(opt, "lzw", 3) == 0) {
		char* cp = strchr(opt, ':');
		if (cp)
			predictor = atoi(cp+1);
		compression = COMPRESSION_LZW;
	} else if (strncmp(opt, "zip", 3) == 0) {
		char* cp = strchr(opt, ':');
		if (cp)
			predictor = atoi(cp+1);
		compression = COMPRESSION_DEFLATE;
	} else
		return (0);
	return (1);
}


//////////////////////////////////////////////////////////////////////////////////////
//					 READ TIFF IMAGES SECTION
// Reference : tiff-3.5.2/tools/tiff2rgba.c
//////////////////////////////////////////////////////////////////////////////////////

static	int tiffcvt(TIFF* in, IplImage ** out);
#define	streq(a,b)	(strcmp(a,b) == 0)
#define	CopyField(tag, v) \
	TIFFGetField(in, tag, &v);

uint32	rowsperstrip = (uint32) -1;
int     no_alpha = 0;

int	process_by_block = 0; /* default is whole image at once */

IplImage * tmOpenTiffImage(const char * filename, int * dpi)
{
	TIFF *in;
	IplImage * out = NULL;
	in = TIFFOpen(filename, "r");
	if (in != NULL) {
		do {
			float resolution_x=0.f, resolution_y=0.f;
			TIFFGetField(in, TIFFTAG_XRESOLUTION, &resolution_x);
			TIFFGetField(in, TIFFTAG_YRESOLUTION, &resolution_y);
			if(dpi) {
				*dpi = (int)tmmax(resolution_x, resolution_y);
			}

			if(!tiffcvt(in, &out)
			//CSE || !TIFFWriteDirectory(out)
				) {
				fprintf(stderr, "%s:%d : cannot use tiffcvt => use cvOpenImage...", __func__, __LINE__);
				//(void) TIFFClose(out);
				return cvLoadImage(filename,
					(CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR)
					);
			}
		} while (TIFFReadDirectory(in));
		(void) TIFFClose(in);
	}


	if(out->nChannels>1) // test only for color pictures
	{
		// We're ok here but the R and B are swapped
		IplImage * out2 = tmCreateImage(cvGetSize(out), out->depth, out->nChannels);
		cvCvtColor(out, out2, CV_BGR2RGB);
		tmReleaseImage(&out);
		out = out2;
	}

	return out;
}


static int
cvt_by_tile( TIFF *in, IplImage *out )
{
	uint32* raster;			/* retrieve RGBA image */
	uint32  width, height;		/* image width & height */
	uint32  tile_width, tile_height;
	uint32  row, col;
	uint32  *wrk_line;
	int	    ok = 1;

	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height);

	if( !TIFFGetField(in, TIFFTAG_TILEWIDTH, &tile_width)
		|| !TIFFGetField(in, TIFFTAG_TILELENGTH, &tile_height) ) {
		TIFFError(TIFFFileName(in), "Source image not tiled");
		return (0);
	}

	fprintf(stderr, "cv2tiff %s:%d : tile: %dx%d\n",
			__func__, __LINE__, tile_width, tile_height);
//	TIFFSetField(out, TIFFTAG_TILEWIDTH, tile_width );
//	TIFFSetField(out, TIFFTAG_TILELENGTH, tile_height );
//
	/*
	 * Allocate tile buffer
	 */
	raster = (uint32*)_TIFFmalloc(tile_width * tile_height * sizeof (uint32));
	if (raster == 0) {
		TIFFError(TIFFFileName(in), "No space for raster buffer");
		return (0);
	}

	/*
	 * Allocate a scanline buffer for swapping during the vertical
	 * mirroring pass.
	 */
	wrk_line = (uint32*)_TIFFmalloc(tile_width * sizeof (uint32));
	if (!wrk_line) {
		TIFFError(TIFFFileName(in), "No space for raster scanline buffer");
		ok = 0;
	}

	/*
	 * Loop over the tiles.
	 */
	for( row = 0; ok && row < height; row += tile_height )
	{
		for( col = 0; ok && col < width; col += tile_width )
		{
			uint32 i_row;

			/* Read the tile into an RGBA array */
// CSE : FIXME : cconvert from RAW : see opencv patch l.
			// UNSUPPORTED IN OpenCV Patch

//			if (!TIFFReadRGBATile(in, col, row, raster)) {
//				ok = 0;
//				break;
//			}

			/*
			 * For some reason the TIFFReadRGBATile() function chooses the
			 * lower left corner as the origin.  Vertically mirror scanlines.
			 */
			for( i_row = 0; i_row < tile_height / 2; i_row++ )
			{
				uint32	*top_line, *bottom_line;

				top_line = raster + tile_width * i_row;
				bottom_line = raster + tile_width * (tile_height-i_row-1);

				_TIFFmemcpy(wrk_line, top_line, 4*tile_width);
				_TIFFmemcpy(top_line, bottom_line, 4*tile_width);
				_TIFFmemcpy(bottom_line, wrk_line, 4*tile_width);
			}

			/*
			 * Write out the result in a tile.
			 */

//			if( TIFFWriteEncodedTile( out,
//									  TIFFComputeTile( out, col, row, 0, 0),
//									  raster,
//									  4 * tile_width * tile_height ) == -1 )
//			{
//				ok = 0;
//				break;
//			}
		}
	}

	_TIFFfree( raster );
	_TIFFfree( wrk_line );

	return ok;
}

static int
	cvt_by_strip( TIFF *in, IplImage *out )
{
	uint32* raster;			/* retrieve RGBA image */
	uint32  width, height;		/* image width & height */
	uint32  row;
	uint32  *wrk_line;
	int	    ok = 1;

	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height);

	if( !TIFFGetField(in, TIFFTAG_ROWSPERSTRIP, &rowsperstrip) ) {
		TIFFError(TIFFFileName(in), "Source image not in strips");
		return (0);
	}

//	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip);

	/*
	 * Allocate strip buffer
	 */
	raster = (uint32*)_TIFFmalloc(width * rowsperstrip * sizeof (uint32));
	if (raster == 0) {
		TIFFError(TIFFFileName(in), "No space for raster buffer");
		return (0);
	}

	/*
	 * Allocate a scanline buffer for swapping during the vertical
	 * mirroring pass.
	 */
	wrk_line = (uint32*)_TIFFmalloc(width * sizeof (uint32));
	if (!wrk_line) {
		TIFFError(TIFFFileName(in), "No space for raster scanline buffer");
		ok = 0;
	}

	/*
	 * Loop over the strips.
	 */
	for( row = 0; ok && row < height; row += rowsperstrip )
	{
		int	rows_to_write, i_row;
/* CSE: commented 8bit version
		// Read the strip into an RGBA array
		if (!TIFFReadRGBAStrip(in, row, raster)) {
			ok = 0;
			break;
		}
*/
		fprintf(stderr, "cv2tiff %s:%d : FIXME : use TIFF\n",
			__func__, __LINE__);
/* From CSE's opencv patch : begin

   From CSE's opencv patch : end */
		/*
		 * Figure out the number of scanlines actually in this strip.
		 */
		if( row + rowsperstrip > height )
			rows_to_write = height - row;
		else
			rows_to_write = rowsperstrip;

		/*
		 * For some reason the TIFFReadRGBAStrip() function chooses the
		 * lower left corner as the origin.  Vertically mirror scanlines.
		 */

		for( i_row = 0; i_row < rows_to_write / 2; i_row++ )
		{
			uint32	*top_line, *bottom_line;

			top_line = raster + width * i_row;
			bottom_line = raster + width * (rows_to_write-i_row-1);

			_TIFFmemcpy(wrk_line, top_line, 4*width);
			_TIFFmemcpy(top_line, bottom_line, 4*width);
			_TIFFmemcpy(bottom_line, wrk_line, 4*width);
		}

		/*
		 * Write out the result in a strip
		 */
		fprintf(stderr, "cv2tiff %s:%d : FIXME : use TIFF\n",
			__func__, __LINE__);

//		if( TIFFWriteEncodedStrip( out, row / rowsperstrip, raster,
//								   4 * rows_to_write * width ) == -1 )
//		{
//			ok = 0;
//			break;
//		}
	}

	_TIFFfree( raster );
	_TIFFfree( wrk_line );

	return ok;
}

/*
 * cvt_whole_image()
 *
 * read the whole image into one big RGBxdepth buffer and then write out
 * strips from that.  This is using the traditional TIFFReadRGBAImage()
 * API that we trust.
 */

static int cvt_whole_image( TIFF *in, IplImage *out )
{
	uint32  width, height;		/* image width & height */
	int32 tile_rowperstrip = 0, tile_width = 0, tile_height = 0;


	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height);

	TIFFGetField( in, TIFFTAG_ROWSPERSTRIP, &tile_rowperstrip);
	TIFFGetField( in, TIFFTAG_TILEWIDTH, &tile_width );
	TIFFGetField( in, TIFFTAG_TILELENGTH, &tile_height );

//	rowsperstrip = TIFFDefaultStripSize(out, rowsperstrip);
//	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip);
	fprintf(stderr, "cv2tiff %s:%d : TIFF Info : "
			"width=%d height=%d\n",
			__func__, __LINE__,
			(int)width, (int)height);

	fprintf(stderr, "cv2tiff %s:%d : TIFF Info TILE: "
			"tile_width=%d tile_height=%d tile_rowperstrip=%d\n",
			__func__, __LINE__,
			(int)tile_width, (int)tile_height,
			(int)tile_rowperstrip );
	if( tile_width <= 0 ) {
		fprintf(stderr, "cv2tiff %s:%d : force tile_width=width=%d",
				__func__, __LINE__, width);
		tile_width = width;
	}

	// allocate
	int32 bitpersample = 0;
	TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitpersample);
	int32 channels=1;		/* image width & height */
	TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &channels);
	if( tile_height <= 0 ) {
		tile_height = 1; //height;
	}

	int bufsize = tile_height * tile_width * channels * bitpersample / 8;

	uchar * buffer = new uchar[bufsize];
	tdata_t buf = (tdata_t)0;
	int byte_depth = bitpersample/8;

	uchar * data = (uchar *)(out->imageData);
	int pitch = out->widthStep;


	/* Read the image in one chunk into an RGBA array *
	fprintf(stderr, "cv2tiff %s:%d : => step: step=%d * tile_height=%d\n",
			__func__, __LINE__, step, tile_height);
	*/
	uint32 y,x;
	for( y = 0; y < height; y += tile_height,
		 data += pitch*tile_height )
	{
		if( y + tile_height > height ) {
			tile_height = height - y;
		}

		for( x = 0; x < width; x += tile_width )
		{
			if( x + tile_width > width ) {
				tile_width = width - x;
			}

			/*if(bitpersample != 8)*/
			{
				uint16  config = PLANARCONFIG_CONTIG;
				TIFFGetField(in, TIFFTAG_PLANARCONFIG, &config);
				if(!buf) {
					buf = _TIFFmalloc(TIFFScanlineSize(in));
					fprintf(stderr, "\t%s:%d : TIFFScanlineSize=%d\n",
							__func__, __LINE__,
							(int)TIFFScanlineSize(in)
							);fflush(stderr);
				}

				if (config == PLANARCONFIG_CONTIG) {
					//for (row = 0; row < imagelength; row++)
					//fprintf(stderr, "\r%s:%d : line %d : TIFFReadScanline(row=%d)...",
					//      __func__, __LINE__, y, y); fflush(stderr);

					if(TIFFReadScanline(in, buf, y) /*row*/ == 1) {

/*fprintf(stderr, "\r\t\t%s:%d : line y=%d x=%d : copy buf=%p into "
		"data=%p nbytes=%d=%dx%dx%d",
		__func__, __LINE__, y, x,
		buf,
		data,
		tile_width*byte_depth*channels,
		tile_width, byte_depth, channels
		); fflush(stderr);
*/
						memcpy(data,
							   buf,
							   tile_width*byte_depth*channels
							   );
						//else memset(data + x + step*y, 255, tile_width*m_bit_depth*m_channels/8);
					}
					else fprintf(stderr, "\t%s:%d : ERROR : failed for line %d\n", __func__, __LINE__, y);

				} /*else if (config == planarconfig_separate) {
														uint16 s, nsamples;

														tiffgetfield(tif, tifftag_samplesperpixel, &nsamples);
														for (s = 0; s < nsamples; s++)
																for (row = 0; row < imagelength; row++)
																		tiffreadscanline(tif, buf, row, s);
												}*/

			}
		}
	}

	if(buf) {
		_TIFFfree(buf);
		buf = NULL;
	}

	delete [] buffer;

	if(g_debug_importexport) {
		// DEBUG
		fprintf(stderr, "%s %s:%d : saving for debug : "
				"%s/cvt_whole_image.ppm",
				__FILE__, __func__, __LINE__, g_tmp_directory);
		char debug_file[512];
		sprintf(debug_file, "%s/cvt_whole_image.ppm",
				g_tmp_directory);
		tmSaveImage(debug_file, out);
	}
	return 1;
}


static int
	tiffcvt(TIFF* in, IplImage ** out)
{
	uint16 shortv;
	char *stringv;
	//uint32 longv;

	uint32 width, height;
	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height);
	int32 bitpersample = 0;
	TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitpersample);
	int32 channels=1;		/* image width & height */
	TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &channels);

	fprintf(stderr, "cv2tiff %s:%d : image : %d x %d x channels=%d x bitpersample=%d\n",
			__func__, __LINE__,
			(int)width, (int)height, (int)channels, (int)bitpersample);

	int ipl_depth = IPL_DEPTH_8U;
	switch(bitpersample) {
	default:
		fprintf(stderr, "\t%s:%d : unsupported bitpersample=%d\n", __func__ ,__LINE__, bitpersample);
		break;
	case 8:
		ipl_depth = IPL_DEPTH_8U;

		// Use OpenCV function to read this image !!

		break;
	case 16:
		ipl_depth = IPL_DEPTH_16U;
		break;
	}


	if(ipl_depth == IPL_DEPTH_8U) {
//		return -1; // no need for 16bit
	}

	fprintf(stderr, "cv2tiff %s:%d : create IplImage ((%d,%d), %d, %d)\n",
			__func__, __LINE__,
			(int)width, (int)height,
			ipl_depth, (int)channels);
	if(*out) {
		tmReleaseImage(out);
	}

	*out = tmCreateImage(cvSize(width, height), ipl_depth, channels);

	int16 fillorder=0;
	TIFFGetField(in, TIFFTAG_PHOTOMETRIC, &shortv);
	switch(shortv) {
	default:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_MINISWHITE:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      0       /* min value is white */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_MINISBLACK:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      1       /* min value is black */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_RGB:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      2       /* RGB color model */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_PALETTE:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      3       /* color map indexed */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_MASK:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      4       /* $holdout mask */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_SEPARATED:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      5       /* !color separations */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_YCBCR:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      6       /* !CCIR 601 */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_CIELAB:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      8       /* !1976 CIE L*a*b* */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_ICCLAB:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      9       /* ICC L*a*b* [Adobe TIFF Technote 4] */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_ITULAB:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      10      /* ITU L*a*b* */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_LOGL:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      32844   /* CIE Log2(L) */\n",
				__func__, __LINE__, shortv);
		break;
	case PHOTOMETRIC_LOGLUV:
		fprintf(stderr, "cv2tiff %s:%d : photometric=%u=      32845   /* CIE Log2(L) (u',v') */\n",
				__func__, __LINE__, shortv);
		break;
	}

	TIFFGetField(in, TIFFTAG_FILLORDER, &fillorder);
	switch(fillorder) {
	default:
		fprintf(stderr, "cv2tiff %s:%d : unknown fillorder=%u\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_TOPLEFT:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=TOPLEFT= 1 /* row 0 top, col 0 lhs */\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_TOPRIGHT:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=TOPRIGHT= 2 /* row 0 top, col 0 rhs */\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_BOTRIGHT:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=BOTRIGHT= 3 /* row 0 bottom, col 0 rhs */\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_BOTLEFT:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=BOTLEFT= 4 /* row 0 bottom, col 0 lhs */\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_LEFTTOP:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=LEFTTOP= 5 /* row 0 lhs, col 0 top */\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_RIGHTTOP:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=RIGHTTOP= 6 /* row 0 rhs, col 0 top */\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_RIGHTBOT:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=RIGHTBOT= 7 /* row 0 rhs, col 0 bottom */\n",
				__func__, __LINE__, fillorder);
		break;
	case ORIENTATION_LEFTBOT:
		fprintf(stderr, "cv2tiff %s:%d : fillorder=%u=LEFTBOT = 8 /* row 0 lhs, col 0 bottom */\n",
				__func__, __LINE__, fillorder);
		break;
	}

	// Read resolution in header
	// => used in Tamanoir for setting dust detection sizes
	float resolution_x=0, resolution_y=0;
	TIFFGetField(in, TIFFTAG_XRESOLUTION, &resolution_x);
	TIFFGetField(in, TIFFTAG_YRESOLUTION, &resolution_y);
	fprintf(stderr, "cv2tiff %s:%d : resolution: %gx%g\n",
			__func__, __LINE__,
			resolution_x, resolution_y);
	CopyField(TIFFTAG_RESOLUTIONUNIT, shortv);
//	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
//	TIFFSetField(out, TIFFTAG_SOFTWARE, TIFFGetVersion());
	CopyField(TIFFTAG_DOCUMENTNAME, stringv);

	fprintf(stderr, "cv2tiff %s:%d : process_by_block=%c TIFFisTiled=%c\n",
			__func__, __LINE__,
			process_by_block ? 'T':'F',
			TIFFIsTiled( in ) ? 'T':'F');

	if( process_by_block && TIFFIsTiled( in ) )
		return( cvt_by_tile( in, *out ) );
	else if( process_by_block )
		return( cvt_by_strip( in, *out ) );
	else
		return( cvt_whole_image( in, *out ) );
}


