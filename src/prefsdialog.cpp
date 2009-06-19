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
}

void PrefsDialog::on_styleComboBox_activated(const QString & str) {
	QString filename=":/qss/tamanoir-" + str + ".qss";

	strcpy(g_display_options.stylesheet, filename.ascii());

	tmApp->saveOptions();
}
