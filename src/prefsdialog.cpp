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

void PrefsDialog::on_styleComboBox_activated(const QString & str) {

	strcpy(g_display_options.stylesheet, str.ascii());

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

