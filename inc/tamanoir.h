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
 	bool m_trust;
	bool m_hotPixels;
	int m_filmType;
        int m_dpi;
        
        /** Refresh the main display */
        void refreshMainDisplay();
        
 	/** Update all displays */
 	void updateDisplay();
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
	
private:
	TamanoirImgProc * m_pImgProc;
	QString m_currentDir;
	QString m_currentFile;
};

#endif
