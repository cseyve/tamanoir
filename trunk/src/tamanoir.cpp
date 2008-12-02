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
extern u8 g_evaluate_mode;

u8 g_debug_TmThread = 0; 


/** constructor */
TamanoirApp::TamanoirApp(QWidget * l_parent) 
	: QMainWindow(l_parent)
{
	fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	statusBar()->showMessage( QString("") );
	
	m_pImgProc = NULL;
	m_pProcThread = NULL;
	force_mode = false;
	cropPixmapLabel_last_button = Qt::NoButton;
	is_src_selected = true;

	QString homeDirStr = QString("/home/");
	if(getenv("HOME"))
		homeDirStr = QString(getenv("HOME"));
	strcpy(m_options.currentDir, homeDirStr.ascii());
	
	ui.setupUi((QMainWindow *)this);
	
	// Load last options
	loadOptions();
	
	connect(&refreshTimer, SIGNAL(timeout()), this, SLOT(on_refreshTimer_timeout()));

	ui.prevButton->setEnabled(TRUE);
	ui.loadingTextLabel->setText(QString(""));
	ui.linearButton->setToggleButton(true);
	m_draw_on = m_resize_rect = false;
	
	m_main_display_rect = ui.mainPixmapLabel->rect();
	
#ifdef SIMPLE_VIEW
	ui.diffPixmapLabel->hide();
	ui.growPixmapLabel->hide();
	ui.hotPixelsCheckBox->hide();
	ui.autoButton->hide();
	ui.overAllProgressBar->hide();
	ui.loadingTextLabel->hide();

	on_loadButton_clicked ();
	
	resize(ui.centralwidget->size().width(), ui.centralwidget->height());
	
#endif
}


/** destructor */
TamanoirApp::~TamanoirApp() 
{
	purge();
}


void TamanoirApp::purge() {
	if(m_pProcThread) {
		delete m_pProcThread;
		m_pProcThread = NULL;
	}
	if(m_pImgProc) {
		delete m_pImgProc; 
		m_pImgProc = NULL;
	}
}



void TamanoirApp::resizeEvent(QResizeEvent * e) {
	// Resize components
	if(!e) return;
	int groupBoxWidth = e->size().width()/2 - ui.cropGroupBox->pos().x() - 10 * 3;
	int groupBoxHeight = e->size().height()/2 - 10 * 3 -  180;
	
	fprintf(stderr, "TamanoirApp::%s:%d : resize %dx%d => mainPixmapLabel=%d,%d+%dx%d groupbox : %dx%d\n",
		__func__, __LINE__,
		e->size().width(), e->size().height(),
		ui.mainPixmapLabel->pos().y(), ui.mainPixmapLabel->pos().y(), ui.mainPixmapLabel->size().width(), ui.mainPixmapLabel->size().height(), 
		groupBoxWidth, groupBoxHeight);

	ui.correctPixmapLabel->resize( ui.cropPixmapLabel->size().width(), ui.cropPixmapLabel->size().height() );
	
	// Then force update of crops
	updateDisplay();
	
	/*

	ui.cropGroupBox->resize( groupBoxWidth, groupBoxHeight);
	ui.dustGroupBox->resize( groupBoxWidth, groupBoxHeight);
	*/
}




void TamanoirApp::on_refreshTimer_timeout() {
	if(m_pProcThread) {
		if(refreshTimer.isActive() && 
			m_pProcThread->getCommand() == PROTH_NOTHING) {
			// Stop timer
			refreshTimer.stop();
			
			lockTools(false);
		}
		
		if(m_curCommand == PROTH_NOTHING && 
			m_pProcThread->getCommand() != PROTH_NOTHING)
			m_curCommand = m_pProcThread->getCommand();
		
		if(g_debug_TmThread)
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
			case PROTH_LOAD_FILE:
				refreshMainDisplay();
				updateDisplay();
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
			fprintf(stderr, "TamanoirApp::%s:%d : LOADING FINISHED ! m_curCommand=%d m_pProcThread=%d\n", __func__, __LINE__,
				m_curCommand, m_pProcThread->getCommand());
			if(m_curCommand == PROTH_LOAD_FILE) {
				on_skipButton_clicked();
			}
			
//			refreshMainDisplay();
			// Update little frame displays
//			updateDisplay();
		}
	}
}

void TamanoirApp::on_mainPixmapLabel_signalMouseMoveEvent(QMouseEvent * e) {
	on_mainPixmapLabel_signalMousePressEvent(e);
}

void TamanoirApp::on_mainPixmapLabel_signalMousePressEvent(QMouseEvent * e) {
	
	
	if(e && m_pImgProc) {
		// Keep current dust in mind to get back to it when done
		if(m_pProcThread && !force_mode)
			m_pProcThread->insertCorrection(current_dust);
		
		// Indicate we are forcing the position of correction, and so
		// we must not store in skipped dusts list
		force_mode = true;
		
		IplImage * origImage = m_pImgProc->getGrayscale();
		if(!origImage)
			return;
		IplImage * displayImage = m_pImgProc->getDisplayImage();
		
		IplImage * cropImage = m_pImgProc->getCrop();
		if(!cropImage)
			return;
		
		//int scaled_width = m_main_display_rect.width()-12;
		//int scaled_height = m_main_display_rect.height()-12;
		int scaled_width = ui.mainPixmapLabel->width()-2;
		int scaled_height = ui.mainPixmapLabel->height()-2;
		
		
		
		//float scale = (float)tmmax( displayImage->width, displayImage->height )
		//	/ (float)tmmax(scaled_width, scaled_height);
		float scale = (float)tmmax( displayImage->width, displayImage->height )
			/ (float)tmmax(scaled_width, scaled_height);
		
		float scale_x = (float)origImage->width / (float)scaled_width;
		float scale_y = (float)origImage->height / (float)scaled_height;
		
		fprintf(stderr, "TamanoirApp::%s:%d : e=%d,%d x scale=%g\n", __func__, __LINE__,
				e->pos().x(), e->pos().y(), scale);
		
		// Create a fake dust in middle
		memset(&current_dust, 0, sizeof(t_correction));
		current_dust.crop_x = (int)roundf(e->pos().x() * scale_x) - (cropImage->width+1) / 2;
		current_dust.crop_y = (int)roundf(e->pos().y() * scale_y) - (cropImage->height +1)/ 2;
		current_dust.rel_src_x = current_dust.rel_dest_x = cropImage->width / 2;
		current_dust.rel_src_y = current_dust.rel_dest_y = cropImage->height / 2;
		current_dust.rel_dest_y += 20;
		current_dust.copy_width = current_dust.copy_height = 16;
		current_dust.area = 1;
		
		m_pImgProc->setCopySrc(&current_dust,
			cropImage->width / 2, cropImage->height / 2);
			
		updateDisplay();
	}
}


