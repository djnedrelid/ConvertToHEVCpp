#pragma once

//
// Relaterte globale variabler for HovedLoop.
//
int FFmpegPercentage = 0;		// Prosentverdi.
std::mutex gMutexLock;			// For låsing av FFmpegPercentage.
PROCESS_INFORMATION piProcInfo = {0};	// Holder denne global for WM_DESTROY.
int HEVCExistingFilesFound = 0;		// Hopp over eksisterende HEVC.
bool HEVCFoundThisRound = false;
bool DetectAlreadyHEVCIsChecked = true;
bool DetectAlreadyHBYTIsChecked = true;

// Stier.
wchar_t ExePath[1024] = {0};
std::wstring SettingsPath, LogPath, DbPathW;
std::string DbPathA;

class ProcessPipeClass
{
	private: 
	SECURITY_ATTRIBUTES saAttr;
	static const int BUFSIZE = 4096;
	std::ofstream loggfil;
	float DurationSecs = 0.0f;
	float TimeSecs = 0.0f;
	std::regex rxDuration, rxTime, rxHEVC;

	public:
	// Konstruktør.
	ProcessPipeClass()
	{
		// Sett opp loggfil og andre globale stier.
		size_t UserProfileLen, UserProfileLenA;
		wchar_t UserProfileBuf[1024];
		char* UserProfileBufA;
		_wgetenv_s(&UserProfileLen,0,0,L"USERPROFILE");
		_dupenv_s(&UserProfileBufA,&UserProfileLenA,"USERPROFILE");
		if (UserProfileLen == 0) {
			MiscStaticFuncsClass::GetErrorW(L"Could not get %USERPROFILE%.",true);
		} else {
			_wgetenv_s(&UserProfileLen, UserProfileBuf, 1024, L"USERPROFILE"); 
			std::wstring UserProfileFolder = std::wstring(UserProfileBuf) + L"\\ConvertToHEVC++";
			std::string UserProfileFolderA = std::string(UserProfileBufA) + "\\ConvertToHEVC++";
			CreateDirectoryW(UserProfileFolder.c_str(), 0);		// Opprett brukerdata mappe.
			SettingsPath = UserProfileFolder + L"\\Settings";	// osv.
			LogPath = UserProfileFolder +L"\\ffmpeg_last_run.log";	
			DbPathW = UserProfileFolder +L"\\Statistics.sqlite3";
			DbPathA = UserProfileFolderA +"\\Statistics.sqlite3";
		}
		GetModuleFileNameW(0, ExePath, 1024);

		// Gjør piper arvbare for underprosess som standard.
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = 1;
		saAttr.lpSecurityDescriptor = 0;
		loggfil.open(LogPath, std::ios::out | std::ios::binary);

		// Forbered regex.
		rxDuration.assign("Duration: ([0-9\\.]{2,5}):([0-9\\.]{2,5}):([0-9\\.]{2,5}),");
		rxTime.assign("time=([0-9\\.]{2,5}):([0-9\\.]{2,5}):([0-9\\.]{2,5})\\s");
		rxHEVC.assign("Stream #0:[0-9]{1}[\\s\\S]{0,6}: Video: hevc \\(Main\\)");
	}

	// Ødelegger.
	~ProcessPipeClass()
	{
		loggfil.close();
	}


	// Hovedfunksjon for start av prosess.
	int CreateAndRunChildProcess(std::wstring Exefile, std::wstring Params) 
	{
		try {
		STARTUPINFOW siStartInfo = {0};
		void* g_hChildStd_OUT_Rd;
		void* g_hChildStd_OUT_Wr;
		int bSuccess = 0;
		unsigned long ExitCode = 0;
		wchar_t mExefile[1024] = {0};
		wchar_t mParams[1024] = {0};

		// Bruker {}/{0} for strukturer og matriser i stedet.
		//ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
		//ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
		
		// Opprett lesepiper som kan brukes til å lese STDOUT og STDERR fra underprosess.
		// Forhindre at underprosess ikke arver handle for STDIN write, eller STDOUT read.
		if (
			!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) ||
			!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)
		)
			MiscStaticFuncsClass::GetErrorW(
				L"Could not open STDOUT pipe to child process. Exiting.", 
				true
			);

