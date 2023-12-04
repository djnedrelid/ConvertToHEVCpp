#pragma once

#include <windows.h>
#include <WinUser.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <thread>
#include <shellapi.h>	// CommandLineToArgvW(), ShellExecuteW(), tray meny.
#include <iostream>
#include <fstream>
#include <sstream>	
#include <Shlwapi.h>	// PathIsDirectoryW() - trenger også shlwapi.lib
#include <algorithm>	// std::transform - bruker sammen med std::towlower
#include <cwctype>	// std::towlower
#include <regex>
#include <mutex>	// std::mutex - For å hindre race condition ved bruk av flere tråder. global lock/unlock.
#include <Richedit.h>
#include <CommCtrl.h>	// TOOLTIPS_CLASS

#include "resource.h"
#include "MiscGlobalVars.h"		// Egne diverse globale variabler.
#include "djDBI.h"			// Egen databaseklasse for SQLITE3.
#include "MiscStaticFuncsClass.h"	// Egne diverse funksjoner.
#include "ProcessPipeClass.h"		// Egen klasse for prosesshåndtering.
#include "HistoricStatClass.h"		// Klasse for håndtering av historisk datasparing. Antall bytes overstiger ULL.

#include "ConverterClass.h"		// Egen klasse for å prate med prosesshåndterer.
					// Starter denne i egen tråd fra hovedprogram slik:

					// ConvertHEVC *CHEVC = new ConvertHEVC();
					// std::thread _start (&ConvertHEVC::Start, CHEVC, &mediafiler, Skalering, Kvalitet);
					// _start.detach();

					// Tråden snakker da med hovedprogram via globale variabler og mutex låsing.