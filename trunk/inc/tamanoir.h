/***************************************************************************
 *            tamanoir.h
 *
 *  Wed Jun 11 08:47:41 2008
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef TAMANOIR_H
#define TAMANOIR_H

#include "imgutils.h"
#include "imgproc.h"

#ifdef SIMPLE_VIEW
#include "ui_tamanoir_simple.h"
#else
#include "ui_tamanoir.h"
#endif

#include <QtGui>
#include <QFileDialog>
#include <QProgressDialog>


// Tamanoir processing thread commands

#define PROTH_NOTHING		0	/*! Nothing to do */
#define PROTH_LOAD_FILE		1	/*! Loading file in memory */
#define PROTH_PREPROC		2	/*! Pre-processing */
#define PROTH_SEARCH		3	/*! Search for next dust */
#define PROTH_OPTIONS		4	/*! Options changes */
#define PROTH_SAVE_FILE		5	/*! Save file */

const char g_command_names[][10] = {
								"NOTHING",
								"LOAD_FILE",
								"PREPROC",
								"SEARCH",
								"OPTIONS",
								"SAVE_FILE"
							};
/** @brief Get command name by its int value */
const char * getCommandName(int cmd);


/** @brief Tamanoir image processing settings/options */
typedef struct {
	char currentDir[512];	/*! Working directory */
	// optional display
	bool hide_auto;			/*! Hide auto button */
	bool show_hotpixels;	/*! Show hot pixels button */
	bool export_layer;		/*! Propose to export dust mask as layer when saving image */

	bool hide_wizard;		/*! Hide wizard on startup */

	// Display options
	char stylesheet[512];	/*! Stylesheet */
} tm_display_options;

/** @brief Print display options to a text file or std output/error */
void fprintfDisplayOptions(FILE * f, tm_display_options * p_options);


/** @brief Tamanoir processing thread
*/
class TamanoirThread : public QThread
{
	Q_OBJECT
public:
	TamanoirThread(TamanoirImgProc * p_pImgProc);

	~TamanoirThread();

	virtual void run();

	int loadFile(QString s);
	int saveFile(QString s);
	int setOptions(tm_options options);

	/** @brief Stop processing thread */
	void stop() { m_run = false; };

	/** @brief Returns processing state, PROTH_NOTHING if done */
	int getCommand();

	/** @brief Returns processing progress value (0 to 100), 100 if done */
	int getProgress();

	/** @brief Launch preprocessing */
	int runPreProcessing();

	/** @brief Search for next dust */
	int firstDust();

	/** @brief Search for next dust */
	int nextDust();

	/** @brief Get last detected dust correction */
	t_correction getCorrection();

	/** @brief Insert a dust correction as first dust (the following to be read)

	Insert a correction as first item of dust_list, to save the dust cancelled
	when we click on "previous" button
	*/
	void insertCorrection(t_correction);

	/** @brief set auto mode flag */
	void setModeAuto(bool on);

	/** @brief get auto mode flag */
	bool getModeAuto() { return m_options.mode_auto ; };

private:

	/** @brief Current running command */
	int m_current_command;
	/** @brief Next running/requested command */
	int m_req_command;

	/// Current file
	QString m_filename;

	/// Run flag (command)
	bool m_run;
	/// Run flag (status)
	bool m_running;

	QMutex mutex;
	QWaitCondition waitCond;

	/** Application options */
	tm_options m_options;

	/** @brief image processing module */
	TamanoirImgProc * m_pImgProc;

	/** List of proposed corrections */
	QList<t_correction> m_dust_list;

	bool m_no_more_dusts;
};


/** @brief Tamanoir main application / user interface

*/
class TamanoirApp : public QMainWindow
{
	Q_OBJECT

public:
	/** construtor */
	TamanoirApp(QWidget *l_parent = NULL);
	/** destructor */
	~TamanoirApp();

	void setArgs(int argc, char **argv);
	/** @brief Load image file */
	int loadFile(QString s);
	/** Save current options for next launch of Tamanoir */
	void saveOptions();

private:

	Ui::Tamanoir ui;

	QFileDialog * m_fileDialog;

	/** @brief Delete allocated structures */
	void purge();

protected:
	//virtual void showEvent( QShowEvent * );
	virtual void resizeEvent ( QResizeEvent * );


private:


	QString optionsFile;

	/** Load last saved options */
	int loadOptions();



	/** Refresh the main display */
	void refreshMainDisplay();

