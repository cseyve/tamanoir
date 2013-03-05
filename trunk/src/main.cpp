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
#include <QLocale>
#include <QLibraryInfo>

#include "tamanoir.h"

#ifdef Q_WS_MACX
#include <Carbon/Carbon.h>
#include <time.h>
#include <sys/time.h>
#endif

/*
Linux :
/usr/bin/qmake-qt4 -unix -o Makefile Tamanoir.pro

MacOS X :
/Developer/Tools/Qt/qmake -spec /usr/local/Qt4.3/mkspecs/darwin-g++  -macx Tamanoir.pro
OR :
/Developer/Tools/Qt/qmake -macx Tamanoir.pro

OR even easier : use QtCreator :)
*/


// Main GUI display
bool maximized = false, full = false;
QString g_application_path;
extern u8 g_debug_imgverbose;
extern u8 g_debug_imgoutput;
extern u8 g_debug_correlation;
extern u8 g_dataset_mode;
extern u8 g_evaluate_mode;
extern u8 g_debug_savetmp;
extern u8 g_debug_displaylabel;
extern u8 g_debug_dust_seek;
extern u8 g_debug_list; // debug dust list insert/remove

void displayHelp() {
	fprintf(stderr, "Usage : ./tamanoir [options]\n"
					"Options:\n"
					"\t--output : insert marks on output image\n"
					"\t--correlation : display correlation between the current dust and its neiboughood (lots of printf !)\n"
					"\t--debug : all debug flags on\n"
					"\t--list : debug dust list mangement\n"
					"\t--dataset : create a .data file to store corrections\n"
					"\t--eval : evaluate the accuracy of algorithms with an existing .data dataset file \n"
					"\t--full : display at full screen (default)\n"
					"\t--maximized : display without full screen and at maximal screen size (with window)\n"
					"\t--minimized : display without full screen and at normal size (not maximized)\n"
					);
}

int m_argc = 0;
char ** m_argv = NULL;
FILE * MacAPP_Log = NULL;



class MacApp : public QApplication {
public:
	MacApp(int & argc, char ** argv);

#ifdef Q_WS_MACX
 //    bool macEventFilter ( EventHandlerCallRef caller, EventRef event );
private:
	static pascal OSErr     handleOpenDocuments(
			   const AppleEvent* inEvent, AppleEvent*, long);
#endif
};

MacApp::MacApp(int & argc, char ** argv)
		: QApplication( argc, argv)
{
	m_argc = argc;
	m_argv = new char * [m_argc + 10]; // Leave one more space for "Open with"
	memset(m_argv, 0, sizeof(char *)*m_argc + 10);
	if(m_argc == 0) {

	} else {
		// Copy strings
		for(int c=0; c<m_argc; c++) {
			m_argv[c] = NULL;
			if(strlen(argv[c])>0) {
				m_argv[c]= new char [strlen(argv[c])+1];
				strcpy(m_argv[c], argv[c]);
			}
		}
	}
	if(!MacAPP_Log) MacAPP_Log = fopen("/tmp/MacAPP.log", "w");

#if defined(Q_WS_MACX) && !defined(Q_WS_MAC64)
	AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
							 NewAEEventHandlerUPP(handleOpenDocuments),
							 0, false);
#endif
}


