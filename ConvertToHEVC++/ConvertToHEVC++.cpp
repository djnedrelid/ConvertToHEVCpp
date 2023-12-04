
//
// ConvertToHEVC++
// ©2020 Dag J Nedrelid <thronic@gmail.com>
//
// Bruker FFmpeg for bulk-konvertering til HEVC.
// Støtter kun NVIDIA med CUDA og NVENC støtte.
// NVENC tilbyr maskinakselerert konvertering.
//

#include "ConvertToHEVC++.h"
#define MAX_LOADSTRING 100

//
// Globale variabler.
//
HINSTANCE hInst;					// current instance
WCHAR szTitle[MAX_LOADSTRING];				// The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HWND MainWindow;					// Hovedvindu
HWND TextReportHwnd, PlayButtonHwnd;			// Rapportvindu og startknapp.
HWND KvalitetsMeny, SkaleringsMeny;			// Rullemenyer.
HWND TipText1, TipText2, TipText3, TipText4;		// Tekster.
HWND TotalPercentCompleted, TotalPercentImg;		// Totalprosent indikator.
HWND AutoStartSjekk, AutoStartToolTip;			// Sjekkboks for autostart.
HWND AcceptBiggerSjekk, AcceptBiggerFilesToolTip;	// Sjekkboks for akseptering av større resultatfiler.
HWND DetectAlreadyHEVC, DetectAlreadyHBYT;		// Sjekkboks for deteksjoner.
bool AutoStartIsChecked = false;
HFONT StandardFont, StandardFont2, StandardFont3;	// Fonter.
HFONT StandardFont2_small;
std::wstring Kvalitet = L"slow";
std::wstring Skalering = L"No Scaling";
std::wstring LastPercText = L"";			// Holder for prosent som vises, for EM_SETSEL,EM_REPLACESEL.
std::wstring LastPercTextFilename = L"";		// Holder for filnavn som blir behandlet. AddReportTextPerc().
bool ExitWhenDone = false;

// Kort notat for neste globale variabel som referanse ang. stack og heap.
// ConvertHEVC CHEVC = lever kortvarig mellom {} på stack.
// ConvertHEVC *CHEVC = new ConvertHEVC(); = lever langvarig på heap.
// heap reservasjoner med new og new[] (eks. new liste[5]) 
// må frigjøres igjen med delete og delete[] respektivt.
ConvertHEVC *CHEVC = new ConvertHEVC();			// StartConv() starter ::Start() i egen tråd.

//
// Prototyper.
//
ATOM			MyRegisterClass(HINSTANCE hInstance);
BOOL			InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
void			AddReportText(std::wstring s, bool bold, COLORREF textcolor);
void			AddReportTextPerc(std::wstring p, bool WarningText);
std::wstring		HentValgtMenyElement(HWND MenyElement);
std::wstring		HentValgtMenyNavn(HWND MenyNavn);
void			HovedLoop();
void			SjekkProgramParametere();
void			RegistrerMediafiler(std::wstring MediaPath);
void			LoadSettings();
void			SaveSettings();
void			EnableUI(int Enable);
void			StartConv();
bool			IsValidMediaFile(std::wstring f);
bool			IsValidSubtitleFile(std::wstring s);
void			PrepFonts();
void			FindRelatedSubtitle(std::wstring MediaFileFullPath);
void			AutoStartHandler();

//
// WinMain
//
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Forbered fonter.
	PrepFonts();

	// Initialiserer strenger fra .rc
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_CONVERTTOHEVC, szWindowClass, MAX_LOADSTRING);

	// Registrer klasser som skal brukes til vinduer.
	MyRegisterClass(hInstance);

	// Initialiser vinduer og kontroller.
	if(!InitInstance (hInstance, nCmdShow))
		return FALSE;

	// Avslutt hvis vi ikke har mottatt noen parametere.
	SjekkProgramParametere();

	// Last inn innstillinger.
	LoadSettings();

	// Vis historikk.
	int HistoryPercent = 0;
	if (TotaleBytesBehandlet == 0.0f)
		HistoryPercent = 0;
	else
		HistoryPercent = (int)(((float)TotaleBytesInnspart/(float)TotaleBytesBehandlet)*100);

	AddReportText(
		L"History: A total of "+ 
		MiscStaticFuncsClass::HumanReadableByteSize(TotaleBytesInnspart) +
		L" space has been gained from handling "+  
		MiscStaticFuncsClass::HumanReadableByteSize(TotaleBytesBehandlet) +
		L" files ("+ std::to_wstring(HistoryPercent) +L" %).\r\n",
		false, 
		RGB(0,0,255)
	);

	// Søk etter mediafiler som skal konverteres.
	RegistrerMediafiler(argv[1]);
	if (mediafiler.size() == 0 && !ExitWhenDone) {
		MiscStaticFuncsClass::GetErrorW(
			L"Found no files that were MP4 or MKV.\n"
			L"Files tagged with 'HBYT' are ignored.",
			true
		);
	} else if (mediafiler.size() == 0 && ExitWhenDone) {
		exit(EXIT_SUCCESS);
	}

	// List filer i rapportboksen.
	if (mediafiler.size() > 0)
		AddReportText(
			L"Detected media files ("+ 
			MiscStaticFuncsClass::HumanReadableByteSize(TotaleBytesSomSkalBehandles) +
			L"):", 
			false, 
			RGB(0,0,255)
		);

	for (unsigned int n=0; n<mediafiler.size(); n++) {
		if (n == (mediafiler.size()-1))
			AddReportText(std::to_wstring(n+1) +L") "+ mediafiler[n] +L"\r\n", false, RGB(0,0,0));
		else
			AddReportText(std::to_wstring(n+1) +L") "+ mediafiler[n], false, RGB(0,0,0));
	}

	// Rapportert eksisterende HEVC filer.
	if (HEVCExistingFilesFound > 0) 
		AddReportText(
		L"Existing HEVC file formats detected and skipped: " +
		std::to_wstring(HEVCExistingFilesFound) +L".",
		false,
		RGB(0,0,255)
	);

	// Sjekk om vi skal starte automatisk.
	AutoStartHandler();

	// Hovedloop.
	MSG msg;
	while(1) {
		// Håndter alle ventende meldinger.
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) > 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			// Avslutt hvis en av dem er WM_QUIT.
			if (msg.message == WM_QUIT)
				return (int)msg.wParam;
		}

		// Hovedloop funksjon.
		HovedLoop();

		// Litt forsinkelse for å ikke stresse CPU.
		Sleep(5);

	}

	return (int)msg.wParam;
}

//
// Funksjon for registrering av vinduklasser. Brukes i WinMain.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONVERTTOHEVC));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CONVERTTOHEVC);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_CONVERTTOHEVC));

	return RegisterClassExW(&wcex);
}

