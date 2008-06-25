/***************************************************************************
 *           tamanoirapp.cpp - Main application
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
#include "tamanoir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgproc.h"

// include files for QT
#include <QFileInfo>
#include <QFileDialog>

/* File for logging messages */
extern FILE * logfile;


extern u8 g_debug_imgverbose;
extern u8 g_debug_savetmp;
extern u8 g_debug_correlation;

QImage iplImageToQImage(IplImage * iplImage);

/** constructor */
TamanoirApp::TamanoirApp(QWidget * l_parent) 
	: QMainWindow(l_parent)
{
	fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	statusBar()->showMessage( QString("") );
	
	m_pImgProc = NULL;
	QString homeDirStr = QString("/home/");
	if(getenv("HOME"))
		homeDirStr = QString(getenv("HOME"));
	m_currentDir = homeDirStr;
	
	ui.setupUi((QMainWindow *)this);
	
	
	// Connect cropPixmapLabel to mouse handling
	connect(ui.cropPixmapLabel, SIGNAL(signalMousePressEvent(QMouseEvent *)), this, SLOT(on_cropPixmapLabel_mousePressEvent(QMouseEvent *)));
	
	
	ui.loadingTextLabel->setText(QString(""));
	
	m_filmType = ui.typeComboBox->currentIndex();
	m_trust = ui.trustCheckBox->isChecked();
	m_hotPixels = ui.hotPixelsCheckBox->isChecked();
}

void TamanoirApp::on_cropPixmapLabel_mousePressedEvent(QMouseEvent * e) {
	if(e && m_pImgProc) {
		m_pImgProc->setCopySrc(e->pos().x(), e->pos().y());
		updateDisplay();
	}
}


/** destructor */
TamanoirApp::~TamanoirApp() {
	
}


void TamanoirApp::setArgs(int argc, char **argv) {
	ui.loadingTextLabel->setText(QString(""));
	statusBar()->showMessage( QString("") );
	
	QString argsStr = tr("Args=");
	if(argc > 1) {
		for(int arg=1; arg<argc; arg++) {
			loadFile( argv[arg] );
			argsStr += QString(argv[arg]);
		}
		statusBar()->showMessage( argsStr );
	}
}

/****************************** Button slots ******************************/
void TamanoirApp::on_loadButton_clicked() 	
{
	ui.loadingTextLabel->setText(tr(""));
	fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	QString s = QFileDialog::getOpenFileName(this,
                   	tr("open file dialog"),
                   	m_currentDir,
                    tr("Images (*.png *.p*m *.xpm *.jp* *.tif* *.bmp"
								"*.PNG *.P*M *.XPM *.JP* *.TIF* *.BMP)"));
	if(s.isEmpty()) {
		fprintf(stderr, "TamanoirApp::%s:%d : cancelled...\n", __func__, __LINE__);
		return;
	}

	loadFile( s);
}

void TamanoirApp::loadFile(QString s) {
	QFileInfo fi(s);
	if(!fi.exists()) 
		return;
	m_currentFile = s;
	
	m_currentDir = fi.absolutePath();
	
	
	statusBar()->showMessage( tr("Loading and pre-processing ") + m_currentFile + QString("..."));
	statusBar()->update();
	
	fprintf(stderr, "TamanoirApp::%s:%d : file='%s'...\n", 
		__func__, __LINE__, s.latin1());
	// Open file
	if(!m_pImgProc) {
		m_pImgProc = new TamanoirImgProc();
		m_pImgProc->setHotPixelsFilter(m_hotPixels);
		m_pImgProc->setTrustCorrection(m_trust);
		m_pImgProc->setResolution(m_dpi);
	}
	int ret = m_pImgProc->loadFile(s.ascii());
	if(ret < 0) {
		
		QMessageBox::critical( 0, tr("Tamanoir"),
			tr("Cannot load file ") + s + tr(". Format or compression is not compatible"));
			
		return;
	}
	

        refreshMainDisplay();
	
	statusBar()->showMessage( tr("Loaded and pre-processed ") + fi.fileName() );
	ui.loadingTextLabel->setText( fi.fileName() );
	
	ui.overAllProgressBar->setValue(0);
	
	if(!g_debug_imgverbose) {
		
		m_pImgProc->firstDust();
	}
	
	
	// Update little frame displays
	updateDisplay();
}