void TamanoirApp::on_linearButton_toggled(bool state) {
	
	
	fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	if(m_draw_on != state) {
		m_draw_on = state;
		
		if(m_draw_on) {
			ui.cropPixmapLabel->setCursor( Qt::CrossCursor );
		} else {
			ui.cropPixmapLabel->setCursor( Qt::ArrowCursor );
		}
	}
}


void TamanoirApp::on_cropPixmapLabel_signalMouseReleaseEvent(QMouseEvent * ) {
	
	//fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	cropPixmapLabel_last_button = Qt::NoButton;
}

void TamanoirApp::on_cropPixmapLabel_signalMousePressEvent(QMouseEvent * e) {
	
	//fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	
	
	if(e && m_pProcThread && m_pImgProc) {
		
		//int e_pos_x = (int)( e->pos().x() * ui.cropPixmapLabel->size().width() / cropSize.width );
		
		switch(e->button()) {
		case Qt::NoButton:
			//fprintf(stderr, "TamanoirApp::%s:%d : NoButton...\n", __func__, __LINE__);
			cropPixmapLabel_last_button = Qt::NoButton;
			ui.cropPixmapLabel->setCursor( Qt::ArrowCursor );
			m_resize_rect = false;
			return;
			break;
		case Qt::LeftButton: {
			if(!m_draw_on) {
				// Check if the click is near the border of the rectangle
				int dx = tmmin(abs(current_dust.rel_src_x+current_dust.copy_width - e->pos().x()),
						abs(current_dust.rel_src_x - e->pos().x()));
				int dy = tmmin(abs(current_dust.rel_src_y+current_dust.copy_height - e->pos().y()),
						abs(current_dust.rel_src_y - e->pos().y()));
				if(dx<= 5 && dy <= 5) {
					cropPixmapLabel_last_button = Qt::RightButton; // e->button();
					ui.cropPixmapLabel->setCursor( Qt::SizeFDiagCursor);
					m_resize_rect = true;
					return;
				} else {
					m_resize_rect = false;
				}
			}
			// First check if click is closer to src or dest
			int dx_src = abs(e->pos().x() - (current_dust.rel_src_x + (current_dust.copy_width+1)/2));
			int dy_src = abs(e->pos().y() - (current_dust.rel_src_y + (current_dust.copy_height+1)/2));
			float dist_src = sqrt((float)(dx_src*dx_src + dy_src*dy_src ));
			
			int dx_dest = abs(e->pos().x() - (current_dust.rel_dest_x + (current_dust.copy_width+1)/2));
			int dy_dest = abs(e->pos().y() - (current_dust.rel_dest_y + (current_dust.copy_height+1)/2));
			float dist_dest = sqrt((float)(dx_dest*dx_dest + dy_dest*dy_dest ));
			if(tmmin(dist_src, dist_dest) < 50) {
				if(dist_src < dist_dest) {
					// Move src
					is_src_selected = true;
				} else {
					// Move dest
					is_src_selected = false;
				}
				
				if(!m_draw_on) {
					ui.cropPixmapLabel->setCursor( Qt::ClosedHandCursor );
				}
			}
			
			if(m_draw_on) {
				// Move dest
				is_src_selected = false;
			}
			
			
			cropPixmapLabel_last_button = e->button();
			if(is_src_selected) {
				// Move src
				current_dust.rel_src_x = e->pos().x() - (current_dust.copy_width+1)/2;
				current_dust.rel_src_y = e->pos().y() - (current_dust.copy_height+1)/2;
			} else {
				// Move dest
				current_dust.rel_dest_x = e->pos().x() - (current_dust.copy_width+1)/2;
				current_dust.rel_dest_y = e->pos().y() - (current_dust.copy_height+1)/2;
				if(m_draw_on) {
					current_dust.rel_seed_x = e->pos().x();
					current_dust.rel_seed_y = e->pos().y();
					
					/** No search when we click = click=apply 
					t_correction search_correct = current_dust;
					
					// Compute correction
					u8 old_debug = g_debug_imgverbose, old_correlation = g_debug_correlation;
					//g_debug_imgverbose = 255;
					//g_debug_correlation = 255;
					int ret = m_pImgProc->findDust(current_dust.crop_x+current_dust.rel_seed_x , 
										current_dust.crop_y+current_dust.rel_seed_y , 
											&search_correct);
					g_debug_imgverbose = old_debug;
					g_debug_correlation = old_correlation;
				
					if(ret > 0) {
						fprintf(stderr, "TamanoirApp::%s:%d : Seed = %d, %d => ret=%d\n", __func__, __LINE__,
							current_dust.crop_x+current_dust.rel_seed_x , 
							current_dust.crop_y+current_dust.rel_seed_y , 
							ret);
						current_dust = search_correct;
						
					}
					** No search when we click = click=apply **/
					m_pImgProc->applyCorrection(current_dust, true);
				}
			}
			
			int center_x = current_dust.rel_src_x + (current_dust.copy_width+1)/2;
			int center_y = current_dust.rel_src_y + (current_dust.copy_height+1)/2;
			m_pImgProc->setCopySrc(&current_dust,
				center_x, center_y);
			updateDisplay();
			}
			break;
		case Qt::RightButton: { // Right is for moving border
			cropPixmapLabel_last_button = Qt::NoButton;
			if(m_draw_on) {	// Move src
				current_dust.rel_src_x = e->pos().x() - (current_dust.copy_width+1)/2;
				current_dust.rel_src_y = e->pos().y() - (current_dust.copy_height+1)/2;
			} else {
				// Check if the click is near the rectangle
				int dx = tmmin(abs(current_dust.rel_src_x+current_dust.copy_width - e->pos().x()),
					   abs(current_dust.rel_src_x - e->pos().x()));
				int dy = tmmin(abs(current_dust.rel_src_y+current_dust.copy_height - e->pos().y()),
					   abs(current_dust.rel_src_y - e->pos().y()));
				if(dx<= 5 && dy <= 5) {
					cropPixmapLabel_last_button = e->button();
					ui.cropPixmapLabel->setCursor( Qt::SizeFDiagCursor);
				}
			}
			} break;
		default:
			//fprintf(stderr, "TamanoirApp::%s:%d : Button = %d...\n", __func__, __LINE__,
			//	(int)e->button());
			
			break;
		}
	}
}


