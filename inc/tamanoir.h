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

#include "ui_tamanoir.h"
#include <QtGui>

/** @brief Tamanoir settings/options */
typedef struct {
	char currentDir[512];
	bool trust;		/*! Trust mode activated */
	bool hotPixels;	/*! Hot pixels detection activated */
	int filmType;	/*! Film type */
	int dpi;		/*! Scan resolution in dot per inch */
} tm_options;

void fprintfOptions(FILE * f, tm_options * p_options);


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

	/** Stop processing thread */
	void stop() { m_run = false; };
	
	/** @brief Returns processing state, PROTH_NOTHING if done */
	int getCommand() { return current_command; };
	
	/** @brief Returns processing progress value (0 to 100), 100 if done */
	int getProgress();
	
	/** @brief Search for next dust */
	int nextDust();
	
	/** @brief Get last detected dust correction */
	t_correction getCorrection();
	
	/** @brief Get pointer to last detected dust correction */
	t_correction * getCorrectionPointer();
	
private:
#define PROTH_NOTHING	0
#define PROTH_LOAD_FILE	1
#define PROTH_SAVE_FILE	2
#define PROTH_SEARCH	3
#define PROTH_OPTIONS	4
	
	/** @brief Current running command */
	int current_command;
	/** @brief Next running command */
	int req_command;
	
	QString m_filename;

	bool m_run;	
	bool m_running;	
	QMutex mutex;
	QWaitCondition waitCond;
	
	tm_options m_options;
	
	int next_dust_retval;
	t_correction current_dust;
	t_correction next_dust;
	
	/** @brief image processing module */
	TamanoirImgProc * m_pImgProc;
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
	void loadFile(QString s);
private:
	Ui::Tamanoir ui;

protected:
	//virtual void showEvent( QShowEvent * );
	virtual void resizeEvent ( QResizeEvent * );

private slots:
	void on_cropPixmapLabel_signalMousePressEvent(QMouseEvent *);

private:
	/** Application options */
	tm_options m_options;

	QString optionsFile;
	
	/** Load last saved options */
	int loadOptions();	
	
	/** Save current options for next launch of Tamanoir */
	void saveOptions();

	/** Refresh the main display */
	void refreshMainDisplay();
	
	/** Update all displays */
	void updateDisplay();

	QTimer refreshTimer;
	int m_curCommand;

signals:
	
private slots:
	//void slotOpenFile();	
	void on_loadButton_clicked();
	void on_saveButton_clicked();	
	
	void on_autoButton_clicked();
	void on_skipButton_clicked();
	void on_correctButton_clicked();

	void on_cropPixmapLabel_customContextMenuRequested(QPoint p);

	void on_typeComboBox_currentIndexChanged(int);
	void on_dpiComboBox_currentIndexChanged(QString );

	void on_hotPixelsCheckBox_toggled(bool);
	void on_trustCheckBox_toggled(bool);
	
	void on_refreshTimer_timeout();

private:
	TamanoirImgProc * m_pImgProc;
	TamanoirThread * m_pProcThread;
	
	QString m_currentFile;
};

/** @brief Convert an OpenCV IplImage to a Qt QImage */
QImage iplImageToQImage(IplImage * iplImage);


#endif