//
// Funksjon for oppsett av vinduer og kontroller. Brukes i WinMain.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	// Legg på versjonsnummer i vindutittel.
	std::wstring WindowTitle;
	WindowTitle.append(szTitle);
	WindowTitle.append(L" v1.7");

	// Lag hovedvinduet.
	// WS_OVERLAPPEDWINDOW byttet med WS_OVERLAPPED for å unngå maksimering, evt. &~WS_MAXIMIZEBOX.
	// Inkluderer WS_THICKFRAME for å beholde sizing. Men begrenset horisontalt i WndProc (WM_SIZING).
	if (!(MainWindow = CreateWindowW(
		szWindowClass, 
		WindowTitle.c_str(), 
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME, 
		CW_USEDEFAULT, 
		0, 
		1200, 
		600, 
		nullptr, 
		nullptr, 
		hInstance, 
		nullptr
	)))
		return FALSE;

	// Sett font for hovedvindu.
	SendMessage(MainWindow, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Lag tekstboks for rapportering.
	if (LoadLibraryW(L"Msftedit.dll") == 0)
		MiscStaticFuncsClass::GetErrorW(L"Could not load Msftedit.dll from the system.",true);

	TextReportHwnd = CreateWindowExW(
		WS_EX_CLIENTEDGE, // Bytt WS_EX_CLIENTEDGE med NULL for å fjerne kantlinjer.
		MSFTEDIT_CLASS, // L"EDIT" for normal edit. Bruker RICHEDIT50W for flere muligheter. Samme som WordPad i Windows 10(Spy++).
		L"", 
		WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
		2, 50, 1181, 490, 
		MainWindow, 
		(HMENU)ID_TEXT_REPORT, // Trengs egentlig kun for ting i dialoger for å få tak i dem, hvis HWND ikke er nok.
		0, 
		0
	);
	SendMessage(TextReportHwnd, WM_SETFONT, (WPARAM)StandardFont2, 1);

	// Lag meny for kvalitetsvalg.
	KvalitetsMeny = CreateWindowExW(
		WS_EX_CLIENTEDGE, 
		L"COMBOBOX", 
		L"",
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
		5, 5, 120, 300,	// Height må ha nok høyde til å kunne vise alle valg.
		MainWindow, 
		(HMENU)ID_KVALITET_MENY, 
		hInstance,
		NULL
	);
	SendMessage(KvalitetsMeny, CB_ADDSTRING, NULL, (LPARAM)L"Slow");
	SendMessage(KvalitetsMeny, CB_ADDSTRING, NULL, (LPARAM)L"Medium");
	SendMessage(KvalitetsMeny, CB_ADDSTRING, NULL, (LPARAM)L"Fast");
	SendMessage(KvalitetsMeny, CB_SETCURSEL, 0, NULL); // Velg Slow som standard.
	SendMessage(KvalitetsMeny, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Lag meny for skalering.
	SkaleringsMeny = CreateWindowExW(
		WS_EX_CLIENTEDGE, 
		L"COMBOBOX", 
		L"",
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
		130, 5, 165, 300, // Height må ha nok høyde til å kunne vise alle valg.
		MainWindow, 
		(HMENU)ID_SKALERING_MENY, 
		hInstance,
		NULL
	);
	SendMessage(SkaleringsMeny, CB_ADDSTRING, NULL, (LPARAM)L"No Scaling");
	SendMessage(SkaleringsMeny, CB_ADDSTRING, NULL, (LPARAM)L"AUTOx480");
	SendMessage(SkaleringsMeny, CB_ADDSTRING, NULL, (LPARAM)L"AUTOx576");
	SendMessage(SkaleringsMeny, CB_ADDSTRING, NULL, (LPARAM)L"AUTOx720");
	SendMessage(SkaleringsMeny, CB_ADDSTRING, NULL, (LPARAM)L"AUTOx1080");
	SendMessage(SkaleringsMeny, CB_ADDSTRING, NULL, (LPARAM)L"AUTOx1440");
	SendMessage(SkaleringsMeny, CB_ADDSTRING, NULL, (LPARAM)L"AUTOx2160");
	SendMessage(SkaleringsMeny, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Velg 'No Scaling' som standard (wParam, 0-basert indeks).
	SendMessage(SkaleringsMeny, CB_SETCURSEL, 0, NULL); 

	// Lag labels for info.
	TipText1 = CreateWindowExW(
		NULL, 
		L"STATIC", 
		L"Scaling will adjust output resolution to a selected preset.",
		WS_VISIBLE | WS_CHILD,
		300, 25, 330, 15,
		MainWindow, 
		(HMENU)ID_TIP_TEXT1, 
		hInstance,
		NULL
	);
	SendMessage(TipText1, WM_SETFONT, (WPARAM)StandardFont2, 1);

	TipText2 = CreateWindowExW(
		NULL, 
		L"STATIC", 
		L"Use the 'Slow' profile preset if you want the best quality.",
		WS_VISIBLE | WS_CHILD,
		300, 8, 330, 15,
		MainWindow, 
		(HMENU)ID_TIP_TEXT2, 
		hInstance,
		NULL
	);
	SendMessage(TipText2, WM_SETFONT, (WPARAM)StandardFont2, 1);

	TipText3 = CreateWindowExW(
		NULL, 
		L"STATIC", 
		L"Original files are\nremoved at 100%\nif fully converted.",
		WS_VISIBLE | WS_CHILD,
		640, 0, 130, 48,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	SendMessage(TipText3, WM_SETFONT, (WPARAM)StandardFont2_small, 1);

	// Illustrasjonsbilde (pil) for total fremgang.
	// Lag playbutton for å starte konvertering.
	TotalPercentImg = CreateWindowExW(
		NULL, 
		L"STATIC",
		L"",
		SS_BITMAP | WS_VISIBLE | WS_CHILD, //SS_BITMAP hvis STATIC. BS_BITMAP hvis BUTTON. For bildebruk.
		775, 25, 25, 25,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	HANDLE TotalPercentImg2 = LoadImageW(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDB_BITMAP2),
		IMAGE_BITMAP,
		NULL,
		NULL,
		LR_DEFAULTCOLOR
	);
	SendMessage(TotalPercentImg, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)TotalPercentImg2); //STM_SETIMAGE hvis STATIC.

	// Totalindikator for fremgang.
	TotalPercentCompleted = CreateWindowExW(
		NULL, 
		L"STATIC", 
		L"0 %",
		WS_VISIBLE | WS_CHILD,
		800, 2, 100, 30,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	SendMessage(TotalPercentCompleted, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Undertekst for total fremgang.
	TipText4 = CreateWindowExW(
		NULL, 
		L"STATIC", 
		L"Completed.",
		WS_VISIBLE | WS_CHILD,
		800, 33, 100, 15,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	SendMessage(TipText4, WM_SETFONT, (WPARAM)StandardFont3, 1);

	// Lag sjekkboks for akseptering av større filer.
	AcceptBiggerSjekk = CreateWindowExW(
		NULL, 
		L"BUTTON", 
		L"Accept bigger files.",
		BS_CHECKBOX | WS_VISIBLE | WS_CHILD,
		640, 38, 120, 11,
		MainWindow, 
		(HMENU)ID_BIGGERFILES_CHECK, 
		hInstance,
		NULL
	);
	SendMessage(AcceptBiggerSjekk, WM_SETFONT, (WPARAM)StandardFont2_small, 1);

	// Lag sjekkboks for automatisk oppstart.
	AutoStartSjekk = CreateWindowExW(
		NULL, 
		L"BUTTON", 
		L"Automatic Start.",
		BS_CHECKBOX | WS_VISIBLE | WS_CHILD,
		920, 5, 120, 15,
		MainWindow, 
		(HMENU)ID_AUTOSTART_CHECK, 
		hInstance,
		NULL
	);
	SendMessage(AutoStartSjekk, WM_SETFONT, (WPARAM)StandardFont2, 1);

	// Lag sjekkboks for ignorering av eksisterende HEVC.
	DetectAlreadyHEVC = CreateWindowExW(
		NULL, 
		L"BUTTON", 
		L"Detect already HEVC.",
		BS_CHECKBOX | WS_VISIBLE | WS_CHILD,
		920, 20, 150, 15,
		MainWindow, 
		(HMENU)ID_DETECTALREADYHEVC_CHECK, 
		hInstance,
		NULL
	);
	SendMessage(DetectAlreadyHEVC, WM_SETFONT, (WPARAM)StandardFont2, 1);

	// Lag sjekkboks for ignorering av eksisterende HBYT.
	DetectAlreadyHBYT = CreateWindowExW(
		NULL, 
		L"BUTTON", 
		L"Detect already HBYT.",
		BS_CHECKBOX | WS_VISIBLE | WS_CHILD,
		920, 35, 150, 15,
		MainWindow, 
		(HMENU)ID_DETECTALREADYHBYT_CHECK, 
		hInstance,
		NULL
	);
	SendMessage(DetectAlreadyHBYT, WM_SETFONT, (WPARAM)StandardFont2, 1);

	// Lag tooltip for sjekkboksen for automatisk oppstart.
	AutoStartToolTip = CreateWindowExW(
		WS_EX_TOPMOST, 
		TOOLTIPS_CLASSW, 
		NULL,
		WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	TOOLINFOW tf = {0};
	tf.cbSize = TTTOOLINFOW_V1_SIZE;
	tf.hwnd = MainWindow;			// dialogboks eller hovedvindu.
	tf.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	tf.uId = (UINT_PTR)AutoStartSjekk;	// HWND til kontroll tooltip skal knyttes til.
	tf.lpszText = (wchar_t*)L"Will start the converting process automatically on the next run(s).";
	if (SendMessage(AutoStartToolTip, TTM_ADDTOOLW, 0, (LPARAM)&tf) == 1)
		SendMessage(AutoStartToolTip, TTM_ACTIVATE, 1, 0);

	// Lag tooltip for sjekkboksen for akseptering av store filer.
	AcceptBiggerFilesToolTip = CreateWindowExW(
		WS_EX_TOPMOST, 
		TOOLTIPS_CLASSW, 
		NULL,
		WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	tf = {0};
	tf.cbSize = TTTOOLINFOW_V1_SIZE;
	tf.hwnd = MainWindow;			// dialogboks eller hovedvindu.
	tf.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	tf.uId = (UINT_PTR)AcceptBiggerSjekk;	// HWND til kontroll tooltip skal knyttes til.
	tf.lpszText = (wchar_t*)L"Will keep files bigger than original. Useful when upscaling.";
	if (SendMessage(AcceptBiggerFilesToolTip, TTM_ADDTOOLW, 0, (LPARAM)&tf) == 1)
		SendMessage(AcceptBiggerFilesToolTip, TTM_ACTIVATE, 1, 0);

	// Lag playbutton for å starte konvertering.
	PlayButtonHwnd = CreateWindowExW(
		NULL, 
		L"BUTTON",
		L"",
		BS_BITMAP | WS_VISIBLE | WS_CHILD, //SS_BITMAP hvis STATIC. BS_BITMAP hvis BUTTON. For bildebruk.
		1080, 2, 100, 45,
		MainWindow, 
		(HMENU)ID_PLAY_KNAPP, 
		hInstance,
		NULL
	);
	HANDLE PlayButtonImg = LoadImageW(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDB_BITMAP1),
		IMAGE_BITMAP,
		NULL,
		NULL,
		LR_DEFAULTCOLOR
	);
	SendMessage(PlayButtonHwnd, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)PlayButtonImg); //STM_SETIMAGE hvis STATIC.

	// Trenger kun sette hånd-peker 1 gang for alle knapper,
	// da det ser ut som den påvirker standardklassen globalt.
	// GCL_HCURSOR hvis 32-bit, GCLP_ hvis 64-bit.
	SetClassLongPtrW(PlayButtonHwnd, GCLP_HCURSOR, (long long)LoadCursorW(0, IDC_HAND));
	
	ShowWindow(MainWindow, nCmdShow);
	UpdateWindow(MainWindow);

	return 1;
}

//
// Meldingsbehandler for hovedvindu. Brukes i MyRegisterClass.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
		case WM_COMMAND:
		{
			switch (HIWORD(wParam)) 
			{
				// Menyvalg endringer.
				case CBN_SELCHANGE:
					std::wstring Menynavn, Menyelement;
					Menynavn = HentValgtMenyNavn((HWND)lParam);
					Menyelement = HentValgtMenyElement((HWND)lParam);

					AddReportText(
						L"Setting " +
						Menynavn + 
						L" to " +
						Menyelement,
						false,  
						RGB(0,0,255)
					);

					// Oppdater valg og lagre dem.
					if (Menynavn == L"Quality")
						Kvalitet = Menyelement;
					else if (Menynavn == L"Scaling")
						Skalering = Menyelement;

					SaveSettings();
					break;
			}

			switch(LOWORD(wParam))
			{
				// Toppmeny-valg og generelle ID klikk-hendelser. 
				case IDM_ABOUT:
					DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
					break;

				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;

				// Vis readme
				case ID_HELP_INFO:
				{
					std::wstring ReadmePath = std::wstring(ExePath).substr(0,std::wstring(ExePath).find_last_of(L'\\')+1) + L"Readme.txt";
					if (MiscStaticFuncsClass::FileExistsW(ReadmePath.c_str())) {
						ShellExecuteW(
							0,
							L"open",
							L"notepad.exe",
							ReadmePath.c_str(),
							0,
							SW_SHOWDEFAULT						
						);
					} else {
						MiscStaticFuncsClass::BeskjedW(L"Readme is not available.");
					}
				}
					break;

				// Vis logg.
				case ID_FILE_LOG:
				{
					if (MiscStaticFuncsClass::FileExistsW(LogPath.c_str()))
						ShellExecuteW(
							0,
							L"open",
							L"notepad.exe",
							LogPath.c_str(),
							0,
							SW_SHOWDEFAULT						
						);
					else
						MiscStaticFuncsClass::BeskjedW(L"No log is generated yet.");
				}
					break;

				// Sjekk om store filer skal aksepteres.
				case ID_BIGGERFILES_CHECK:
					if (!IsDlgButtonChecked(MainWindow, ID_BIGGERFILES_CHECK)) {
						CheckDlgButton(MainWindow, ID_BIGGERFILES_CHECK, BST_CHECKED);
						AcceptBiggerFiles = true;
						SaveSettings();
					} else {
						CheckDlgButton(MainWindow, ID_BIGGERFILES_CHECK, BST_UNCHECKED);
						AcceptBiggerFiles = false;
						SaveSettings();
					}
					break;

				// Sjekk om autostart skal være på eller av.
				case ID_AUTOSTART_CHECK:
					if (!IsDlgButtonChecked(MainWindow, ID_AUTOSTART_CHECK)) {
						CheckDlgButton(MainWindow, ID_AUTOSTART_CHECK, BST_CHECKED);
						AutoStartIsChecked = true;
						SaveSettings();
					} else {
						CheckDlgButton(MainWindow, ID_AUTOSTART_CHECK, BST_UNCHECKED);
						AutoStartIsChecked = false;
						SaveSettings();
					}
					break;

				// Sjekk om deteksjon av filer som allerede er HEVC skal være på eller av.
				case ID_DETECTALREADYHEVC_CHECK:
					if (!IsDlgButtonChecked(MainWindow, ID_DETECTALREADYHEVC_CHECK)) {
						CheckDlgButton(MainWindow, ID_DETECTALREADYHEVC_CHECK, BST_CHECKED);
						DetectAlreadyHEVCIsChecked = true;
						SaveSettings();
					} else {
						CheckDlgButton(MainWindow, ID_DETECTALREADYHEVC_CHECK, BST_UNCHECKED);
						DetectAlreadyHEVCIsChecked = false;
						SaveSettings();
					}
					break;

				// Sjekk om deteksjon av filer som allerede er HBYT skal være på eller av.
				case ID_DETECTALREADYHBYT_CHECK:
					if (!IsDlgButtonChecked(MainWindow, ID_DETECTALREADYHBYT_CHECK)) {
						CheckDlgButton(MainWindow, ID_DETECTALREADYHBYT_CHECK, BST_CHECKED);
						DetectAlreadyHBYTIsChecked = true;
						SaveSettings();
					} else {
						CheckDlgButton(MainWindow, ID_DETECTALREADYHBYT_CHECK, BST_UNCHECKED);
						DetectAlreadyHBYTIsChecked = false;
						SaveSettings();
					}
					MiscStaticFuncsClass::BeskjedW(L"This change is only reflected the next time you load file(s).");
					break;
				
				case ID_PLAY_KNAPP:
					EnableUI(0);
					AddReportText(L"Starting ...", false, RGB(0,0,255));
					SendMessage(TextReportHwnd, WM_VSCROLL, SB_TOP, 0);
					StartConv();
					break;

				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
			break;

		/*
			WM_: Referanse:
			CTLCOLORBTN       Button control
			CTLCOLORDLG       Dialog box
			CTLCOLOREDIT      Edit control
			CTLCOLORLISTBOX    List-box control
			CTLCOLORMSGBOX     Message box
			CTLCOLORSCROLLBAR  Scroll-bar control
			CTLCOLORSTATIC     Static control 
		*/
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
		{
			// Håndter farger.
			HDC hdc = (HDC)wParam;
			HWND hwnd = (HWND)lParam;

			if (
				hwnd == TipText1 || 
				hwnd == TipText2 || 
				hwnd == AutoStartSjekk || 
				hwnd == DetectAlreadyHEVC
			) {
				SetTextColor(hdc, RGB(0,0,0));
				SetBkColor(hdc, RGB(255,255,255));
				SetDCBrushColor(hdc, RGB(255,255,255));
			
			} else if (
				hwnd == TipText3 || 
				hwnd == TipText4 ||
				hwnd == TotalPercentCompleted
			) {
				SetTextColor(hdc, RGB(0,0,255));
				SetBkColor(hdc, RGB(255,255,255));
				SetDCBrushColor(hdc, RGB(255,255,255));
			
			} else if (hwnd == TextReportHwnd) {
				SetTextColor(hdc, RGB(0,0,0));
				SetBkColor(hdc, RGB(255,255,255));
				SetDCBrushColor(hdc, RGB(255,255,255));
			}

			// Returnerer DC_BRUSH som standard er hvit. 
			// Returnert brush brukes til å farge bakgrunn.
			// Vi setter likevel eksplisitt ovenfor, 
			// fordi vi er noobs og vil være sikre..
			return (LRESULT) GetStockObject(DC_BRUSH);
		}
			break;

		// Tilpass rapport teksten ved hvilken som helst size event.
		case WM_SIZE:
		{
			RECT CurMainClientSize;
			GetClientRect(hWnd, &CurMainClientSize);
			
			// Oppdater edit kontrollen.
			SetWindowPos(
				TextReportHwnd, 
				HWND_TOP, 
				2, 
				50, 
				1181, 
				CurMainClientSize.bottom - 52,
				SWP_NOMOVE
			);
		}
			break;


		// Kun tillat vertikal resize.
		case WM_SIZING:
		{
			// Hent alle størrelser.
			RECT CurMainWindowSize, CurMainClientSize, ReportTextSize;
			RECT *NewMainWindowSize = (RECT*)lParam;
			GetClientRect(TextReportHwnd, &ReportTextSize);
			GetWindowRect(hWnd, &CurMainWindowSize);
			GetClientRect(hWnd, &CurMainClientSize);
			
			// Tilpass edit hvis noen vil dra opp eller ned på vanlig vis.
			if (wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM) {

				// Oppdater edit kontrollen.
				SetWindowPos(
					TextReportHwnd, 
					HWND_TOP, 
					2, 
					50, 
					1181, 
					CurMainClientSize.bottom - 52, 
					SWP_NOMOVE
				);

			} else {
				// Behold størrelse horisontalt.
				NewMainWindowSize->left = CurMainWindowSize.left;
				NewMainWindowSize->right = CurMainWindowSize.right;
			} 

			return (LRESULT)1;
		}
			break;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			// TODO: Add any drawing code that uses hdc here...
			EndPaint(hWnd, &ps);
		}
			break;

		case WM_DESTROY:
		{
			// Drep ffmpeg prosess hvis den fortsatt kjører.
			unsigned long prosess_exit_code = 0;
			GetExitCodeProcess(piProcInfo.hProcess, &prosess_exit_code);
			if (prosess_exit_code == STILL_ACTIVE) 
				TerminateProcess(piProcInfo.hProcess,0);
			
			// Avslutt program.
			PostQuitMessage(0);
		}
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//
// Funksjon for oppsett av fonter. Brukes i WinMain.
//
void PrepFonts()
{
	// Sett opp standard fonter jeg vil bruke.
	StandardFont = CreateFont(
		28,			// Høyde.
		0,			// Bredde.
		0, 
		0, 
		400,			// 0-1000. 400=Normal, 700=bold.
		false,			// Italic.
		false,			// Underline.
		false,			// Strikeout.
		DEFAULT_CHARSET,	// Charset.
		OUT_OUTLINE_PRECIS,	
		CLIP_DEFAULT_PRECIS, 
		ANTIALIASED_QUALITY,	// Kvalitet.
		VARIABLE_PITCH,	
		L"Verdana"		// Font.
	);
	
	StandardFont2 = CreateFont(
		13,			// Høyde.
		0,			// Bredde.
		0, 
		0, 
		400,			// 0-1000. 400=Normal, 700=bold.
		false,			// Italic.
		false,			// Underline.
		false,			// Strikeout.
		DEFAULT_CHARSET,	// Charset.
		OUT_OUTLINE_PRECIS,	
		CLIP_DEFAULT_PRECIS, 
		ANTIALIASED_QUALITY,	// Kvalitet.
		VARIABLE_PITCH,	
		L"Verdana"		// Font.
	);

	StandardFont2_small = CreateFont(
		12,			// Høyde.
		0,			// Bredde.
		0, 
		0, 
		400,			// 0-1000. 400=Normal, 700=bold.
		false,			// Italic.
		false,			// Underline.
		false,			// Strikeout.
		DEFAULT_CHARSET,	// Charset.
		OUT_OUTLINE_PRECIS,	
		CLIP_DEFAULT_PRECIS, 
		ANTIALIASED_QUALITY,	// Kvalitet.
		VARIABLE_PITCH,	
		L"Verdana"		// Font.
	);

	StandardFont3 = CreateFont(
		13,			// Høyde.
		0,			// Bredde.
		0, 
		0, 
		700,			// 0-1000. 400=Normal, 700=bold.
		false,			// Italic.
		false,			// Underline.
		false,			// Strikeout.
		DEFAULT_CHARSET,	// Charset.
		OUT_OUTLINE_PRECIS,	
		CLIP_DEFAULT_PRECIS, 
		ANTIALIASED_QUALITY,	// Kvalitet.
		VARIABLE_PITCH,	
		L"Verdana"		// Font.
	);
}

//
// Funksjon for å starte en konverteringsjobb i egen tråd. Brukes i WndProc.
//
static void StartConv()
{
	// Starter CHEVC.Start() i en egen tråd.
	// Fra cppreference:
	// std::thread t5(&foo::bar, &f); // t5 runs foo::bar() on object f
	
	std::thread _start (&ConvertHEVC::Start, CHEVC, &mediafiler, &subtitles, Skalering, Kvalitet);
	_start.detach();
}

//
// Message handler for about box.
//
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch(message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

//
// Slår av og på evt. UI kontroller/elementer. Brukes i WndProc.
//
void EnableUI(int Enable)
{
	EnableWindow(KvalitetsMeny, Enable);
	EnableWindow(SkaleringsMeny, Enable);
	EnableWindow(PlayButtonHwnd, Enable);
	EnableWindow(DetectAlreadyHEVC, Enable);
	EnableWindow(DetectAlreadyHBYT, Enable);
}

//
// Funksjon for å legge til tekst i rapportboksen (TextReportHwnd). Brukes i WinMain og HovedLoop.
//
void AddReportText(std::wstring s, bool bold, COLORREF textcolor) 
{	
	// Reserver minne til mellomlager, for å sjekke om tekst er tom eller ikke.
	int TextReportLen = GetWindowTextLengthW(TextReportHwnd);
	wchar_t* tmpTextReport = new wchar_t[TextReportLen+1]; // Husk å delete[].
	if (GetWindowTextW(TextReportHwnd, tmpTextReport, TextReportLen+1) == 0 && TextReportLen != 0) {
		MiscStaticFuncsClass::GetErrorW(L"Could not retrieve previous report text (1).", false);
		return;
	}

	// Sett inn ny tekst på inngangspunkt (slutten).
	// Begynn kun med \r\n (ny linje) hvis innholdet ikke er tomt fra før.
	std::wstring s2 = tmpTextReport;
	delete[] tmpTextReport;
	std::wstring s3 = (s2==L""?L"":L"\r\n")+ MiscStaticFuncsClass::DatotidW() + L"  " + s;
	SendMessage(TextReportHwnd, EM_SETSEL, TextReportLen, TextReportLen);

	if (bold)
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, CFE_BOLD, CFM_BOLD | CFM_COLOR, textcolor);
	else
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, 0, CFE_BOLD | CFM_COLOR, textcolor);

	SendMessage(TextReportHwnd, EM_REPLACESEL, 0, (LPARAM)s3.c_str());

	// Skroll nedover.
	//SendMessage(TextReportHwnd, WM_VSCROLL, SB_BOTTOM, 0);
}

//
// Funksjon for oppdatering av prosenttekst. Søker posisjon til nåværende fil- og prosenttekst som blir 
// behandlet fra forrige runde. Forbereder ny tekst og erstatter eksisterende med ny informasjon. 
// Globale variabler: LastPercText, LastPercTextFilename.
// Brukes i HovedLoop.
//
void AddReportTextPerc(std::wstring p, bool WarningText)
{
	// Hent nåværende tekstlengde.
	int TextReportLen = GetWindowTextLengthW(TextReportHwnd);

	// Forbered ny prosent.
	std::wstring NewPercent;
	if (p == L"Failed" || p == L"Bigger") {
		NewPercent.append(L" ("+ p +L")");

		//if (p == L"Bigger")
		//	NewPercent.append(L" Keeping original, tagged HBYT.");
		//else if (p == L"Failed")
		//	NewPercent.append(L" Keeping original, no changes.");

	} else {
		NewPercent.append(L" (" + p +L"%)");
	}

	// Finn posisjon til forrige prosent fil- og tekst.
	std::wstring SearchText = LastPercTextFilename + LastPercText;
	std::wstring InsertText = LastPercTextFilename + NewPercent;
	long long loc = TextReportLen;
	FINDTEXTW ftw;
	CHARRANGE cr;
	cr.cpMin = 0;
	cr.cpMax = -1;
	ftw.chrg = cr;
	ftw.lpstrText = SearchText.c_str();
	if((int)(loc = SendMessage(TextReportHwnd, EM_FINDTEXTW, FR_DOWN, (LPARAM)&ftw)) == -1) 
		MiscStaticFuncsClass::GetErrorW(
			L"Could not find position of previous percentage:\n"
			L"SearchText = \n\n" + SearchText,
			false
		);

	// Sett inn tekst på inngangspunkt.
	if (WarningText) {
		SendMessage(TextReportHwnd, EM_SETSEL, (WPARAM)loc, (LPARAM)(loc+LastPercTextFilename.length()));
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, 0, CFM_BOLD | CFM_COLOR, RGB(0,0,0));
		SendMessage(TextReportHwnd, EM_REPLACESEL, 0, (LPARAM)LastPercTextFilename.c_str());

		SendMessage(TextReportHwnd, EM_SETSEL, (WPARAM)loc+LastPercTextFilename.length(), (LPARAM)(loc+LastPercTextFilename.length()+LastPercText.length()));
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, CFE_BOLD, CFM_BOLD | CFM_COLOR, RGB(255,0,0));
		SendMessage(TextReportHwnd, EM_REPLACESEL, 0, (LPARAM)NewPercent.c_str());

	} else {
		SendMessage(TextReportHwnd, EM_SETSEL, (WPARAM)loc, (LPARAM)(loc+LastPercTextFilename.length()));
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, 0, CFM_BOLD | CFM_COLOR, RGB(0,0,0));
		SendMessage(TextReportHwnd, EM_REPLACESEL, 0, (LPARAM)LastPercTextFilename.c_str());

		SendMessage(TextReportHwnd, EM_SETSEL, (WPARAM)loc+LastPercTextFilename.length(), (LPARAM)(loc+LastPercTextFilename.length()+LastPercText.length()));
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, CFE_BOLD, CFM_BOLD | CFM_COLOR, RGB(0,128,0));
		SendMessage(TextReportHwnd, EM_REPLACESEL, 0, (LPARAM)NewPercent.c_str());
	}

	// Lagre prosent til neste runde.
	LastPercText = NewPercent;
}

//
// Hjelpefunksjon for WndProc.
//
std::wstring HentValgtMenyNavn(HWND MenyNavn)
{
	if (MenyNavn == KvalitetsMeny)
		return L"Quality";
	else if (MenyNavn == SkaleringsMeny)
		return L"Scaling";
	
	return L"Menu";
}

//
// Hjelpefunksjon for WndProc.
//
std::wstring HentValgtMenyElement(HWND MenyElement)
{
	wchar_t ListItem[100];
	SendMessage(
		MenyElement, 
		CB_GETLBTEXT, 
		SendMessage(MenyElement, CB_GETCURSEL, NULL, NULL), // ItemIndex.
		(LPARAM)ListItem
	);
	
	return (std::wstring)ListItem;
}

//
// Funksjon for validering av program-argumenter. Brukes i WinMain.
//
void SjekkProgramParametere()
{
	int args;
	LPWSTR* tmp_argv = CommandLineToArgvW(GetCommandLineW(), &args);

	if (tmp_argv == NULL)
		MiscStaticFuncsClass::GetErrorW(
			L"Could not read program parameters.", 
			true
		);

	// Programkommando er alltid første parameter.
	if (args == 1) {
		MiscStaticFuncsClass::BeskjedW(
			L"You gave me nothing to do, so I'm\n"
			L"gonna make like a tree and LEAF!"
		);

		// Send WM_CLOSE hvis vi har ting å rydde opp i WM_DESTROY.
		//SendMessageW(MainWindow, WM_CLOSE, NULL, NULL);
		LocalFree(tmp_argv);
		exit(0);
	}

	// Samle argumenter og frigjør minnet til CommandLineToArgvW.
	for (int n=0; n<args; n++)
		argv.push_back(tmp_argv[n]);

	// Rapporter hvilken sti vi mottok.
	if (PathIsDirectoryW(argv[1].c_str()))
		AddReportText(L"Received path: " + argv[1], false, RGB(0,0,255));

	// Sjekk videre om vi har mottatt EXITWHENDONE.
	for (int a=0; a<argv.size(); a++) 
		if(argv[a].find(L"EXITWHENDONE") != std::string::npos)
			ExitWhenDone = true;
	
	// Lar stå for lstrcmpiW referanse..
	//if (args == 3) {
	//	if (lstrcmpiW(argv[2].c_str(), L"EXITWHENDONE") == 0)
	//		ExitWhenDone = true;
	//}

	LocalFree(tmp_argv);
}

//
// Funksjon for registrering av media- og undertekstfiler. Brukes i WinMain.
//
void RegistrerMediafiler(std::wstring MediaPath)
{
	WIN32_FIND_DATAW fd;
	std::wstring Mappe = MediaPath + L"\\*.*";

	// Er det en enkeltfil?
	if (PathIsDirectoryW(MediaPath.c_str()) == 0) {

		// Sjekk om fila faktisk eksisterer.
		if (!MiscStaticFuncsClass::FileExistsW(MediaPath.c_str()))
			MiscStaticFuncsClass::GetErrorW(
				L"Provided file does not exist.",
				true
			);

		// Er det MP4 eller MKV?
		if (!IsValidMediaFile(MediaPath)) 
			return;

		// Registrer fil for konvertering.
		mediafiler.push_back(MediaPath);
		TotaleBytesSomSkalBehandles += MiscStaticFuncsClass::FileSizeInBytesW(MediaPath.c_str());

		// Finnes det en tilhørende subtitle?
		FindRelatedSubtitle(MediaPath);

		return;
	}
	
	HANDLE FileSearchHandle = FindFirstFileW(Mappe.c_str(), &fd);
	if (FileSearchHandle != INVALID_HANDLE_VALUE) {
		while (FindNextFileW(FileSearchHandle, &fd) != 0) {
			
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				
				// Rekursivt kall for å grave dypere.
				if (lstrcmpiW(fd.cFileName, L".") != 0 && lstrcmpiW(fd.cFileName, L"..") != 0)
					RegistrerMediafiler(MediaPath +L"\\"+ fd.cFileName);

			} else {
				// Legg til fil hvis det er media.
				if (IsValidMediaFile(fd.cFileName)) {
					mediafiler.push_back(MediaPath +L"\\"+ fd.cFileName);
					TotaleBytesSomSkalBehandles += MiscStaticFuncsClass::FileSizeInBytesW(mediafiler[mediafiler.size()-1].c_str());
				}

				// Legg til fil hvis det er subtitle.
				if (IsValidSubtitleFile(fd.cFileName))
					subtitles.push_back(MediaPath +L"\\"+ fd.cFileName);
			}
		}
		FindClose(FileSearchHandle);
	}
}