#ifdef Q_WS_MACX
OSErr MacApp::handleOpenDocuments(const AppleEvent* inEvent,
                                  AppleEvent* /*reply*/, long /*refCon*/) {

	QString s_arg;
	AEDescList documentList;
	OSErr err = AEGetParamDesc(inEvent, keyDirectObject, typeAEList,
							   &documentList);
	if (err != noErr) {
		return err;
	}

	//              err = checkAppleEventForMissingParams(*inEvent);
	if (err == noErr) {
		long documentCount;
		err = AECountItems(&documentList, &documentCount);
		for (long documentIndex = 1;
			 err == noErr && documentIndex <= documentCount;
			 documentIndex++) {
			DescType returnedType;
			Size actualSize;
			AEKeyword keyword;
			FSRef ref;
			char qstr_buf[1024];
			err = AESizeOfNthItem(&documentList, documentIndex,
								  &returnedType, &actualSize);
			if (err == noErr) {
				err = AEGetNthPtr(&documentList, documentIndex,
								  typeFSRef, &keyword,
								  &returnedType, (Ptr)&ref,
								  sizeof(FSRef), &actualSize);
				if (err == noErr) {
					FSRefMakePath(&ref, (UInt8*)qstr_buf,
								  1024);
					s_arg=QString::fromUtf8(qstr_buf);
					if(MacAPP_Log) {
						struct timeval nowtv;
						gettimeofday(&nowtv, NULL);
						fprintf(MacAPP_Log, "%03d.%03d : new doc : '%s'\n",
								(int)(nowtv.tv_sec % 1000),  (int)(nowtv.tv_usec / 1000),
								qstr_buf);
						fflush(MacAPP_Log);
					}
					m_argv[m_argc] = new char [512];
					memset(m_argv[m_argc], 0, 512);
					strcpy(m_argv[m_argc], qstr_buf);
					m_argc++;
					if(tmApp) {
						tmApp->loadFile(s_arg);
					}
					break;
				}
			}
		} // for ...
	}
	AEDisposeDesc(&documentList);

	return 0;
}
#endif
/*

bool MacApp::macEventFilter ( EventHandlerCallRef caller, EventRef event ) {
	if(!MacAPP_log) MacAPP_log = fopen("/Users/tof/MacApp.log", "w");
  //  if(MacAPP_log)
  //QString s_arg;  fprintf(MacAPP_log, "macEventFilter(caller=%d, event=%d) \n", (int)caller, (int)event);
	return false;
}*/

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
			if(strstr(argv[argcount], "import"))	{
				g_debug_importexport = 1;
			}
			if(strstr(argv[argcount], "debug")) {
				g_debug_imgverbose = 1;
				g_debug_imgoutput = 1;
				g_debug_list = 1;
				g_debug_dust_seek = 1;
			}
			if(strstr(argv[argcount], "seek")
				|| strstr(argv[argcount], "search")) {
				g_debug_dust_seek = 1;
				//g_debug_imgoutput = 1;
			}
			if(strstr(argv[argcount], "output")) {
				g_debug_savetmp = 2;
				g_debug_imgoutput = 2;
			}
			if(strstr(argv[argcount], "dataset")) {
				g_dataset_mode = 1;
				g_evaluate_mode = 0;
			}
			if(strstr(argv[argcount], "eval")) {
				g_evaluate_mode = 1;

				// Force turning of dataset creation mode
				g_dataset_mode = 0;
			}
			if(strstr(argv[argcount], "list")) {
				g_debug_list = 1;
			}
			if(strstr(argv[argcount], "display")) {
				g_debug_displaylabel = 1;
			}

			if(strstr(argv[argcount], "correl"))	{
				g_debug_correlation = 1;
			}

			argcount++;
		}
	}



	MacApp a(argc, argv);
	QTranslator tor( 0 );

	// set the location where your .qm files are in load() below as the last parameter instead of "."
	// for development, use "/" to use the english original as
	// .qm files are stored in the base project directory.
	QLocale localLang;

#ifdef LINUX
	g_application_path = QString("/usr/share/tamanoir");
	QDir dir("/usr/share/tamanoir");
#else
	QDir dir(QApplication::applicationDirPath());
	g_application_path = QApplication::applicationDirPath();
#endif
	QString translationFile = QString("tamanoir_") +
			  localLang.languageToString(localLang.language()) +
			  QString(".qm");
	fprintf(stderr, "Translation file='%s'\n", translationFile.toAscii().data());
	tor.load( translationFile,
				dir.absolutePath() );

	a.installTranslator( &tor );


	// Load generic for buttons, ...
	QTranslator tor2;
	QString lang = localLang.languageToString(localLang.language());
	QString shortlang = lang;
	shortlang.truncate(2);
	bool loaded2 = tor2.load("qt_" + shortlang.toLower(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	if(loaded2)
	{
		a.installTranslator( &tor2 );
	}








#ifdef Q_WS_MACX

	if(MacAPP_Log) {
		struct timeval nowtv;
		gettimeofday(&nowtv, NULL);
		fprintf(MacAPP_Log, "%03d.%03d : lauching app\n",
				(int)(nowtv.tv_sec % 1000),  (int)(nowtv.tv_usec / 1000));
		fflush(MacAPP_Log);
	}
	if(MacAPP_Log) {
		struct timeval nowtv;
		gettimeofday(&nowtv, NULL);
		fprintf(MacAPP_Log, "%03d.%03d : lauching app with args=\n",
				(int)(nowtv.tv_sec % 1000),  (int)(nowtv.tv_usec / 1000));
		for(int i=0; i<m_argc; i++) fprintf(MacAPP_Log, "argv[%d]='%s' ", i, m_argv[i]);
		fflush(MacAPP_Log);
	}
#endif
	/* --- Create application ---*/
	tmApp = new TamanoirApp();
	tmApp->setArgs(m_argc, m_argv);
	tmApp->show();

	return a.exec();
}