void TamanoirApp::on_saveButton_clicked()
{
	fprintf(stderr, "TamanoirApp::%s:%d : saving in original file, and use a copy for backup...\n", __func__, __LINE__);
	
	QFileInfo fi(m_currentFile);
	QString ext = fi.extension(FALSE);
	QString base = fi.baseName( TRUE ); 
       
	
	
    // Save a copy before saving output image
	QString copystr = base + tr("-copy.") + ext;
	if(m_pImgProc) {
		QString msg = tr("Saved ") + m_currentFile;
		
		// Save a copy 
		QDir dir( fi.dirPath(TRUE) );
		dir.rename(m_currentFile, copystr);
		msg+= tr(" + backup as ") + copystr;
		
		// Save image
		m_pImgProc->saveFile(m_currentFile.ascii());
		
		
		statusBar()->showMessage( msg );
		
		// Store statistics
		dust_stats_t stats = m_pImgProc->getDustStats();
		processAndPrintStats(&stats);
		
		// Display in progress bar
		
	}
}



void TamanoirApp::on_skipButton_clicked()
{
	if(m_pImgProc) {
		int ret = m_pImgProc->nextDust();
		
		if(ret == 0) // Finished
		{
			ui.overAllProgressBar->setValue(100);
			statusBar()->showMessage(tr("Finished"));
			
			return;
		}
			
		updateDisplay();
		
		
	}
}

void TamanoirApp::on_correctButton_clicked()
{
	// Apply previous correction
	if(m_pImgProc)
		m_pImgProc->applyCorrection();
	
	// Then go to next dust
	on_skipButton_clicked();
}



void TamanoirApp::on_autoButton_clicked()
{
	statusBar()->showMessage(tr("Auto-correct running... please wait."));
	statusBar()->update();
	
	fprintf(stderr, "TamanoirApp::%s:%d : AUTO MODE ...\n", __func__, __LINE__);
	if(logfile == stderr) {
		char logfilename[512] = TMP_DIRECTORY "tamanoir.txt";
		
		QFileInfo fi(m_currentFile);
		//QString abspathname = fi.absPathName();
		QString ext = fi.extension();  // ext = ".jpg"
		
		sprintf(logfilename, "%s%s.txt", TMP_DIRECTORY, fi.baseName(false).latin1());
		
		logfile = fopen(logfilename, "w");
		if(!logfile) {
			g_debug_imgverbose = 0;
			logfile = stderr;
		} else {
			fprintf(stderr, "TamanoirApp::%s:%d : Logging messages in '%s'\n", __func__, __LINE__,
				logfilename);
		}
	} else {
		g_debug_imgverbose = 0;
	}	
	g_debug_savetmp = 0;
        
	// Apply previous correction
	if(m_pImgProc) {
		int ret = 1;
                int progressDiv = 0;
                
		do {
			m_pImgProc->applyCorrection();
	
			// Then go to next dust
			ret = m_pImgProc->nextDust();
			
                        // Periodically refresh display
                        int l_progress = m_pImgProc->getProgress();
                        if((l_progress / 10) > progressDiv) {
                                progressDiv = (l_progress / 10);
                                updateDisplay();
                        }
            
			ui.overAllProgressBar->setValue(l_progress);
			
		} while(ret == 1);
	}
	fflush(logfile);
	
        
	ui.overAllProgressBar->setValue(100);
	
	// Update little frame displays
	updateDisplay();
        
        
	statusBar()->showMessage( tr("Auto-correct : done.") );
	
	fprintf(stderr, "TamanoirApp::%s:%d : AUTO MODE FINISHED\n", __func__, __LINE__);
}



void TamanoirApp::resizeEvent(QResizeEvent *) {
}






void TamanoirApp::on_typeComboBox_currentIndexChanged(int i) {
	fprintf(stderr, "TamanoirApp::%s:%d : film changed to type %d ...\n", 
		__func__, __LINE__, i);
	statusBar()->showMessage( tr("Changed film type: please wait...") );
	statusBar()->update();
	
	m_filmType = i;
	if(m_pImgProc) {
		m_pImgProc->setFilmType(i);
		
		// Refresh main display
		refreshMainDisplay();
		
		updateDisplay();
	}
	statusBar()->showMessage( tr("Changed film type: done.") );
	statusBar()->update();
}