//
// Hjelpefunksjon for RegistrerMediafiler. 
//
void FindRelatedSubtitle(std::wstring MediaFileFullPath)
{
	WIN32_FIND_DATAW fd;
	std::wstring FolderPathFromFile = MediaFileFullPath.substr(0,MediaFileFullPath.find_last_of(L"\\"));
	std::wstring Mappe = FolderPathFromFile + L"\\*.*";

	HANDLE FileSearchHandle = FindFirstFileW(Mappe.c_str(), &fd);
	if (FileSearchHandle != INVALID_HANDLE_VALUE) {
		while (FindNextFileW(FileSearchHandle, &fd) != 0) {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				
				// Legg til fil hvis det er subtitle.
				if (IsValidSubtitleFile(fd.cFileName))
					subtitles.push_back(FolderPathFromFile +L"\\"+ fd.cFileName);
			}
		}
		FindClose(FileSearchHandle);
	}
}

//
// Hjelpefunksjon for RegistrerMediafiler.
//
bool IsValidMediaFile(std::wstring f)
{
	// Avslå hvis ingen av forventede media endelser finnes.
	if (
		f.find(L".mp4") == std::wstring::npos && 
		f.find(L".MP4") == std::wstring::npos && 
		f.find(L".mkv") == std::wstring::npos && 
		f.find(L".MKV") == std::wstring::npos
	)
		return false;

	// Ta høyde for HBYT.
	if (DetectAlreadyHBYTIsChecked && f.find(L"HBYT") != std::wstring::npos)
		return false;

	return true;
}