void TamanoirApp::on_cropPixmapLabel_signalMouseMoveEvent(QMouseEvent * e) {
	
	if(e && m_pProcThread && m_pImgProc) {
		
		//if(e->button() == Qt::NoButton) return;
		
		switch(cropPixmapLabel_last_button) {
		default:
		case Qt::NoButton: {
			if(!m_draw_on) {
				int dx = tmmin(abs(current_dust.rel_src_x+current_dust.copy_width - e->pos().x()),
					   abs(current_dust.rel_src_x - e->pos().x()));
				int dy = tmmin(abs(current_dust.rel_src_y+current_dust.copy_height - e->pos().y()),
					   abs(current_dust.rel_src_y - e->pos().y()));
				if(dx<= 5 && dy <= 5) {
					ui.cropPixmapLabel->setCursor( Qt::SizeFDiagCursor);
					return;
				}
				
				// First check if click is closer to src or dest
				int dx_src = abs(e->pos().x() - (current_dust.rel_src_x + (current_dust.copy_width+1)/2));
				int	dy_src = abs(e->pos().y() - (current_dust.rel_src_y + (current_dust.copy_height+1)/2));
				float dist_src = sqrt((float)(dx_src*dx_src + dy_src*dy_src ));
			
				int dx_dest = abs(e->pos().x() - (current_dust.rel_dest_x + (current_dust.copy_width+1)/2));
				int dy_dest = abs(e->pos().y() - (current_dust.rel_dest_y + (current_dust.copy_height+1)/2));
				float dist_dest = sqrt((float)(dx_dest*dx_dest + dy_dest*dy_dest ));
				if(tmmin(dist_src, dist_dest) < tmmin(current_dust.copy_width, current_dust.copy_height)/2) {
					
					ui.cropPixmapLabel->setCursor( Qt::OpenHandCursor);
				}
				else {
					ui.cropPixmapLabel->setCursor( Qt::ArrowCursor);
				}
				
				
			} else {
				current_dust.rel_dest_x = e->pos().x() - (current_dust.copy_width+1)/2;
				current_dust.rel_dest_y = e->pos().y() - (current_dust.copy_height+1)/2;
				current_dust.rel_seed_x = e->pos().x();
				current_dust.rel_seed_y = e->pos().y();
				
				// Compute correction
				t_correction search_correct = current_dust;
				u8 old_debug = g_debug_imgverbose, old_correlation = g_debug_correlation;
				//g_debug_imgverbose = 255;
				//g_debug_correlation = 255;
				int ret = m_pImgProc->findDust(current_dust.crop_x+current_dust.rel_seed_x , 
										current_dust.crop_y+current_dust.rel_seed_y , 
											&search_correct);
				g_debug_imgverbose = old_debug;
				g_debug_correlation = old_correlation;
				
				if(ret > 0) {
					fprintf(stderr, "TamanoirApp::%s:%d : Seed = %d, %d => ret=%d\n", __func__, __LINE__,
							current_dust.crop_x+current_dust.rel_seed_x , 
							current_dust.crop_y+current_dust.rel_seed_y , 
							ret);
					current_dust = search_correct;
						
					//m_pImgProc->applyCorrection(search_correct, true);
				}
			}
			int center_x = current_dust.rel_src_x + (current_dust.copy_width+1)/2;
			int center_y = current_dust.rel_src_y + (current_dust.copy_height+1)/2;
			m_pImgProc->setCopySrc(&current_dust,
				center_x, center_y);
			updateDisplay();
			}break;
		case Qt::LeftButton:
			{
			if(is_src_selected) {
				// Move src
				current_dust.rel_src_x = e->pos().x() - (current_dust.copy_width+1)/2;
				current_dust.rel_src_y = e->pos().y() - (current_dust.copy_height+1)/2;
			} else {
				// Move dest
				current_dust.rel_dest_x = e->pos().x() - (current_dust.copy_width+1)/2;
				current_dust.rel_dest_y = e->pos().y() - (current_dust.copy_height+1)/2;
			}
			
			if(m_draw_on) {
				// Get move from last position 
				int dx = e->pos().x() - current_dust.rel_seed_x;
				int dy = e->pos().y() - current_dust.rel_seed_y;
			
				current_dust.rel_seed_x = e->pos().x();
				current_dust.rel_seed_y = e->pos().y();
				
				// Move src too
				current_dust.rel_src_x += dx;
				current_dust.rel_src_y += dy;
				
				
				/* No search when moving with the button down 
				// Compute correction
				t_correction search_correct = current_dust;
					u8 old_debug = g_debug_imgverbose, old_correlation = g_debug_correlation;
					//g_debug_imgverbose = 255;
					//g_debug_correlation = 255;
					int ret = m_pImgProc->findDust(current_dust.crop_x+current_dust.rel_seed_x , 
										current_dust.crop_y+current_dust.rel_seed_y , 
											&search_correct);
					g_debug_imgverbose = old_debug;
					g_debug_correlation = old_correlation;
				
					if(ret > 0) {
						fprintf(stderr, "TamanoirApp::%s:%d : Seed = %d, %d => ret=%d\n", __func__, __LINE__,
							current_dust.crop_x+current_dust.rel_seed_x , 
							current_dust.crop_y+current_dust.rel_seed_y , 
							ret);
						current_dust = search_correct;
						
						
					}
				* No search when moving with the button down */
				m_pImgProc->applyCorrection(current_dust, true);
			}

			int center_x = current_dust.rel_src_x + (current_dust.copy_width+1)/2;
			int center_y = current_dust.rel_src_y + (current_dust.copy_height+1)/2;
			m_pImgProc->setCopySrc(&current_dust,
				center_x, center_y);
			updateDisplay();
			}break;
		case Qt::RightButton: { // Resize rectangle
			int center_x = current_dust.rel_src_x + (current_dust.copy_width+1)/2;
			int center_y = current_dust.rel_src_y + (current_dust.copy_height+1)/2;
			
			
			int dest_x = current_dust.rel_dest_x + (current_dust.copy_width+1)/2;
			int dest_y = current_dust.rel_dest_y + (current_dust.copy_height+1)/2;
			
			current_dust.copy_width = 2*abs(center_x - e->pos().x());
			current_dust.copy_height = 2*abs(center_y - e->pos().y());
			
			current_dust.rel_dest_x = dest_x -  (current_dust.copy_width+1)/2;
			current_dust.rel_dest_y = dest_y -  (current_dust.copy_height+1)/2;
			
			
			m_pImgProc->setCopySrc(&current_dust,
				center_x, center_y);
			updateDisplay();
			}break;
		}
	}
}

