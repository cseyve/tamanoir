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


/** constructor */
TamanoirApp::TamanoirApp(QWidget * l_parent) 
	: QMainWindow(l_parent)
{
	fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	statusBar()->showMessage( QString("") );
	
	m_pImgProc = NULL;
	m_pProcThread = NULL;
	
	QString homeDirStr = QString("/home/");
	if(getenv("HOME"))
		homeDirStr = QString(getenv("HOME"));
	strcpy(m_options.currentDir, homeDirStr.ascii());
	
	ui.setupUi((QMainWindow *)this);
	
	// Load last options
	loadOptions();
	
	connect(&refreshTimer, SIGNAL(timeout()), this, SLOT(on_refreshTimer_timeout()));

	
	ui.loadingTextLabel->setText(QString(""));
	
}

void TamanoirApp::on_refreshTimer_timeout() {
	if(m_pProcThread) {
		if(refreshTimer.isActive() && 
			m_pProcThread->getCommand() == PROTH_NOTHING) {
			// Stop timer
			refreshTimer.stop();
		}
		
		fprintf(stderr, "TamanoirApp::%s:%d : m_curCommand=%d m_pProcThread=%d\n", __func__, __LINE__,
			m_curCommand, m_pProcThread->getCommand());
		
		// If nothing changed, just update GUI
		if( m_curCommand == m_pProcThread->getCommand()) {
			// Update Progress bar
			ui.overAllProgressBar->setValue( m_pImgProc->getProgress() );
			
			// Do specific updates
			switch(m_curCommand) {
			default:
				break;
			case PROTH_SEARCH:
				refreshMainDisplay();
				updateDisplay();
				break;
			}
			
			return;
		}
		
			
		// If we WERE loading a file and now it's done
		if( (m_curCommand == PROTH_LOAD_FILE 
			|| m_curCommand == PROTH_OPTIONS)
			&& m_pProcThread->getCommand() == PROTH_NOTHING) {
			refreshMainDisplay();
			
			QFileInfo fi(m_currentFile);
			QString str;
			IplImage * curImage = m_pImgProc->getOriginal();
			if(!curImage) {
				statusBar()->showMessage( tr("Could not load file ") 
					+ fi.fileName() );
			} else {
				str.sprintf("%d x %d x %dbit", curImage->width, curImage->height, 8*tmByteDepth(curImage));
				
				statusBar()->showMessage( tr("Loaded and pre-processed. Image: ") 
					+ fi.fileName() + tr(". Size:") + str);
				ui.loadingTextLabel->setText( fi.fileName() );
			}
			
			ui.overAllProgressBar->setValue(0);
			
			
			if(!g_debug_imgverbose) {
				
				m_pImgProc->firstDust();
			}
			
			
			// Update little frame displays
			updateDisplay();
		}
	}
}


void TamanoirApp::on_cropPixmapLabel_signalMousePressEvent(QMouseEvent * e) {
	
	//fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	if(e && m_pProcThread) {
		
		m_pImgProc->setCopySrc(m_pProcThread->getCorrectionPointer(),
			e->pos().x(), e->pos().y());
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
			
			if(argv[arg][0] == '-') //option
			{
				fprintf(stderr, "TamanoirApp::%s:%d option '%s'\n", 
					__func__, __LINE__, argv[arg]);
				if(strcasestr(argv[arg], "debug")) // All debug options
					g_debug_savetmp = g_debug_imgverbose = 1;
				if(strcasestr(argv[arg], "save")) // All debug options
					g_debug_savetmp = 1;
				
			} else {
				loadFile( argv[arg] );
			}
			
			
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
                   	m_options.currentDir,
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
	
	strcpy(m_options.currentDir, fi.absolutePath().ascii());
	saveOptions();
	
	statusBar()->showMessage( tr("Loading and pre-processing ") + m_currentFile + QString("..."));
	statusBar()->update();
	
	fprintf(stderr, "TamanoirApp::%s:%d : file='%s'...\n", 
		__func__, __LINE__, s.latin1());
	// Open file
	if(!m_pImgProc) {
		m_pImgProc = new TamanoirImgProc();
		m_pImgProc->setHotPixelsFilter(m_options.hotPixels);
		m_pImgProc->setTrustCorrection(m_options.trust);
		m_pImgProc->setResolution(m_options.dpi);
		m_pImgProc->setFilmType(m_options.filmType);
	}
	if(!m_pProcThread) {
		m_pProcThread = new TamanoirThread(m_pImgProc);
	}
	
	int ret = m_pProcThread->loadFile(s.ascii());
	if(ret < 0) {
		
		QMessageBox::critical( 0, tr("Tamanoir"),
			tr("Cannot load file ") + s + tr(". Format or compression is not compatible"));
		
		return;
	}
	
	m_curCommand = m_pProcThread->getCommand();
	refreshTimer.start(500);
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
		QString msg = tr("Saveing ") + m_currentFile;
		
		// Save a copy 
		QDir dir( fi.dirPath(TRUE) );
		dir.rename(m_currentFile, copystr);
		msg+= tr(" + backup as ") + copystr;
		
		// Save image
		m_pProcThread->saveFile(m_currentFile.ascii());
		
		
		statusBar()->showMessage( msg );
		
		// Store statistics
		dust_stats_t stats = m_pImgProc->getDustStats();
		processAndPrintStats(&stats);
		
		// Display in progress bar
		
	}
}