//
// Hjelpefunksjon for FindRelatedSubtitle.
//
bool IsValidSubtitleFile(std::wstring f)
{
	if (
		f.find(L"HBYT") == std::wstring::npos && 
		(
			f.find(L".srt") != std::wstring::npos || 
			f.find(L".SRT") != std::wstring::npos ||
			f.find(L".smi") != std::wstring::npos || 
			f.find(L".SMI") != std::wstring::npos ||
			f.find(L".ssa") != std::wstring::npos || 
			f.find(L".SSA") != std::wstring::npos ||
			f.find(L".ass") != std::wstring::npos || 
			f.find(L".ASS") != std::wstring::npos ||
			f.find(L".vtt") != std::wstring::npos || 
			f.find(L".VTT") != std::wstring::npos
		)
	)
		return true;
	else
		return false;
}

//
// Funksjon for innlasting av konfigurasjonsdata. Brukes i WinMain.
//
void LoadSettings()
{
	// Kvalitet og Skalering.
	std::wstring Settingname, Equalsign, S1, S2;
	int AutoStartCheckVal = 0;
	int AcceptBiggerFilesCheckVal = 0;
	int DetectAlreadyHEVCCheckVal = 1;
	int DetectAlreadyHBYTCheckVal = 1;

	// Hvis filer ikke eksisterer allerede, generer dem.
	if (
		!MiscStaticFuncsClass::FileExistsW(SettingsPath.c_str()) || 
		!MiscStaticFuncsClass::FileExistsW(DbPathW.c_str())
	) {
		SaveSettings();
		//return;
	}

	// Filer som skal importeres.
	std::wifstream Settings(SettingsPath.c_str());
	
	// Hent verdier.
	Settings >> Settingname >> Equalsign >> Kvalitet;
	Settings >> Settingname >> Equalsign >> S1;
	if (S1 == L"No") { // Hent kun S2 hvis innstillingen er "No Scaling".
		Settings >> S2;
		Skalering = S1 +L" "+ S2;
	} else {
		Skalering = S1;
	}
	Settings >> Settingname >> Equalsign >> AutoStartCheckVal;
	Settings >> Settingname >> Equalsign >> DetectAlreadyHEVCCheckVal;
	Settings >> Settingname >> Equalsign >> DetectAlreadyHBYTCheckVal;
	Settings >> Settingname >> Equalsign >> AcceptBiggerFilesCheckVal;
		
	// Accept bigger files sjekkboks.
	if (AcceptBiggerFilesCheckVal == 1) {
		AcceptBiggerFiles = true;
		CheckDlgButton(MainWindow, ID_BIGGERFILES_CHECK, BST_CHECKED);
	} else {
		AcceptBiggerFiles = false;
		CheckDlgButton(MainWindow, ID_BIGGERFILES_CHECK, BST_UNCHECKED);
	}

	// Auto start sjekkboks.
	if (AutoStartCheckVal == 1) {
		AutoStartIsChecked = true;
		CheckDlgButton(MainWindow, ID_AUTOSTART_CHECK, BST_CHECKED);
	} else {
		AutoStartIsChecked = false;
		CheckDlgButton(MainWindow, ID_AUTOSTART_CHECK, BST_UNCHECKED);
	}

	// Detekter eksisterende HEVC filer.
	if (DetectAlreadyHEVCCheckVal == 1) {
		DetectAlreadyHEVCIsChecked = true;
		CheckDlgButton(MainWindow, ID_DETECTALREADYHEVC_CHECK, BST_CHECKED);
	} else {
		DetectAlreadyHEVCIsChecked = false;
		CheckDlgButton(MainWindow, ID_DETECTALREADYHEVC_CHECK, BST_UNCHECKED);
	}

	// Detekter eksisterende HBYT filer.
	if (DetectAlreadyHBYTCheckVal == 1) {
		DetectAlreadyHBYTIsChecked = true;
		CheckDlgButton(MainWindow, ID_DETECTALREADYHBYT_CHECK, BST_CHECKED);
	} else {
		DetectAlreadyHBYTIsChecked = false;
		CheckDlgButton(MainWindow, ID_DETECTALREADYHBYT_CHECK, BST_UNCHECKED);
	}

	// Sjekk parametere for overstyring av kvalitet og skalering.
	size_t funnet = std::string::npos;
	for (int a=0; a<argv.size(); a++) {

		funnet = argv[a].find(L"scaling=");
		if (funnet != std::string::npos)
			Skalering = argv[a].substr(funnet+8);

		funnet = argv[a].find(L"quality=");
		if (funnet != std::string::npos)
			Kvalitet = argv[a].substr(funnet+8);
	}

	// Skalering.
	if (Kvalitet == L"slow")
		SendMessage(KvalitetsMeny, CB_SETCURSEL, 0, NULL);
	else if(Kvalitet == L"medium")
		SendMessage(KvalitetsMeny, CB_SETCURSEL, 1, NULL);
	else
		SendMessage(KvalitetsMeny, CB_SETCURSEL, 2, NULL);

	if (Skalering == L"No Scaling")
		SendMessage(SkaleringsMeny, CB_SETCURSEL, 0, NULL);
	else if (Skalering == L"AUTOx480")
		SendMessage(SkaleringsMeny, CB_SETCURSEL, 1, NULL);
	else if (Skalering == L"AUTOx576")
		SendMessage(SkaleringsMeny, CB_SETCURSEL, 2, NULL);
	else if (Skalering == L"AUTOx720")
		SendMessage(SkaleringsMeny, CB_SETCURSEL, 3, NULL);
	else if (Skalering == L"AUTOx1080")
		SendMessage(SkaleringsMeny, CB_SETCURSEL, 4, NULL);
	else if (Skalering == L"AUTOx1440")
		SendMessage(SkaleringsMeny, CB_SETCURSEL, 5, NULL);
	else
		SendMessage(SkaleringsMeny, CB_SETCURSEL, 6, NULL);

	// Statistikk.
	/*
	db.Open(DbPathA.c_str());
	db.QuerySelect(L"SELECT TotalBytesProcessed, TotalBytesSaved FROM statistics");
	for (int n=0; n<db.Rows.size(); n++) {
		TotaleBytesBehandlet += (unsigned long long)db.Rows[n][0]._integer;
		TotaleBytesInnspart += (unsigned long long)db.Rows[n][1]._integer;
	}
	db.Close();
	*/

	/* Ole klarte å overstige 18,446,744,073,709,551,615 ...
* 	  FEIL - var pga en brainfart. Det ville også betydd 18446744 TB... */
	db.Open(DbPathA.c_str());
	db.QuerySelect(L"SELECT SUM(TotalBytesProcessed), SUM(TotalBytesSaved) FROM statistics");
	for (int n=0; n<db.Rows.size(); n++) {
		TotaleBytesBehandlet += (unsigned long long)db.Rows[n][0]._integer;
		TotaleBytesInnspart += (unsigned long long)db.Rows[n][1]._integer;
	}
	db.Close();
	

	/* Men i tilfelle vi trenger slike ekstreme lagringsmengder i fremtiden, 
* 	  har jeg allerede laget en klasse som skal kunne takle det. Beholdes for referanse.
	db.Open(DbPathA.c_str());
	db.QuerySelect(L"SELECT TotalBytesProcessed, TotalBytesSaved FROM statistics");
	for (int n=0; n<db.Rows.size(); n++) {

		// Registrer antall bytes for historisk visning ved oppstart i egen klasse.
		HistoricalStats.ProcessedBytes.push_back((unsigned long long)db.Rows[n][0]._integer);
		HistoricalStats.SavedBytes.push_back((unsigned long long)db.Rows[n][1]._integer);
	}
	db.Close();
	*/
}