		// Initialiser STARTINFO objektet.
		siStartInfo.cb = sizeof(STARTUPINFOW);
		siStartInfo.hStdError = g_hChildStd_OUT_Wr;
		siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
		siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
		siStartInfo.dwFlags |= STARTF_USESHOWWINDOW; // ved debugging.
		siStartInfo.wShowWindow = SW_HIDE;	     // ved debugging.

		// Konverter mottatte parametre til mutable datatyper.
		for (int n=0; n<(int)Exefile.length(); n++)
			mExefile[n] = Exefile[n];
		for (int n=0; n<(int)Params.length(); n++)
			mParams[n] = Params[n];

		// Opprett prosess.
		// Sett dwCreationFlags til CREATE_NO_WINDOW ved release,og 0 ved testing.
		if (!(bSuccess = CreateProcessW(mExefile,mParams,0,0,1,CREATE_NO_WINDOW,0,0,&siStartInfo,&piProcInfo)))
			MiscStaticFuncsClass::GetErrorW(L"Could not create child process.", true);

		// Lukk handle som vi har gitt videre og ikke lenger trengs.
		CloseHandle(g_hChildStd_OUT_Wr);

		// Prøv å les pipe mens prosessen kjører.
		// Den vil henge uten å tømmes etter 4096.
		ReadFromPipe(&g_hChildStd_OUT_Rd);

		// Nullstill tider når alt er lest.
		DurationSecs = 0.0f;
		TimeSecs = 0.0f;

		if (!HEVCFoundThisRound) {
			// Vent til prosessen er ferdig, hvis det ikke var allerede HEVC.
			WaitForSingleObject(piProcInfo.hProcess, INFINITE);
			GetExitCodeProcess(piProcInfo.hProcess, &ExitCode);
			
			// Lukk handles til prosess og tråd.
			CloseHandle(piProcInfo.hProcess);
			CloseHandle(piProcInfo.hThread);

		} else {
			// Drep ffmpeg tidlig hvis media allerede er HEVC.
			unsigned long prosess_exit_code = 0;
			GetExitCodeProcess(piProcInfo.hProcess, &prosess_exit_code);
			if (prosess_exit_code == STILL_ACTIVE) 
				TerminateProcess(piProcInfo.hProcess,0);

			// Lukk handles til prosess og tråd.
			CloseHandle(piProcInfo.hProcess);
			CloseHandle(piProcInfo.hThread);
		}

