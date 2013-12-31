/***************************************************************************
 *            prefsdialog.h
 *
 *  Thu Jun 18 23:47:41 2009
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

#ifndef PREFSDIALOG_H
#define PREFSDIALOG_H

#include "ui_prefsdialog.h"

class PrefsDialog : public QDialog, private Ui::PrefsDialog
{
	Q_OBJECT

public:
	PrefsDialog(QWidget *parent = 0);

private slots:
	void on_styleComboBox_activated(const QString & str);
	void on_hideAutoCheckBox_toggled(bool on);
	void on_exportLayerCheckBox_toggled(bool on);

	// Debug options
	void on_imgverbose_checkBox_toggled(bool checked);
	void on_imgoutput_checkBox_toggled(bool checked);
	void on_saveintmp_checkBox_toggled(bool checked);
	void on_correlation_checkBox_toggled(bool checked);
	void on_dataset_mode_checkBox_toggled(bool checked);
	void on_evaluate_mode_checkBox_toggled(bool checked);
	void on_displaylabel_checkBox_toggled(bool checked);
	void on_dust_seek_checkBox_toggled(bool checked);
	void on_dust_list_checkBox_toggled(bool checked);

private:
	void updateDebugOptionsToWidget();
	void updateDebugOptionsFromWidget();
};


#endif // PREFSDIALOG_H
