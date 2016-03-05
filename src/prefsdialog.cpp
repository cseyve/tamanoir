/***************************************************************************
 *            prefsdialog.cpp
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

#include "prefsdialog.h"
#include "tamanoir.h"

PrefsDialog::PrefsDialog(QWidget * parent)
		: QDialog(parent)
{
	setupUi(this);

	// Display current options
	QString str(g_display_options.stylesheet);
	int ind = styleComboBox->findText(str, Qt::MatchContains);
	if(ind >= 0) styleComboBox->setCurrentIndex(ind);

	hideAutoCheckBox->setChecked(g_display_options.hide_auto);
}

void PrefsDialog::on_styleComboBox_activated(const QString & str)
{
    strcpy(g_display_options.stylesheet, qPrintable(str) );

	tmApp->saveOptions();
}

void PrefsDialog::on_hideAutoCheckBox_toggled(bool on)
{
	fprintf(stderr, "PrefsDialog::%s:%d : on=%d \n", __func__, __LINE__, on?'T':'F');
	g_display_options.hide_auto = on;
	tmApp->saveOptions();
}

void PrefsDialog::on_exportLayerCheckBox_toggled(bool on)
{
	fprintf(stderr, "PrefsDialog::%s:%d : on=%d\n", __func__, __LINE__, on?'T':'F');
	g_display_options.export_layer = (on ? 1 : 0);
	tmApp->saveOptions();
}

/* DEBUG OPTIONS */

extern u8 g_debug_imgverbose;
extern u8 g_debug_imgoutput;
extern u8 g_debug_correlation;
extern u8 g_dataset_mode;
extern u8 g_evaluate_mode;
extern u8 g_debug_savetmp;
extern u8 g_debug_displaylabel;
extern u8 g_debug_dust_seek;
extern u8 g_debug_list; // debug dust list insert/remove

#define SETTOWIDGET(_option,_checkbox) (_checkbox)->blockSignals(true); \
	(_checkbox)->setChecked((_option)); \
	(_checkbox)->blockSignals(false);

void PrefsDialog::updateDebugOptionsToWidget()
{
	SETTOWIDGET(g_debug_imgverbose, imgverbose_checkBox);
	SETTOWIDGET(g_debug_imgoutput, imgoutput_checkBox);
	SETTOWIDGET(g_debug_savetmp, saveintmp_checkBox);
	SETTOWIDGET(g_debug_correlation, correlation_checkBox);
	SETTOWIDGET(g_dataset_mode, dataset_mode_checkBox);
	SETTOWIDGET(g_evaluate_mode, evaluate_mode_checkBox);
	SETTOWIDGET(g_debug_displaylabel, displaylabel_checkBox);
	SETTOWIDGET(g_debug_dust_seek, dust_seek_checkBox);
	SETTOWIDGET(g_debug_list, dust_list_checkBox);

}

void PrefsDialog::updateDebugOptionsFromWidget()
{
	g_debug_imgverbose = imgverbose_checkBox->isChecked();
	g_debug_imgoutput = imgoutput_checkBox->isChecked();
	g_debug_savetmp = saveintmp_checkBox->isChecked();
	g_debug_correlation = correlation_checkBox->isChecked();
	g_dataset_mode = dataset_mode_checkBox->isChecked();
	g_evaluate_mode = evaluate_mode_checkBox->isChecked();
	g_debug_displaylabel = displaylabel_checkBox->isChecked();
	g_debug_dust_seek = dust_seek_checkBox->isChecked();
	g_debug_list = dust_list_checkBox->isChecked();
}

void PrefsDialog::on_imgverbose_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();

}

void PrefsDialog::on_imgoutput_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();

}

void PrefsDialog::on_saveintmp_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();

}

void PrefsDialog::on_correlation_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();

}

void PrefsDialog::on_dataset_mode_checkBox_toggled(bool checked)
{

	updateDebugOptionsFromWidget();
}

void PrefsDialog::on_evaluate_mode_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();

}

void PrefsDialog::on_displaylabel_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();

}

void PrefsDialog::on_dust_seek_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();

}

void PrefsDialog::on_dust_list_checkBox_toggled(bool checked)
{
	updateDebugOptionsFromWidget();
}