int TamanoirApp::loadOptions() {
	// 
	char homedir[512] = ".";
	if(getenv("HOME")) {
		strcpy(homedir, getenv("HOME"));
	}
	optionsFile = QString(homedir) + QString("/.tamanoirrc");
	
	// Read 
	FILE * foptions = fopen(optionsFile.ascii(), "r");
	if(!foptions) {
		// Update options
		memset(&m_options, 0, sizeof(tm_options));
		m_options.filmType = ui.typeComboBox->currentIndex();
		m_options.trust = ui.trustCheckBox->isChecked();
		m_options.hotPixels = ui.hotPixelsCheckBox->isChecked();
		on_dpiComboBox_currentIndexChanged(ui.dpiComboBox->currentText());
		
		return 0;
	}
	
	
	char line[512], *ret=line;
	while(ret && !feof(foptions)) {
		line[0] = '#';
		ret= fgets(line, 511, foptions);
		if(ret) {
			if(strlen(line)>1 && line[0] != '#' && line[0] != '\n' && line[0] != '\r' ) {
				if(line[strlen(line)-1]=='\r')
					line[strlen(line)-1]='\0';
				if(strlen(line)>1)
					if(line[strlen(line)-1]=='\n')
						line[strlen(line)-1]='\0';
				
				char * separation = strstr(line, ":");
				if(separation) {
					*separation = '\0';
					char * cmd = line, *arg = separation+1;
	
					if(strcasestr(cmd, "dir")) {
						strcpy(m_options.currentDir, arg);
					} else 
					if(strcasestr(cmd, "trust")) {
						m_options.trust = (arg[0]=='T');
					} else
					if(strcasestr(cmd, "hot")) {
						m_options.hotPixels = (arg[0]=='T');
					} else
					if(strcasestr(cmd, "film")) {
						m_options.filmType = atoi(arg);
					} else
					if(strcasestr(cmd, "dpi")) {
						m_options.dpi = atoi(arg);
					} 
				}
			}
		}
					
		// 
		
	}
	
	fclose(foptions);
	
	// Update GUI with those options
	ui.typeComboBox->setCurrentIndex( m_options.filmType );
	ui.trustCheckBox->setChecked( m_options.trust );
	ui.hotPixelsCheckBox->setChecked( m_options.hotPixels );
	QString str;
	str.sprintf("%d", m_options.dpi);
	int ind = ui.dpiComboBox->findText(str, Qt::MatchContains);
	if(ind >= 0)
		ui.dpiComboBox->setCurrentIndex(ind);
		
	return 1;
}

/* Tamanoir settings/options *
typedef struct {
	QString currentDir;
	bool trust;		/ *! Trust mode activated * /
	bool hotPixels;	/ *! Hot pixels detection activated * /
	int filmType;	/ *! Film type * /
	int dpi;		/ *! Scan resolution in dot per inch * /
} tm_options;
*/
void fprintfOptions(FILE * f, tm_options * p_options) {
	if(!f) return;
	
	fprintf(f, "CurrentDir:%s\n", p_options->currentDir );
	fprintf(f, "Trust:%c\n", p_options->trust ? 'T' : 'F');
	fprintf(f, "HotPixels:%c\n", p_options->hotPixels ? 'T' : 'F');
	fprintf(f, "FilmType:%d\n", p_options->filmType);
	fprintf(f, "DPI:%d\n", p_options->dpi);
	fflush(f);
}