void TamanoirApp::on_cropPixmapLabel_signalWheelEvent(QWheelEvent * e) {
	if(e && m_pProcThread) {
		t_correction * l_correction = &current_dust;
		int numDegrees = e->delta() / 8;
		int numSteps = numDegrees / 15;
		
		int inc = -numSteps * 2;

		int center_x = l_correction->rel_src_x + (l_correction->copy_width+1)/2;
		int center_y = l_correction->rel_src_y + (l_correction->copy_height+1)/2;
		
		l_correction->rel_dest_x -= inc/2; 
		l_correction->rel_dest_y -= inc/2;
		
		if(inc < 0) {
			if(l_correction->copy_width<=2 || l_correction->copy_height<=2)
				return;
		}
		l_correction->copy_width  += inc;
		l_correction->copy_height += inc;
		
		// This function only update l_correction
		m_pImgProc->setCopySrc(l_correction,
			center_x,
			center_y);
		
		updateDisplay();
	}
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



void TamanoirApp::loadFile(QString s) {
	QFileInfo fi(s);
	if(!fi.exists()) 
		return;
	
	
	m_currentFile = s;
	
	strcpy(m_options.currentDir, fi.absolutePath().ascii());
	saveOptions();
	
	// Clear known dusts list
	memset(&current_dust, 0, sizeof(t_correction));
	skipped_list.clear();
	
	
	statusBar()->showMessage( tr("Loading and pre-processing ") + m_currentFile + QString("..."));
	statusBar()->update();
	
	fprintf(stderr, "TamanoirApp::%s:%d : file='%s'...\n", 
		__func__, __LINE__, s.latin1());
	// Open file
	if(!m_pImgProc) {
		m_pImgProc = new TamanoirImgProc(ui.cropPixmapLabel->width() -2, ui.cropPixmapLabel->height() -2);
		
		refreshMainDisplay();
		
		m_pImgProc->setHotPixelsFilter(m_options.hotPixels);
		m_pImgProc->setTrustCorrection(m_options.trust);
		m_pImgProc->setResolution(m_options.dpi);
		m_pImgProc->setFilmType(m_options.filmType);
	}
	if(!m_pProcThread) {
		m_pProcThread = new TamanoirThread(m_pImgProc);
	}
	
	int ret = m_pProcThread->loadFile( s );
	if(ret < 0) {
		
		QMessageBox::critical( 0, tr("Tamanoir"),
			tr("Cannot load file ") + s + tr(". Format or compression is not compatible"));
		
		return;
	}
	
	// Lock tool frame
	lockTools(true);
	
	m_curCommand = m_pProcThread->getCommand();
	refreshTimer.start(500);
}


void TamanoirApp::lockTools(bool lock) {
	ui.toolFrame->setDisabled(lock);
	//ui.loadButton->setEnabled(lock);
	ui.saveButton->setDisabled(lock);
}

/****************************** Button slots ******************************/
void TamanoirApp::on_loadButton_clicked() 	
{
	fprintf(stderr, "TamanoirApp::%s:%d : ...\n", __func__, __LINE__);
	QString s = QFileDialog::getOpenFileName(this,
                   	tr("Open file dialog"),
                   	m_options.currentDir,
                    tr("Images (*.png *.p*m *.xpm *.jp* *.tif* *.bmp"
								"*.PNG *.P*M *.XPM *.JP* *.TIF* *.BMP)"));
	if(s.isEmpty()) {
		//fprintf(stderr, "TamanoirApp::%s:%d : cancelled...\n", __func__, __LINE__);
		return;
	}
	
	ui.loadingTextLabel->setText(tr("Loading..."));
	
	loadFile( s);
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
		QString msg = tr("Saving ") + m_currentFile;
		
		lockTools(true);
		
		// Save a copy 
		QDir dir( fi.dirPath(TRUE) );
		dir.rename(m_currentFile, copystr);
		msg+= tr(" + backup as ") + copystr;
		
		// Save image
		m_curCommand = m_pProcThread->saveFile( m_currentFile );
		
		
		statusBar()->showMessage( msg );
		
		// Store statistics
		dust_stats_t stats = m_pImgProc->getDustStats();
		processAndPrintStats(&stats);
		
		// Display in progress bar
		m_curCommand = m_pProcThread->getCommand();
		refreshTimer.start(500);
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

void TamanoirApp::on_prevButton_clicked() {
	if(skipped_list.isEmpty())
		return;
	if(!force_mode && m_pProcThread)
		m_pProcThread->insertCorrection(current_dust);
		
	current_dust = skipped_list.takeLast();
	fprintf(stderr, "[TmApp]::%s:%d : back for one dust\n", __func__, __LINE__);
	
	if(skipped_list.isEmpty()) {
		ui.prevButton->setEnabled(FALSE);
	}
	updateDisplay();
}
void TamanoirApp::on_rewindButton_clicked() {
	
	int ret = QMessageBox::warning(this, tr("Tamanoir"),
			tr("Rewind to first dust will make the application ask you "
			"another time to refuse the dusts you have already seen.\n"
			"Do you want to rewind to first dust?"),
			QMessageBox::Apply, 
			QMessageBox::Cancel);

	if(ret == QMessageBox::Cancel)
		return;
	
	
	memset(&current_dust, 0, sizeof(t_correction));
	if(m_pImgProc)
		m_pImgProc->firstDust();
	if(m_pProcThread) {
		m_pProcThread->firstDust();
	}
	
	ui.overAllProgressBar->setValue(0);
	statusBar()->showMessage( tr("Rewind to first dust."));
}

void TamanoirApp::on_skipButton_clicked()
{
	
	if(m_pProcThread) {
		// Mark skip on image
		if(m_pImgProc)
			m_pImgProc->skipCorrection(current_dust);
		if(skipped_list.isEmpty())
			ui.prevButton->setEnabled(TRUE);
		
		if(!force_mode)
			skipped_list.append(current_dust);
		
		// First check if a new dust if available
		current_dust = m_pProcThread->getCorrection();
		
		if(current_dust.copy_width <= 0) // No dust available, wait for a new one
		{
			int state = m_pProcThread->getCommand();
			
			int ret = m_curCommand = m_pProcThread->nextDust();
			
			if(ret == 0) // Finished
			{
				ui.overAllProgressBar->setValue(100);
				statusBar()->showMessage(tr("Finished"));
				
				updateDisplay(); // To show corrections
				
				/* DISPLAY PERFORMANCES */
				if(g_evaluate_mode) {
					QString msg, s;
					msg = tr("NONE");
					
					t_perf_stats stats = m_pImgProc->getPerfs();
					int sum = stats.true_positive+stats.no_proposal
						+ stats.false_positive + stats.false_negative;
					
					if(sum>0) {
						msg += tr("After background processing => ");
						
						msg += tr("True positive:");
						s.sprintf("%d / %d = %g %%\n", stats.true_positive, sum, 
								  100.f * (float)stats.true_positive / sum);
						msg += s;

						msg += tr("No proposal:");
						s.sprintf("%d / %d = %g %%\n", stats.no_proposal, sum, 
								  100.f * (float)stats.no_proposal / sum);
						msg += s;
						
						msg += tr("False positive:");
						s.sprintf("%d / %d = %g %%\n", stats.false_positive, sum, 
								  100.f * (float)stats.false_positive / sum);
						msg += s;
						
						msg += tr("False negative:");
						s.sprintf("%d / %d = %g %%\n", stats.false_negative, sum, 
								  100.f * (float)stats.false_negative / sum);
						msg += s;
						
						
						QMessageBox::information( 0, tr("Performances"),
											 tr(" ") + msg + QString("."));
					}	
				}
				
				return;
			}
			
			
			if(state == PROTH_NOTHING) // Search was done
			{
				m_curCommand = PROTH_NOTHING;
				
				updateDisplay();
			} else {
				refreshTimer.start(250);
				lockTools(true);
			}
		} else 
			updateDisplay();
	}
	
	force_mode = false;
}



void TamanoirApp::on_correctButton_clicked()
{
	// Apply previous correction
	if(m_pImgProc) {
		m_pImgProc->forceCorrection(current_dust, force_mode);
	}
	
	// Clear current dust
	memset(&current_dust, 0, sizeof(t_correction));
	
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
	if(m_pProcThread) {
		
		
		m_pProcThread->setModeAuto(true);
		
	}
	fflush(logfile);
	
        
	ui.overAllProgressBar->setValue(100);
	
	// Update little frame displays
	updateDisplay();
        
        
	statusBar()->showMessage( tr("Auto-correct : done.") );
	
	fprintf(stderr, "TamanoirApp::%s:%d : AUTO MODE FINISHED\n", __func__, __LINE__);
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
	if(!iplImage)
		return QImage();
	

	int depth = iplImage->nChannels;
	
	bool rgb24_to_bgr32 = false;
	if(depth == 3  ) {// RGB24 is obsolete on Qt => use 32bit instead
		depth = 4;
		rgb24_to_bgr32 = true;
	}
	
	int orig_width = iplImage->width;
	if((orig_width % 2) == 1)
		orig_width--;
	QImage qImage(orig_width, iplImage->height, 8*depth);
	memset(qImage.bits(), 0, orig_width*iplImage->height*depth);
	
	switch(iplImage->depth) {
	default:
		fprintf(stderr, "[TamanoirApp]::%s:%d : Unsupported depth = %d\n", __func__, __LINE__, iplImage->depth);
		break;
	
	case IPL_DEPTH_8U: {
		if(!rgb24_to_bgr32) {
			for(int r=0; r<iplImage->height; r++)
				memcpy(qImage.bits() + r*orig_width, 
					iplImage->imageData + r*iplImage->widthStep, orig_width*depth);
		}
		else {
			// RGB24 to BGR32
			u8 * buffer3 = (u8 *)iplImage->imageData;
			u8 * buffer4 = (u8 *)qImage.bits();
			int orig_width4 = 4 * orig_width;
			
			for(int r=0; r<iplImage->height; r++)
			{
				int pos3 = r * iplImage->widthStep;
				int pos4 = r * orig_width4;
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
		}break;
	case IPL_DEPTH_16S: {
		if(!rgb24_to_bgr32) {
			
			u8 * buffer4 = (u8 *)qImage.bits();
			short valmax = 0;
			
			for(int r=0; r<iplImage->height; r++)
			{
				short * buffershort = (short *)(iplImage->imageData + r*iplImage->widthStep);
				for(int c=0; c<iplImage->width; c++)
					if(buffershort[c]>valmax)
						valmax = buffershort[c];
			}
			
			if(valmax>0)
				for(int r=0; r<iplImage->height; r++)
				{
					short * buffer3 = (short *)(iplImage->imageData 
									+ r * iplImage->widthStep);
					int pos3 = 0;
					int pos4 = r * orig_width;
					for(int c=0; c<orig_width; c++, pos3++, pos4++)
					{
						int val = abs((int)buffer3[pos3]) * 255 / valmax;
						if(val > 255) val = 255;
						buffer4[pos4] = (u8)val;
					}
				}
		}
		else {
			u8 * buffer4 = (u8 *)qImage.bits();
			if(depth == 3) {
				
				for(int r=0; r<iplImage->height; r++)
				{
					short * buffer3 = (short *)(iplImage->imageData + r * iplImage->widthStep);
					int pos3 = 0;
					int pos4 = r * orig_width*4;
					for(int c=0; c<orig_width; c++, pos3+=3, pos4+=4)
					{
						buffer4[pos4   ] = buffer3[pos3];
						buffer4[pos4 + 1] = buffer3[pos3+1];
						buffer4[pos4 + 2] = buffer3[pos3+2];
					}
				}
			} else if(depth == 1) {
				short valmax = 0;
				short * buffershort = (short *)(iplImage->imageData);
				for(int pos=0; pos< iplImage->widthStep*iplImage->height; pos++)
					if(buffershort[pos]>valmax)
						valmax = buffershort[pos];
				
				if(valmax>0)
					for(int r=0; r<iplImage->height; r++)
					{
						short * buffer3 = (short *)(iplImage->imageData 
											+ r * iplImage->widthStep);
						int pos3 = 0;
						int pos4 = r * orig_width;
						for(int c=0; c<orig_width; c++, pos3++, pos4++)
						{
							int val = abs((int)buffer3[pos3]) * 255 / valmax;
							if(val > 255) val = 255;
							buffer4[pos4] = (u8)val;
						}
					}
			}
		}
		}break;
	case IPL_DEPTH_16U: {
		if(!rgb24_to_bgr32) {
			
			u8 * buffer4 = (u8 *)qImage.bits();
			unsigned short valmax = 0;
			
			for(int r=0; r<iplImage->height; r++)
			{
				unsigned short * buffershort = (unsigned short *)(iplImage->imageData + r*iplImage->widthStep);
				for(int c=0; c<iplImage->width; c++)
					if(buffershort[c]>valmax)
						valmax = buffershort[c];
			}
			
			if(valmax>0)
				for(int r=0; r<iplImage->height; r++)
				{
					unsigned short * buffer3 = (unsigned short *)(iplImage->imageData 
									+ r * iplImage->widthStep);
					int pos3 = 0;
					int pos4 = r * orig_width;
					for(int c=0; c<orig_width; c++, pos3++, pos4++)
					{
						int val = abs((int)buffer3[pos3]) * 255 / valmax;
						if(val > 255) val = 255;
						buffer4[pos4] = (u8)val;
					}
				}
		}
		else {
			fprintf(stderr, "[TamanoirApp]::%s:%d : U16  depth = %d -> BGR32\n", __func__, __LINE__, iplImage->depth);
			u8 * buffer4 = (u8 *)qImage.bits();
			if(depth == 3) {
				
				for(int r=0; r<iplImage->height; r++)
				{
					short * buffer3 = (short *)(iplImage->imageData + r * iplImage->widthStep);
					int pos3 = 0;
					int pos4 = r * orig_width*4;
					for(int c=0; c<orig_width; c++, pos3+=3, pos4+=4)
					{
						buffer4[pos4   ] = buffer3[pos3]/256;
						buffer4[pos4 + 1] = buffer3[pos3+1]/256;
						buffer4[pos4 + 2] = buffer3[pos3+2]/256;
					}
				}
			} else if(depth == 1) {
				short valmax = 0;
				short * buffershort = (short *)(iplImage->imageData);
				for(int pos=0; pos< iplImage->widthStep*iplImage->height; pos++)
					if(buffershort[pos]>valmax)
						valmax = buffershort[pos];
				
				if(valmax>0)
					for(int r=0; r<iplImage->height; r++)
					{
						short * buffer3 = (short *)(iplImage->imageData 
											+ r * iplImage->widthStep);
						int pos3 = 0;
						int pos4 = r * orig_width;
						for(int c=0; c<orig_width; c++, pos3++, pos4++)
						{
							int val = abs((int)buffer3[pos3]) * 255 / valmax;
							if(val > 255) val = 255;
							buffer4[pos4] = (u8)val;
						}
					}
			}
		}
		}break;
	}
	
	if(iplImage->nChannels == 1) {
		qImage.setNumColors(256);
		for(int c=0; c<256; c++) 
			qImage.setColor(c, qRgb(c,c,c));
	}
	return qImage;
}


void TamanoirApp::refreshMainDisplay() {
	if(!m_pImgProc) {
		return;
	}
	
	int scaled_width = m_main_display_rect.width()-2;
	int scaled_height = m_main_display_rect.height()-2;
	
	m_pImgProc->setDisplaySize(scaled_width, scaled_height);
}




void TamanoirApp::updateDisplay()
{
	if(m_pImgProc) {
		// After pre-processing, we can get the grayscale version of input image
		IplImage * displayImage = m_pImgProc->getDisplayImage();
		
		
		if(!displayImage) {
			refreshMainDisplay ();
			displayImage = m_pImgProc->getDisplayImage();
		}
		
		if(displayImage) {
			
			// Display in main frame
			int gray_width = displayImage->width;
			int scaled_width = displayImage->width;
			int scaled_height = displayImage->height;
			QImage grayQImage(gray_width, displayImage->height, 8*displayImage->nChannels);
			if(displayImage->nChannels == 1) {
				memcpy(grayQImage.bits(), displayImage->imageData, displayImage->widthStep * displayImage->height);
				grayQImage.setNumColors(256);
				for(int c=0; c<256; c++) 
					grayQImage.setColor(c, qRgb(c,c,c));
			
				grayQImage.setColor(COLORMARK_CORRECTED, qRgb(0,255,0));
				grayQImage.setColor(COLORMARK_REFUSED, qRgb(255,255,0));
				grayQImage.setColor(COLORMARK_FAILED, qRgb(255,0,0));
				grayQImage.setColor(COLORMARK_CURRENT, qRgb(0,0,255));
			}
			else
				grayQImage = iplImageToQImage(displayImage);
			
			QImage ratioImage;
			ratioImage = grayQImage.scaled( 
				ui.mainPixmapLabel->size().width(), ui.mainPixmapLabel->size().height() , 
				Qt::KeepAspectRatio, Qt::SmoothTransformation);
			
			QPixmap pixmap;
			
			
			fprintf(stderr, "TamanoirApp::%s:%d : => grayscaled display : %dx%d\n", 
					__func__, __LINE__, 
					ui.mainPixmapLabel->size().width(), ui.mainPixmapLabel->size().height());
			
			
			pixmap.convertFromImage( ratioImage ); //, QImage::ScaleMin);
			/*
			fprintf(stderr, "TamanoirApp::%s:%d : orginal rectangle : %d,%d+%dx%d\n", 
									__func__, __LINE__,
									m_main_display_rect.x(), m_main_display_rect.y(),
									m_main_display_rect.width(),m_main_display_rect.height() );
			fprintf(stderr, "TamanoirApp::%s:%d : pixmap=%dx%d => Scaled=%dx%d\n", __func__, __LINE__, 
									pixmap.width(), pixmap.height(),
									scaled_width, scaled_height);
			*/
			
			//ui.mainPixmapLabel->setFixedSize(scaled_width,scaled_height);
			/*
			mainPixmapLabel->setGeometry(largViewFrame->x() + (largViewFrame->width()-scaled_width)/2,
																	 largViewFrame->y() + (largViewFrame->height()-scaled_height)/2,
																	 scaled_width, scaled_height);
			*/
			/*
			ui.mainPixmapLabel->setGeometry(
								ui.largViewFrame->x() +	10 + m_main_display_rect.x() + (m_main_display_rect.width()-scaled_width+1)/2,
								ui.largViewFrame->y() +	20 + m_main_display_rect.y() + (m_main_display_rect.height()-scaled_height+1)/2,
										scaled_width+2, scaled_height+2);
			*/
			ui.mainPixmapLabel->setPixmap(pixmap);
		}
		
		IplImage * curImage;
		
		// Update cropped buffers
		if(m_pProcThread) {
			current_dust.crop_width = ui.cropPixmapLabel->size().width()-2;
			current_dust.crop_height = ui.cropPixmapLabel->size().height()-2;
		
			m_pImgProc->cropCorrectionImages(current_dust);
		}
		
		// Top-left : Display cropped / detailled images
		curImage = m_pImgProc->getCrop();
		if(curImage) {
			QLabel * pLabel = ui.cropPixmapLabel;
			
			// Display in frame
			QImage grayQImage = iplImageToQImage(curImage).scaledToWidth(pLabel->width());
			if(curImage->nChannels == 1) {
				grayQImage.setNumColors(256);
				for(int c=0; c<255; c++) 
					grayQImage.setColor(c, qRgb(c,c,c));
				
				grayQImage.setColor(255, qRgb(0,255,0));
			}
			QPixmap pixmap;
			pixmap.convertFromImage( 
				grayQImage,
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
			QImage grayQImage = iplImageToQImage(curImage).scaledToWidth(pLabel->width());
			if(curImage->nChannels == 1) {
				grayQImage.setNumColors(256);
				for(int c=0; c<255; c++) 
					grayQImage.setColor(c, qRgb(c,c,c));
				
				grayQImage.setColor(255, qRgb(255,0,0));
			}
			
			QPixmap pixmap;
			pixmap.convertFromImage( 
					grayQImage,
					QPixmap::Color);
			pLabel->setPixmap(pixmap);
			pLabel->repaint();
		}
		
		
		
		// Top-right : Display dust info
		
		char strinfo[32];
		float width_mm = current_dust.width_mm;
		float height_mm = current_dust.width_mm;
		
		sprintf(strinfo, "%d pix/%.1gx%.1g mm",
			current_dust.area,
			width_mm, height_mm);
		QString str = tr("Dust: ") + QString(strinfo);
		ui.dustGroupBox->setTitle(str);
		
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
	
	req_command = current_command = PROTH_NOTHING;
	no_more_dusts = false;
	m_modeAuto = m_run = m_running = false;
	
	start();
}

int TamanoirThread::getCommand() {
	if(req_command)
		return req_command;
	
	return current_command;
}

/* set auto mode flag */ 
void TamanoirThread::setModeAuto(bool on) {
	m_modeAuto = on;
	if(on) {
		if(!m_pImgProc) 
			return;
		if(!m_run)
			start();
		
		req_command = PROTH_SEARCH;
		m_options.trust = 1;
		m_pImgProc->setTrustCorrection(m_options.trust);
		// Unlock thread 
		mutex.lock();
		waitCond.wakeAll();
		mutex.unlock();
	}
}

int TamanoirThread::setOptions(tm_options options) {
	m_options = options;
	if(!m_pImgProc) 
		return 0;
	if(!m_run)
		start();
	
	int ret = req_command = PROTH_OPTIONS;
	
	// Unlock thread 
	mutex.lock();
	waitCond.wakeAll();
	mutex.unlock();
	
	return ret;
}


int TamanoirThread::loadFile(QString s) {
	m_filename = s;
	no_more_dusts = false;
	if(!m_run) {
		start();
		// Wait for thread to start
		while(!m_run) usleep(200000);
	}
	
	// Clear dust list
	dust_list.clear();
	
	
	QFileInfo fi(s);
	if(!fi.exists()) {
		fprintf(stderr, "TmThread::%s:%d : file '%s' does not exists\n", __func__, __LINE__, s.ascii());
		return -1;
	}
	
	int ret = req_command = PROTH_LOAD_FILE;
	fprintf(stderr, "TmThread::%s:%d : request load file '%s' \n", __func__, __LINE__, s.ascii());
		
	mutex.lock();
	// Unlock thread 
	waitCond.wakeAll();
	mutex.unlock();
	
	return ret;
}

TamanoirThread::~TamanoirThread() {
	m_run = false;
	while(m_running) {
		mutex.lock();
		req_command = PROTH_NOTHING;
		waitCond.wakeAll();
		mutex.unlock();
	
		fprintf(stderr, "TmThread::%s:%d : waiting for thread to stop\n", 
				__func__, __LINE__);
		if(m_running)
			sleep(1);
	}
}

int TamanoirThread::saveFile(QString s) {
	m_filename = s;
	
	if(!m_run)
		start();
	
	mutex.lock();
	int ret = req_command = PROTH_SAVE_FILE;
	waitCond.wakeAll();
	mutex.unlock();
	
	// Unlock thread 
	return ret;
}

/* Get last detected dust correction */
t_correction TamanoirThread::getCorrection() {
	t_correction current_dust;
	if(dust_list.isEmpty())
		memset(&current_dust, 0, sizeof(t_correction));
	else
		current_dust = dust_list.takeFirst();
	
	if(g_debug_TmThread)
	fprintf(stderr, "TMThread::%s:%d : dust=%d,%d +%dx%d\n", 
		__func__, __LINE__, 
		current_dust.crop_x + current_dust.rel_dest_x,
		current_dust.crop_y + current_dust.rel_dest_y,
		current_dust.copy_width, current_dust.copy_height);
	
	return current_dust;
}

void TamanoirThread::insertCorrection(t_correction current_dust) {
	if(current_dust.copy_width <= 0)
		return;
	
	if(g_debug_TmThread)
		fprintf(stderr, "TMThread::%s:%d : insert dust=%d,%d +%dx%d\n", 
			__func__, __LINE__, 
			current_dust.crop_x + current_dust.rel_dest_x,
			current_dust.crop_y + current_dust.rel_dest_y,
			current_dust.copy_width, current_dust.copy_height);
	
	dust_list.prepend(current_dust);
	
}

int TamanoirThread::firstDust() {
	if(!dust_list.isEmpty())
		dust_list.clear();
	if(!m_pImgProc) 
		return -1;
	
	m_pImgProc->firstDust();
	
	mutex.lock();
	req_command = PROTH_SEARCH;
	waitCond.wakeAll();
	mutex.unlock();
	
	return 1;
}

int TamanoirThread::nextDust() {
	if(!m_pImgProc) 
		return -1;
	
	int ret = 1;
	
	if(dust_list.isEmpty() && no_more_dusts)
		ret = 0;
	
	mutex.lock();
	req_command = PROTH_SEARCH;
	waitCond.wakeAll();
	mutex.unlock();
	
	return ret;
}

int TamanoirThread::getProgress() { 
	if(current_command == PROTH_NOTHING) return 100;
	if(!m_pImgProc) return 100;
	
	return m_pImgProc->getProgress(); 
}


/*
 * Background processing thread 
 */
void TamanoirThread::run() {
	m_running = true;
	m_run = true;
	
	no_more_dusts = false;
	while(m_run) {
		mutex.lock();
		waitCond.wait(&mutex, 20);
		mutex.unlock();
		
		if(req_command != PROTH_NOTHING && g_debug_TmThread)
			fprintf(stderr, "TmThread::%s:%d : run command = %d\n", __func__, __LINE__, req_command);
		
		current_command = req_command;
		req_command = PROTH_NOTHING;
		
		int ret;
		
		switch(current_command) {
		default:
		case PROTH_NOTHING:
			//fprintf(stderr, "TmThread::%s:%d : do NOTHING ???\n", __func__, __LINE__);
			if(!no_more_dusts) {
				req_command = PROTH_SEARCH;
				if(g_debug_TmThread) 
					fprintf(stderr, "nothing\t=>\tTmThread::%s:%d : => PROTH_SEARCH !!!\n", __func__, __LINE__);
			}
			
			break;
		case PROTH_SAVE_FILE:
			fprintf(stderr, "TmThread::%s:%d : save file '%s'\n", 
				__func__, __LINE__, m_filename.ascii());
			m_pImgProc->saveFile(m_filename);
			break;
		case PROTH_LOAD_FILE:
			fprintf(stderr, "TmThread::%s:%d : load file '%s'\n", 
				__func__, __LINE__, m_filename.ascii());
			
			ret = m_pImgProc->loadFile(m_filename);
			no_more_dusts = false;
			fprintf(stderr, "TmThread::%s:%d : file '%s' LOADED => search for first dust\n", 
				__func__, __LINE__, m_filename.ascii());
			if(ret >= 0)
				req_command = PROTH_SEARCH;
			break;
			
		case PROTH_SEARCH:
			if(g_debug_TmThread) {
				fprintf(stderr, "TmThread::%s:%d : searching for next dust (while main frame is displaying)\n", 
					__func__, __LINE__);
			}
			ret = m_pImgProc->nextDust();
			if(ret > 0) {
				// Add to list
				t_correction l_dust = m_pImgProc->getCorrection();
				no_more_dusts = false;
				if(!m_modeAuto) {
					dust_list.append(l_dust);
				} else {
					fprintf(stderr, "TmThread::%s:%d : mode AUTO : apply correction\n", 
						__func__, __LINE__);
					m_pImgProc->applyCorrection(l_dust);
					// Fasten next search
					req_command = PROTH_SEARCH;
				}
				
			} else {
				if(ret == 0) {
					no_more_dusts = true;
					
					fprintf(stderr, "TmThread::%s:%d : no more dust (ret=%d)\n", 
						__func__, __LINE__, ret);
				}
			}
			if(g_debug_TmThread)
				fprintf(stderr, "TmThread::%s:%d : => next dust ret=%d\n", 
					__func__, __LINE__, ret);
			
			break;
		case PROTH_OPTIONS:
			fprintf(stderr, "TmThread::%s:%d : process options changes\n", 
				__func__, __LINE__);
			
			// Clear dust list
			dust_list.clear();
			
			m_pImgProc->setFilmType(m_options.filmType);
			m_pImgProc->setResolution(m_options.dpi);
			m_pImgProc->setTrustCorrection(m_options.trust);
			m_pImgProc->setHotPixelsFilter(m_options.hotPixels);
			
			no_more_dusts = false;
			
			// Search for next dust 
			req_command = PROTH_SEARCH;
			
			break;
		}
		
		current_command = PROTH_NOTHING;
	}
	
	m_running = false;
}