	/** Update all displays */
	void updateDisplay();
	/** Update global view */
	void updateMainDisplay();
	/** Update cropped images views */
	void updateCroppedDisplay();
	/** Update cropped cursor */
	void updateCroppedCursor();

	QTimer refreshTimer;
	int m_curCommand;

	/** Dust src/dest deplacement tool button state */
	bool is_src_selected;
	Qt::MouseButton cropPixmapLabel_last_button;

	CvSize originalMainPixmapLabelSize;

	bool m_unsaved_changes;
	bool m_overCorrected;
signals:

private slots:
	//void slotOpenFile();
	void on_loadButton_clicked();
	void on_saveButton_clicked();


	void on_actionFull_screen_activated();
	void on_actionAbout_activated();
	void on_actionTutorial_activated();
	void on_actionShortcuts_activated();
	void on_actionPreferences_activated();

	void on_actionOpen_2_activated();
	void on_actionSave_activated();
	void on_actionQuit_activated();


	void on_autoButton_clicked();
	void on_skipButton_clicked();
	void on_prevButton_clicked();
	void on_rewindButton_clicked();
	void on_correctButton_clicked();

	void on_cloneButton_toggled(bool state);
	void on_inpaintButton_toggled(bool state);
	void on_searchCloneSrcCheckBox_toggled(bool state);
	void on_dustInfoButton_toggled(bool state);

	// Global image navigation button
	void on_topLeftButton_clicked();
	void on_pageDownButton_clicked();
	void on_pageUpButton_clicked();
	void on_undoButton_clicked();
	void on_markButton_clicked();

	void on_fullScreenButton_clicked();


	void on_mainPixmapLabel_signalMousePressEvent(QMouseEvent *);
	void on_mainPixmapLabel_signalMouseMoveEvent(QMouseEvent *);

	void on_cropPixmapLabel_customContextMenuRequested(QPoint p);
	void on_cropPixmapLabel_signalMousePressEvent(QMouseEvent *);
	void on_cropPixmapLabel_signalMouseReleaseEvent(QMouseEvent *);
	void on_cropPixmapLabel_signalMouseMoveEvent(QMouseEvent *);
	void on_cropPixmapLabel_signalWheelEvent(QWheelEvent *);

	void on_correctPixmapLabel_signalFocusInEvent(QFocusEvent *);
	void on_correctPixmapLabel_signalFocusOutEvent(QFocusEvent *);
	void on_correctPixmapLabel_signalMouseMoveEvent(QMouseEvent *);

	void on_typeComboBox_currentIndexChanged(int);
	void on_dpiComboBox_currentIndexChanged(QString );
	void on_sensitivityComboBox_currentIndexChanged(int);

	/// Display sensitivity in main display when moving the slider
	void on_sensitivityHorizontalSlider_valueChanged(int);
	/// Display sensitivity in main display when moving the slider
	void on_sensitivityHorizontalSlider_sliderReleased();

	void on_hotPixelsCheckBox_toggled(bool);
	void on_trustCheckBox_toggled(bool);
	void on_emptyCheckBox_toggled(bool);

	/// Periodic refresh when background thread is processing
	void on_refreshTimer_timeout();

private:
	/** @brief lock tools while doing big tasks : loading/saving/settings */
	void lockTools(bool lock);


	TamanoirImgProc * m_pImgProc;
	TamanoirThread * m_pProcThread;
	QList<t_correction> skipped_list;
	bool force_mode;

	QProgressDialog * m_pProgressDialog;

	/** @brief Correction mode
	  0: no forced correction, 1: clone, 2: inpaint
	  */
	int m_draw_on;
	bool m_searchCloneSrc;

	bool m_resize_rect;
	float m_resize_rect_xscale;
	float m_resize_rect_yscale;

	/// Currently used dust
	t_correction m_current_dust;

	QSize m_main_display_rect;
	QString m_currentFile;

	CvSize m_blockSize;
	// Page up/down navigation
	int m_nav_x_block;
	int m_nav_y_block;
	void moveBlock();
};

/** @brief Convert an OpenCV IplImage to a Qt QImage */
QImage iplImageToQImage(IplImage * iplImage, bool false_colors = false, bool red_only = true);

#ifdef TAMANOIRAPP_CPP
TamanoirApp * tmApp = NULL;
/** Global application options */
tm_options g_options;
tm_display_options g_display_options;
#else
extern TamanoirApp * tmApp;
extern tm_options g_options;
extern tm_display_options g_display_options;
#endif

#endif