void TamanoirApp::on_dpiComboBox_currentIndexChanged(QString str) {
	fprintf(stderr, "TamanoirApp::%s:%d : resolution changed to type %s ...\n", 
		__func__, __LINE__, str.ascii());
	statusBar()->showMessage( tr("Changed resolution: please wait...") );
	statusBar()->update();
	
	int dpi = 2400;
	if(sscanf(str.ascii(), "%d", &dpi) != 1)
		m_dpi = 2400;
	else
		m_dpi = dpi;
	
	if(m_pImgProc) {
		m_pImgProc->setResolution(m_dpi);
		
		
		// Refresh main display
		refreshMainDisplay();
		
		updateDisplay();
	}
	statusBar()->showMessage( tr("Changed scan resolution: done.") );
	statusBar()->update();
}

void TamanoirApp::on_trustCheckBox_toggled(bool on) {
	statusBar()->showMessage(tr("Changed to 'trust' mode : " + (on?tr("ON"):tr("OFF"))));
	
	m_trust = on;
	
	if(m_pImgProc) {
		m_pImgProc->setTrustCorrection(on);
	}
}

void TamanoirApp::on_hotPixelsCheckBox_toggled(bool on) {
	statusBar()->showMessage( tr("Changed hot pixels filter: please wait...") );
	statusBar()->update();
	m_hotPixels = on;
	if(m_pImgProc) {
		m_pImgProc->setHotPixelsFilter(on);
		
		// Refresh main display
		refreshMainDisplay();
		
		updateDisplay();
	}
	
	statusBar()->showMessage( tr("Changed hot pixels filter: done.") );
	statusBar()->update();
}

void TamanoirApp::on_cropPixmapLabel_customContextMenuRequested(QPoint p)
{
	fprintf(stderr, "TamanoirApp::%s:%d : clicked on %d,%d", __func__, __LINE__, p.x(), p.y());
	
}





QImage iplImageToQImage(IplImage * iplImage) {
	int depth = iplImage->nChannels;
	bool rgb24_to_bgr32 = false;
	if(depth == 3) {// RGB24 is obsolete on Qt => use 32bit instead
		depth = 4;
		rgb24_to_bgr32 = true;
	}
	
	
	int orig_width = iplImage->width;
	if((orig_width % 2) == 1)
		orig_width--;
	QImage qImage(orig_width, iplImage->height, 8*depth);
	memset(qImage.bits(), 0, orig_width*iplImage->height*depth);
	
	if(!rgb24_to_bgr32) {
		for(int r=0; r<iplImage->height; r++)
			memcpy(qImage.bits() + r*orig_width, 
				iplImage->imageData + r*iplImage->widthStep, orig_width*depth);
	}
	else {
		u8 * buffer3 = (u8 *)iplImage->imageData;
		u8 * buffer4 = (u8 *)qImage.bits();
		for(int r=0; r<iplImage->height; r++)
		{
			int pos3 = r * iplImage->widthStep;
			int pos4 = r * orig_width*4;
			for(int c=0; c<orig_width; c++, pos3+=3, pos4+=4)
			{
				//buffer4[pos4 + 2] = buffer3[pos3];
				//buffer4[pos4 + 1] = buffer3[pos3+1];
				//buffer4[pos4    ] = buffer3[pos3+2];
				buffer4[pos4   ] = buffer3[pos3];
				buffer4[pos4 + 1] = buffer3[pos3+1];
				buffer4[pos4 + 2] = buffer3[pos3+2];
			}
		}
	}
	
	if(iplImage->nChannels == 1) {
		qImage.setNumColors(256);
		for(int c=0; c<256; c++) 
			qImage.setColor(c, qRgb(c,c,c));
	}
	return qImage;
}
void TamanoirApp::refreshMainDisplay() {
	if(!m_pImgProc) return;
	
	int scaled_width = ui.largViewFrame->width()-12;
        int scaled_height = ui.largViewFrame->height()-12;
        
	m_pImgProc->setDisplaySize(scaled_width, scaled_height);
}