		return ExitCode;

		} catch (...) {
			MiscStaticFuncsClass::GetErrorW(
				L"Failed creating and running child process.", 
				false
			);
			return EXIT_FAILURE;
		}
	}

	//
	// Lesefunksjon for å lese STDOUT og STDERR i kombinert pipe. 
	// Bruker ReadFile() som leser bytes og bryr seg ikke om innhold.
	// Tilpass visning og/eller bruk av data etter hva som forventes.
	//
	void ReadFromPipe(void** ChildStdOut)
	{
		unsigned long dwRead;
		unsigned long long pos = 0;
		char chBuf[BUFSIZE] = {0};
		int bSuccess = 0;
		std::string tmpBuf;
		
		// Jeg trenger std::ios::app for å kunne LEGGE TIL med <<.
		std::stringstream ssBuf("", std::ios::app | std::ios::out);

		// Del opp STDERR og STDOUT i linjer og lagre alle i loggfil for feilsøking.
		while (1) {

			// Les til pipen blir lukket av prosess.
			if (!(bSuccess = ReadFile(*ChildStdOut, chBuf, BUFSIZE, &dwRead, 0))) 
				break;

			if (dwRead == 0)
				break;

			// Legg til data i ssBuf, loggfør, og nullstill chBuf for neste runde.
			ssBuf << chBuf;
			loggfil << chBuf;
			memset(chBuf, 0, BUFSIZE);
			
			// (Beholdes for referanse, vi lager nå ny per runde).
			// Slett fila hvis den er for stor (10MB).
			/*
			if (MiscStaticFuncsClass::FileSizeInBytes(logname) > 10485760) {
				
				// Ta kopi av den gamle til et eget arkiv.
				std::ifstream gammelfil(logname, std::ios::binary);
				std::ofstream backupfil(logname_archive, std::ios::binary);
				backupfil << gammelfil.rdbuf();
				
				// Slett gammel fil.
				DeleteFileA(logname);
			} */

			// Sjekk om det har kommet noen nye linjer.
			// MERK: Måtte bruke CR(\r) for å finne dem.
			// Det var det eneste som funka med tidene.
			tmpBuf = ssBuf.str();
			while ((pos = tmpBuf.find('\r')) != std::string::npos) {
				
				// Trenger kun å finne duration 1 gang.
				if (DurationSecs == 0.0f)
					FindAndSetDurationSecs(tmpBuf.substr(0, pos));
				
				// Søk etter tidspunkter.
				// Legg på std::endl på tidslinjer for lettlest logg.
				if (FindAndSetTimeSecs(tmpBuf.substr(0, pos)))
					loggfil << std::endl;

				// Sjekk om vi finner antydning til eksisterende HEVC format.
				if(IsAlreadyHEVC(tmpBuf.substr(0, pos)))
					return;

				// Fjern behandlet data for neste runde.
				tmpBuf.erase(0, pos+1);

				// Oppdater prosent hvis vi har nok data.
				if (DurationSecs > 0.0f && TimeSecs > 0.0f)
					FFmpegPercentage = (int)(((TimeSecs / DurationSecs) * 100) + 0.5);
			}

			// Lagre gjenværende data i ssBuf for neste runde.
			ssBuf.str(tmpBuf);
		}
	}

	// Hjelpefunksjon for å oppdage HEVC indikasjon i STDERR.
	bool IsAlreadyHEVC(std::string Line)
	{
		if (!DetectAlreadyHEVCIsChecked)
			return false;

		std::smatch m;
		if (std::regex_search(Line,m,rxHEVC)) {
			HEVCFoundThisRound = true;
			return true;
		}
		return false;
	}

	// Hjelpefunksjon for å finne varigheten av media for prosentutregning.
	void FindAndSetDurationSecs(std::string Line)
	{
		try {

		std::smatch m;
		float tmpDurationSecs = 0.0f;
		
		// Fant vi duration? 
		if (std::regex_search(Line, m, rxDuration)) {
			tmpDurationSecs += std::stof(m[3].str().c_str());
			tmpDurationSecs += std::stof(m[2].str().c_str()) * 60;
			tmpDurationSecs += (std::stof(m[1].str().c_str()) * 60) * 60;
			DurationSecs = tmpDurationSecs;
		}

		} catch (...) {
			MiscStaticFuncsClass::GetErrorW(
				L"Failed during duration detection of media.", 
				false
			);
		}
	}

	// Hjelpefunksjon for å finne gjeldende konvertert tidspunkt av 
	// media for prosentutregning ved å sammenligne mot varighet.
	bool FindAndSetTimeSecs(std::string Line)
	{
		try {

		std::smatch m;
		float tmpTimeSecs = 0.0f;

		// Fant vi tidsfremgang?
		if (std::regex_search(Line, m, rxTime)) {
			tmpTimeSecs += std::stof(m[3].str().c_str());
			tmpTimeSecs += std::stof(m[2].str().c_str()) * 60;
			tmpTimeSecs += (std::stof(m[1].str().c_str()) * 60) * 60;
			
			// Litt overdrevent å låse her, men greit som referanse.
			std::lock_guard<std::mutex> lock(gMutexLock);
			TimeSecs = tmpTimeSecs;
			
			return true;
		} else {
			return false;
		}

		} catch(...) {
			MiscStaticFuncsClass::GetErrorW(
				L"Failed during time detection of media.", 
				false
			);
			return false;
		}
	}
};