//
// Funksjon for automatisk start dersom sjekkboks er aktiv ved oppstart.
//
void AutoStartHandler()
{
	bool autostart = false;

	if (IsDlgButtonChecked(MainWindow, ID_AUTOSTART_CHECK))
		autostart = true;

	// Sjekk om en parameter overstyrer autostart.
	for (int a=0; a<argv.size(); a++) {
		if (argv[a] == L"autostart")
			autostart = true;
	}

	if (autostart) {
		// Utfør vanlig startprosedyre.
		EnableUI(0);
		AddReportText(L"Starting ...", false, RGB(0,0,255));
		SendMessage(TextReportHwnd, WM_VSCROLL, SB_TOP, 0);
		StartConv();
	}
}

//
// Funksjon for lagring av konfigurasjonsdata. 
// Brukes i HovedLoop, WndProc og LoadSettings.
//
void SaveSettings()
{
	std::wofstream Settings(SettingsPath.c_str());

	// Konverter til små bokstaver. Eks. Slow = slow.
	// ellers klarer ikke ffmpeg å tolke det riktig.
	std::transform(
		Kvalitet.begin(), 
		Kvalitet.end(),
		Kvalitet.begin(),
		[](wchar_t c){ return std::towlower(c); } // yay... lambda.
	);

	Settings << "Quality = " << Kvalitet << std::endl
		<< "Scaling = " << Skalering << std::endl
		<< "Autostart = " << (AutoStartIsChecked?1:0) << std::endl
		<< "DetectHEVC = " << (DetectAlreadyHEVCIsChecked?1:0) << std::endl
		<< "DetectHBYT = " << (DetectAlreadyHBYTIsChecked?1:0) << std::endl
		<< "AcceptBiggerFiles = " << (AcceptBiggerFiles?1:0);

	// Opprett DbPath hvis den ikke finnes, kopier fra exepath\fil.
	std::wstring ExeDbPath = std::wstring(ExePath).substr(0,std::wstring(ExePath).find_last_of(L'\\')+1) + L"Statistics.sqlite3";
	if (!MiscStaticFuncsClass::FileExistsW(DbPathW.c_str()))
		if (CopyFileW(ExeDbPath.c_str(), DbPathW.c_str(), 0) == 0)
			MiscStaticFuncsClass::GetErrorW(L"Could not regenerate db to userdata.",true);
}

