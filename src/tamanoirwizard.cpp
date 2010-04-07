/***************************************************************************
 *            tamanoirwizard.cpp
 *
 *  Tue 9 22:47:41 2010
 *  Copyright  2010  Christophe Seyve
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



#include "tamanoirwizard.h"
#include "ui_tamanoirwizard.h"
#include "tamanoir.h"

extern tm_display_options g_display_options;

TamanoirWizard::TamanoirWizard(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::TamanoirWizard)
{
    ui->setupUi(this);

	if(g_display_options.hide_wizard) {
		ui->showCheckBox->blockSignals(true);
		ui->showCheckBox->setChecked(false);
		ui->showCheckBox->blockSignals(false);
	}
}

TamanoirWizard::~TamanoirWizard()
{
    delete ui;
}

void TamanoirWizard::on_showCheckBox_toggled(bool on)
{
	if(on) {
		g_display_options.hide_wizard = false;
	} else {
		g_display_options.hide_wizard = true;
	}
	if(tmApp) {
		tmApp->saveOptions();
	}
}


void TamanoirWizard::changeEvent(QEvent *e)
{
    QWizard::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