void TamanoirApp::updateDisplay()
{
	if(m_pImgProc) {
		// After pre-processing, we can get the grayscale version of input image
		IplImage * curImage = m_pImgProc->getGrayscale();
		
		if(curImage) {
			IplImage * displayImage = m_pImgProc->getDisplayImage();
			
			// Display in main frame
			int gray_width = displayImage->widthStep;
			int scaled_width = displayImage->widthStep;
			int scaled_height = displayImage->height;
			
			QImage grayQImage(gray_width, displayImage->height, 8);
			memcpy(grayQImage.bits(), displayImage->imageData, displayImage->widthStep * displayImage->height);
			
			grayQImage.setNumColors(256);
			for(int c=0; c<256; c++) 
				grayQImage.setColor(c, qRgb(c,c,c));
			
			grayQImage.setColor(COLORMARK_CORRECTED, qRgb(0,255,0));
			grayQImage.setColor(COLORMARK_REFUSED, qRgb(255,255,0));
			grayQImage.setColor(COLORMARK_FAILED, qRgb(255,0,0));
			grayQImage.setColor(COLORMARK_CURRENT, qRgb(0,0,255));
			
			QPixmap pixmap;
			
			/*
			 fprintf(stderr, "TamanoirApp::%s:%d : => grayscaled display : %dx%d\n", 
					__func__, __LINE__, 
					largViewFrame->width(),largViewFrame->height());
			*/
			pixmap.convertFromImage( grayQImage ); //, QImage::ScaleMin);
			
			/*
			fprintf(stderr, "TamanoirApp::%s:%d : pixmap=%dx%d => Scaled=%dx%d\n", __func__, __LINE__, 
									pixmap.width(), pixmap.height(),
									scaled_width, scaled_height);
			*/
			
			//mainPixmapLabel->setFixedSize(scaled_width,scaled_height);
			/*
			mainPixmapLabel->setGeometry(largViewFrame->x() + (largViewFrame->width()-scaled_width)/2,
																	 largViewFrame->y() + (largViewFrame->height()-scaled_height)/2,
																	 scaled_width, scaled_height);
			*/
			ui.mainPixmapLabel->setGeometry((ui.largViewFrame->width()-scaled_width+1)/2,
										(ui.largViewFrame->height()-scaled_height+1)/2,
										scaled_width+2, scaled_height+2);
			ui.mainPixmapLabel->setPixmap(pixmap);
		}
		
		// Display images
		curImage = m_pImgProc->getCrop();
		if(curImage) {
			QLabel * pLabel = ui.cropPixmapLabel;
			
			// Display in frame
			QImage grayQImage = iplImageToQImage(curImage);
			QPixmap pixmap;
			pixmap.convertFromImage( 
				grayQImage.scaledToWidth(pLabel->width()),
					QPixmap::Color);
			pLabel->setPixmap(pixmap);
			pLabel->repaint();
		}
		
		
		// Mask image = dust in white on black background
		IplImage * maskImage = m_pImgProc->getMask();
		curImage = maskImage;
		if(curImage) {
			QLabel * pLabel = ui.growPixmapLabel;
			
			
			// Display in frame
			QImage grayQImage = iplImageToQImage(curImage);
			QPixmap pixmap;
			pixmap.convertFromImage( 
				grayQImage.scaledToWidth(pLabel->width()),
					QPixmap::Color);
			pLabel->setPixmap(pixmap);
			pLabel->repaint();
		}
		
		// Proposed correction
		IplImage * correctImage = m_pImgProc->getCorrectedCrop();
		curImage = correctImage;
		if(curImage) {
			QLabel * pLabel = ui.correctPixmapLabel;
			
			// Display in frame
			QImage grayQImage = iplImageToQImage(curImage);
			QPixmap pixmap;
			pixmap.convertFromImage( 
				grayQImage.scaledToWidth(pLabel->width()),
				QPixmap::Color);
			pLabel->setPixmap(pixmap);
			pLabel->repaint();
		}
		
		
		// Display dust info
		CvConnectedComp dust = m_pImgProc->getDustComp();
		char strinfo[32];
		sprintf(strinfo, "%dx%d / %d pix",
			(int)dust.rect.width, (int)dust.rect.height, (int)dust.area);
		QString str = tr("Dust: ") + QString(strinfo);
		ui.growTextLabel->setText(str);
		
		// Display diff image
		IplImage * grayImage = m_pImgProc->getDiffCrop();
		if(g_debug_correlation)
			grayImage = getCorrelationImage();
		
		if(grayImage) {
			QLabel * pLabel = ui.diffPixmapLabel;

			// Display in frame
			QImage grayQImage = iplImageToQImage(grayImage);
			QPixmap pixmap;
			pixmap.convertFromImage( grayQImage.smoothScale(pLabel->width(),pLabel->height()),
				QPixmap::Color);
			pLabel->setPixmap(pixmap);
			pLabel->repaint();
		}

		ui.overAllProgressBar->setValue(m_pImgProc->getProgress());
		
	} else {
		ui.overAllProgressBar->setValue(0);
	}
}
