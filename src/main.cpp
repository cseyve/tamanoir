/***************************************************************************
 *            main.cpp
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

#include <QApplication>

#include "tamanoir.h"

/*
Linux : 
/usr/bin/qmake-qt4 -unix -o Makefile Tamanoir.pro

MacOS X :
/Developer/Tools/Qt/qmake -spec /usr/local/Qt4.3/mkspecs/darwin-g++  -macx Tamanoir.pro
OR :
/Developer/Tools/Qt/qmake -macx Tamanoir.pro
*/


// Main GUI display
bool maximized = false, full = false;

extern u8 g_debug_imgverbose;
extern u8 g_debug_imgoutput;
extern u8 g_debug_correlation;
extern u8 g_dataset_mode;
extern u8 g_evaluate_mode;


void displayHelp() {
	fprintf(stderr, "Usage : ./tamanoir [options]\n"
					"Options:\n"
					"\t--output : insert marks on output image\n"
					"\t--correlation : display correlation between the current dust and its neiboughood (lots of printf !)\n"
					"\t--debug : all debug flags on\n"
					"\t--full : display at full screen (default)\n"
					"\t--maximized : display without full screen and at maximal screen size (with window)\n"
					"\t--minimized : display without full screen and at normal size (not maximized)\n"
					);
}

int main(int argc, char *argv[])
{
	if(argc>1) {
		int argcount = 1;
		while(argcount<argc) {
			fprintf(stderr, "%s:%d argv[%d] = '%s'\n", __func__, __LINE__, argcount, argv[argcount]);
			
			if(strstr(argv[argcount], "help"))	{
				displayHelp();
				return 0;
			}
			if(strstr(argv[argcount], "maximize")
				|| strstr(argv[argcount], "max"))	{
				maximized = true;
			}
			if(strstr(argv[argcount], "minimize"))	{
				full = false;
				maximized = false;
			}
			if(strstr(argv[argcount], "full"))	{
				full = true;
			}
			if(strstr(argv[argcount], "debug")) {
				g_debug_imgverbose =
					g_debug_imgoutput = 2;
			}
			if(strstr(argv[argcount], "output")) {
				g_debug_imgoutput = 2;
			}
			if(strstr(argv[argcount], "dataset")) {
				g_dataset_mode = 1;
			} 
			
			if(strstr(argv[argcount], "eval")) {
				g_evaluate_mode = 1;
			
				// Force turning of dataset creation mode
				g_dataset_mode = 0;
			}
			
			
			if(strstr(argv[argcount], "correl"))	{
				g_debug_correlation = 1;
			}
			
			argcount++;
		}
	}
		
	
	
	QApplication a(argc, argv);
	//a.setFont(QFont("helvetica", 12));
  
  /*
	QTranslator tor( 0 );
  	// set the location where your .qm files are in load() below as the last parameter instead of "."
  	// for development, use "/" to use the english original as
  	// .qm files are stored in the base project directory.
  	tor.load( QString("tamanoir_") + QTextCodec::locale() + QString(".qm"), 
		"." );

 	a.installTranslator( &tor );
*/
	/* --- Create application ---*/
	TamanoirApp sA;
	sA.setArgs(argc, argv);
	sA.show();
	
	return a.exec();
}