//
// Funksjon for hovedfunksjonalitet og sanntidsloop.
//
void HovedLoop()
{
	// Sjekk om det jobbes med en fil.
	if (FileBeingWorkedOn != L"") {
		EnableUI(0);
		LastPercTextFilename = FileBeingWorkedOn;
		//AddReportText(std::to_wstring(FileBeingWorkedOnNum) +L") "+ FileBeingWorkedOn, false);
		FileBeingWorkedOn = L"";
		LastPercText = L"";
	}

	// Sjekk om alt av arbeid har blitt fullført.
	if (HasCompletedAQueue) {

		// Oppdater total fremgang.
		SendMessage(TotalPercentCompleted, WM_SETTEXT, 0, (LPARAM)L"100 %");

		HasCompletedAQueue = false;
		EnableWindow(KvalitetsMeny, 1);
		EnableWindow(SkaleringsMeny, 1);
		SaveSettings(); // Lagrer statistikk utenfor programmet.
		AddReportText(L"Done.\r\n", false, RGB(0,0,255));

		// Var det HEVC filer funnet?
		if (HEVCExistingFilesFound > 0)
			AddReportText(
				std::to_wstring(HEVCExistingFilesFound) +
				L" file(s) already encoded as HEVC were detected and tagged.", 
				false, 
				RGB(0,0,255)
			);

		// Rapporter hvor mye data som ble behandlet denne runden.
		int RoundPercent = 0;
		if (drTotaleBytesBehandlet == 0.0f)
			RoundPercent = 0;
		else
			RoundPercent = (int)(100-(((float)drTotaleBytesKonvertert/(float)drTotaleBytesBehandlet)*100)+0.5f);

		// Rapporter hvis noe gikk galt.
		if (DetectedFailedErrEver) {
			AddReportText(L"(Failed) files are kept original for future runs. Maybe you were out of streams.", true, RGB(255,0,0));
			AddReportText(L"Make sure your GPU driver is up to date, it may add needed CUDA toolkit support.", true, RGB(255,0,0));
			AddReportText(L"You may check File > FFmpeg log for any additional raw details.\r\n", true, RGB(255,0,0));
			AddReportText(L"A log message like 'out of memory' from hevc_nvenc is normal when you're out of streams (e.g. busy Plex Server).", true, RGB(255,0,0));
			AddReportText(L"If that's the case, you can either run it on a dedicated computer, or patch your geforce driver for more streams.\r\n", true, RGB(255,0,0));
		}
		if (DetectedFailedBigEver) {
			AddReportText(L"(Bigger) files are tagged/named with HBYT so they will be ignored on future runs.", true, RGB(255,0,0));
			AddReportText(L"(Bigger) means the resulting files were bigger after conversion than before.\r\n", true, RGB(255,0,0));
		}

		// Lagre sluttresultatene i databasen.
		// (Flyttes til å lagre per enkelt fil).
		/*
		if (drTotaleBytesBehandlet > 0) {
			db.Open(DbPathA.c_str());
			std::wstring LogBytesQuery = L""
				L"INSERT INTO statistics "
				L"(TotalBytesProcessed,TotalBytesSaved) "
				L"VALUES "
				L"("+ std::to_wstring(drTotaleBytesBehandlet) +L","+ std::to_wstring(drTotaleBytesInnspart) +L")";
			db.QueryExecute(LogBytesQuery.c_str());
			db.Close();
		}*/

		AddReportText(L""+
			MiscStaticFuncsClass::HumanReadableByteSize(drTotaleBytesBehandlet) +
			L" converted to "+
			MiscStaticFuncsClass::HumanReadableByteSize(drTotaleBytesKonvertert) +
			L". "+
			std::to_wstring(RoundPercent) + 
			L"% space gained this round.\r\n\r\n", 
			false, 
			RGB(0,0,255)
		);

		// Frigjør prosessobjekt som kjørte i egen tråd.
		delete CHEVC;

		// Skroll helt ned.
		SendMessage(TextReportHwnd, WM_VSCROLL, SB_BOTTOM, 0);

		// Avslutt automatisk hvis argument 3 til programmet er EXITWHENDONE.
		if (ExitWhenDone)
			exit(0);
	}

	// Begynn oppdatering av prosent hvis prosesstråd har økt den fra 0.
	// Prosess vil sette den til -1 som betyr at vi kan frigjøre minnet.
	// !Pass på at ProcessPipeClass faktisk venter på at den er 0 igjen!
	// AddReportTextPerc vil da reservere nytt minne for neste runde.

	// Litt overdrevent å låse her, men greit som referanse.
	gMutexLock.lock();
	if (FFmpegPercentage > 0) {
		AddReportTextPerc(std::to_wstring(FFmpegPercentage), false);
		
		// Forhindre blinking fra brukerklikk ved at fokuset blir tatt vekk per runde.
		SetFocus(AutoStartSjekk);

	} else if (FFmpegPercentage == -1) {

		// Setter prosent til 100 i tilfelle prosess stoppa oppdatering på 99.
		if (!DetectedFailedConversionSingleRoundERR && !DetectedFailedConversionSingleRoundBIG) {
			AddReportTextPerc(std::to_wstring(100), false); 

		} else if (DetectedFailedConversionSingleRoundERR){
			AddReportTextPerc(L"Failed", true);
			DetectedFailedConversionSingleRoundERR = false;

		} else if (DetectedFailedConversionSingleRoundBIG){
			AddReportTextPerc(L"Bigger", true);
			DetectedFailedConversionSingleRoundBIG = false;
		}

		// Oppdater total fremgang %.
		float _f = (((float)FerdigKonvertertAntall / (float)mediafiler.size()) * 100) + 0.5f;
		std::wstring _s = std::to_wstring(_f);
		_s = _s.substr(0,_s.find_last_of('.')) + L" %";
		SendMessage(TotalPercentCompleted, WM_SETTEXT, 0, (LPARAM)_s.c_str());

		// 0 vil la prosess fortsette med neste fil.
		FFmpegPercentage = 0;

		// Skroll nedover.
		SendMessage(TextReportHwnd, WM_VSCROLL, SB_LINEDOWN, 0);
	}
	gMutexLock.unlock();
}