void TamanoirApp::saveOptions() {
	FILE * foptions = fopen(optionsFile.ascii(), "w");
	if(!foptions) {
		return;
	}
	
	fprintfOptions(foptions, &m_options);
	fclose(foptions);
}


void TamanoirApp::on_skipButton_clicked()
{
	if(m_pProcThread) {
		int ret = m_pProcThread->nextDust();
		
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
		m_pImgProc->applyCorrection(m_pProcThread->getCorrection());
	
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
	
	m_options.filmType = i;
	
	if(m_pProcThread) {
		m_curCommand = m_pProcThread->setOptions(m_options);
		refreshTimer.start(250);
	} else m_curCommand = PROTH_NOTHING;
	

	
	saveOptions();
}

void TamanoirApp::on_dpiComboBox_currentIndexChanged(QString str) {
	fprintf(stderr, "TamanoirApp::%s:%d : resolution changed to type %s ...\n", 
		__func__, __LINE__, str.ascii());
	statusBar()->showMessage( tr("Changed resolution: please wait...") );
	statusBar()->update();
	
	int dpi = 2400;
	if(sscanf(str.ascii(), "%d", &dpi) != 1)
		m_options.dpi = 2400;
	else
		m_options.dpi = dpi;
	
	if(m_pProcThread) {
		m_curCommand = m_pProcThread->setOptions(m_options);
		refreshTimer.start(250);
	} else m_curCommand = PROTH_NOTHING;
	
	
	saveOptions();
}

void TamanoirApp::on_trustCheckBox_toggled(bool on) {
	statusBar()->showMessage(tr("Changed to 'trust' mode : " + (on?tr("ON"):tr("OFF"))));
	
	m_options.trust = on;
	
	if(m_pProcThread) {
		m_curCommand = m_pProcThread->setOptions(m_options);
		refreshTimer.start(250);
	} else m_curCommand = PROTH_NOTHING;
	
	saveOptions();
}

void TamanoirApp::on_hotPixelsCheckBox_toggled(bool on) {
	statusBar()->showMessage( tr("Changed hot pixels filter: please wait...") );
	statusBar()->update();
	m_options.hotPixels = on;
	
	if(m_pProcThread) {
		m_curCommand = m_pProcThread->setOptions(m_options);
		refreshTimer.start(250);
	} else m_curCommand = PROTH_NOTHING;
	
	saveOptions();
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
		IplImage * displayImage = m_pImgProc->getDisplayImage();
		if(displayImage) {
			
			
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
		
		IplImage * curImage;
		
		// Update cropped buffers
		if(m_pProcThread) {
			m_pImgProc->cropCorrectionImages(m_pProcThread->getCorrection());
		}
		
		// Top-left : Display cropped / detailled images
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
		
		// Bottom-left : Proposed correction
		curImage = m_pImgProc->getCorrectedCrop();
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
		
		
		// Mask image = dust in white on black background
		curImage = m_pImgProc->getMask();
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
		
		
		
		// Top-right : Display dust info
		CvConnectedComp dust = m_pImgProc->getDustComp();
		
		char strinfo[32];
		float width_mm = 25.4f * dust.rect.width / m_options.dpi;
		float height_mm = 25.4f * dust.rect.height / m_options.dpi;
		
		sprintf(strinfo, "%dx%d-%d pix/%.1gx%.1g mm",
			(int)dust.rect.width, (int)dust.rect.height, (int)dust.area,
			width_mm, height_mm);
		QString str = tr("Dust: ") + QString(strinfo);
		ui.growTextLabel->setText(str);
		
		// Bottom-right : Display diff image (neighbouring)
		curImage = m_pImgProc->getDiffCrop();
		if(g_debug_correlation)
		{
			curImage = getCorrelationImage();
		}
		
		if(curImage) {
			QLabel * pLabel = ui.diffPixmapLabel;

			// Display in frame
			QImage grayQImage = iplImageToQImage(curImage);
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





TamanoirThread::TamanoirThread(TamanoirImgProc * p_pImgProc) {
	m_pImgProc = p_pImgProc;
	
	current_command = PROTH_NOTHING;
	
	m_run = m_running = false;
}

int TamanoirThread::setOptions(tm_options options) {
	m_options = options;
	if(!m_pImgProc) 
		return 0;
	if(!m_run)
		start();
	
	int ret = current_command = PROTH_OPTIONS;
	
	// Unlock thread 
	mutex.lock();
	waitCond.wakeAll();
	mutex.unlock();
	
	return ret;
}


int TamanoirThread::loadFile(QString s) {
	m_filename = s;
	
	if(!m_run)
		start();
	next_dust_retval = -1;
	
	memset(&next_dust, 0, sizeof(t_correction));
	memset(&current_dust, 0, sizeof(t_correction));
	
	int ret = current_command = PROTH_LOAD_FILE;
	QFileInfo fi(s);
	if(!fi.exists()) return -1;
	
	// Unlock thread 
	mutex.lock();
	waitCond.wakeAll();
	mutex.unlock();
	
	return ret;
}

TamanoirThread::~TamanoirThread() {
	m_run = false;
	while(m_running) {
		fprintf(stderr, "TmThread::%s:%d : waiting for thread to stop\n", 
				__func__, __LINE__);
		sleep(1);
	}
}

int TamanoirThread::saveFile(QString s) {
	m_filename = s;
	
	if(!m_run)
		start();
	
	int ret = current_command = PROTH_SAVE_FILE;
	mutex.lock();
	waitCond.wakeAll();
	mutex.unlock();
	// Unlock thread 
	return ret;
}

/* Get last detected dust correction */
t_correction TamanoirThread::getCorrection() {
	return current_dust;
}

/* Get pointer to last detected dust correction */
t_correction * TamanoirThread::getCorrectionPointer()  {
	return &current_dust;
}


int TamanoirThread::nextDust() {
	if(!m_pImgProc) 
		return -1;
	
	current_dust = next_dust;
	int ret = next_dust_retval;
	if(ret == 0) // No more values
		return ret;
	
	ret = current_command = PROTH_SEARCH;
	mutex.lock();
	waitCond.wakeAll();
	mutex.unlock();
	return ret;
}

int TamanoirThread::getProgress() { 
	if(current_command == PROTH_NOTHING) return 100;
	if(!m_pImgProc) return 100;
	
	return m_pImgProc->getProgress(); 
}

void TamanoirThread::run() {
	m_running = true;
	m_run = true;
	
	while(m_run) {
		mutex.lock();
		waitCond.wait(&mutex);
		fprintf(stderr, "TmThread::%s:%d : run\n", __func__, __LINE__);
		int ret;
		
		switch(current_command) {
		default:
		case PROTH_NOTHING:
			fprintf(stderr, "TmThread::%s:%d : do NOTHING ???\n", __func__, __LINE__);
			sleep(1);
			break;
		case PROTH_LOAD_FILE:
			fprintf(stderr, "TmThread::%s:%d : load file '%s'\n", 
				__func__, __LINE__, m_filename.ascii());
			m_pImgProc->loadFile(m_filename);
			next_dust_retval = m_pImgProc->firstDust();
			next_dust = m_pImgProc->getCorrection();
			break;
		case PROTH_SAVE_FILE:
			fprintf(stderr, "TmThread::%s:%d : save file '%s'\n", 
				__func__, __LINE__, m_filename.ascii());
			m_pImgProc->saveFile(m_filename);
			break;
		case PROTH_SEARCH:
			fprintf(stderr, "TmThread::%s:%d : searching for next dust\n", 
				__func__, __LINE__);
			
			next_dust_retval = m_pImgProc->nextDust();
			next_dust = m_pImgProc->getCorrection();
			fprintf(stderr, "TmThread::%s:%d : => next dust ret=%d\n", 
				__func__, __LINE__, next_dust_retval);
			
			break;
		case PROTH_OPTIONS:
			fprintf(stderr, "TmThread::%s:%d : process options changes\n", 
				__func__, __LINE__);
			m_pImgProc->setFilmType(m_options.filmType);
			m_pImgProc->setResolution(m_options.dpi);
			m_pImgProc->setTrustCorrection(m_options.trust);
			m_pImgProc->setHotPixelsFilter(m_options.hotPixels);
			
			next_dust_retval = m_pImgProc->nextDust();
			next_dust = m_pImgProc->getCorrection();
			
			break;
		}
		
		current_command = PROTH_NOTHING;
		mutex.unlock();
	}
	
	m_running = false;
}
