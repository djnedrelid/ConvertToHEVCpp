#pragma once
#include "sqlite3.h"

/*
	djDBI for C/C++ programmer.
	©2020 Dag J Nedrelid <thronic@gmail.com>

	Klasse og struktur for å benytte SQLITE3.
	Støtter tekst, tall og flyt/reelle tall.
	Lagrer vanligvis binære data på andre vis.
	Tekst typen brukes jo også for datotider.

	Hvordan:
	main.cpp:
		djDBI db;
		db.Open("fil.db");
		
		// INSERT, UPDATE, DELETE
		void db.QueryExecute(const wchar_t* Query);

		// SELECT
		void db.QuerySelect(const wchar_t* Query);

		// Loop gjennom rader og kolonner.
		for (int n=0; n<db.Rows.size(); n++) {
			// Rows[n] inneholder dbdata strukturer.
			// Rows[n][kolonnenummer] for å hente:
			// _text, _real, _integer (alle har også _type verdi).
			// Sjekk dbdata strukturen for typeinformasjon.

			// Det er enklest å vite på forhånd hva slags type man henter.
			// Klassen tar seg av det automatisk via typegjenkjenning.
		}

		// Lukk og rydd opp.
		db.Close();

	Testprosjektet benytter seg av amalgamation versjonen av sqlite3.c og sqlite3.h 
	i prosjektet for å fungere. Det fungerte å bygge i både x64 og x86 konfigurasjon. 
	På den måten slipper man ekstra DLL eller andre eksterne avhengigheter.

	https:www.sqlite.org/download.html (amalgamation source).
*/

//
// Struktur med typene vi trenger.
//
struct dbdata
{
	std::wstring _text = L"";
	float _real = 0.0f;
	long long _integer = 0;
	int _type = 0;

	/*	Typenummer:
 		SQLITE_INTEGER  1
		SQLITE_FLOAT    2
		SQLITE_NULL     5
		SQLITE3_TEXT    3
	*/
};

class djDBI
{
	private:
	sqlite3* DB;		// Databaseobjekt.
	sqlite3_stmt *STMT;	// Uttrykksobjekt.

	public:

	// Rad samling etter hver QuerySelect().
	std::vector<std::vector<dbdata>> Rows;

	// Funksjon for åpning av tilkobling til en SQLITE3 databasefil.
	void Open(const char* FileName)
	{
		// Åpne database.
		if (sqlite3_open_v2(FileName, &DB, SQLITE_OPEN_READWRITE, 0) != SQLITE_OK)
			GetErrorW(L"Could not open database.",true);
	}

	// Funksjon for INSERT, UPDATE og DELETE operasjoner.
	// Hvis den ikke gir kritisk feil, har alt gått bra.
	void QueryExecute(const wchar_t* Query)
	{
		// Forbered spørring.
		int ecode = -999;
		if ((ecode = sqlite3_prepare16_v2(DB, Query, -1, &STMT, 0)) != SQLITE_OK) {
			this->Close();
			GetErrorW(L"Could not prepare database statement in QueryExecute(). Code "+ std::to_wstring(ecode),true);
		}

		// Utfør spørring.
		if ((ecode = sqlite3_step(STMT)) != SQLITE_DONE) {
			this->Close();
			GetErrorW(L"Could not step database statement in QueryExecute(). Code "+ std::to_wstring(ecode),true);
		}
		
		// Rydd opp statement objektet.
		if ((ecode = sqlite3_finalize(STMT)) != SQLITE_OK) {
			this->Close();
			GetErrorW(L"Could not finalize database statement in QueryExecute(). Code "+ std::to_wstring(ecode),true);
		}
	}

	// Funksjon for oppgraderingsoperasjoner.
	// Denne skal få lov å feile som normalt dersom strukturelle forandringer allerede finnes.
	void UpgradeQueryExecute(const wchar_t* Query)
	{
		// Forbered spørring.
		int ecode = -999;
		if ((ecode = sqlite3_prepare16_v2(DB, Query, -1, &STMT, 0)) != SQLITE_OK) {
			// Ignorer.
		}

		// Utfør spørring.
		if ((ecode = sqlite3_step(STMT)) != SQLITE_DONE) {
			// Ignorer.
		}
		
		// Rydd opp statement objektet.
		if ((ecode = sqlite3_finalize(STMT)) != SQLITE_OK) {
			// Ignorer.
		}
	}

	// Funksjon for SELECT operasjoner.
	// Samler resultater i Rows vector.
	// Hvis den ikke gir kritisk feil, har alt gått bra.
	void QuerySelect(const wchar_t* s)
	{
		// Forbered spørring.
		int ecode = -999;
		if ((ecode = sqlite3_prepare16_v2(DB, s, -1, &STMT, 0)) != SQLITE_OK) {
			this->Close();
			GetErrorW(L"Could not prepare database statement in QuerySelect(). Code "+ std::to_wstring(ecode),true);
		}

		// Utfør spørring.
		/*
		sqlite3_column_double	→	REAL result
		sqlite3_column_int	→	32-bit INTEGER result
		sqlite3_column_int64	→	64-bit INTEGER result
		sqlite3_column_text	→	UTF-8 TEXT result
		sqlite3_column_text16	→	UTF-16 TEXT result
		sqlite3_column_value	→	The result as an unprotected sqlite3_value object. 
		sqlite3_column_bytes	→	Size of a BLOB or a UTF-8 TEXT result in bytes
		sqlite3_column_bytes16  	→  	Size of UTF-16 TEXT in bytes
		sqlite3_column_type	→	Default datatype of the result

		const void *sqlite3_column_blob(sqlite3_stmt*, int iCol);
		double sqlite3_column_double(sqlite3_stmt*, int iCol);
		int sqlite3_column_int(sqlite3_stmt*, int iCol);
		sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int iCol);
		const unsigned char *sqlite3_column_text(sqlite3_stmt*, int iCol);
		const void *sqlite3_column_text16(sqlite3_stmt*, int iCol);
		sqlite3_value *sqlite3_column_value(sqlite3_stmt*, int iCol);
		int sqlite3_column_bytes(sqlite3_stmt*, int iCol);
		int sqlite3_column_bytes16(sqlite3_stmt*, int iCol);
		int sqlite3_column_type(sqlite3_stmt*, int iCol);

		If the result is a BLOB or a TEXT string, then the 
		sqlite3_column_bytes() or sqlite3_column_bytes16() 
		interfaces can be used to determine the size (without \0).

		Strings returned by sqlite3_column_text() and sqlite3_column_text16(), 
		even empty strings, are always zero-terminated. The return value from 
		sqlite3_column_blob() for a zero-length BLOB is a NULL pointer.

		Typenummer:
 		SQLITE_INTEGER  1
		SQLITE_FLOAT    2
		SQLITE_BLOB     4
		SQLITE_NULL     5
		SQLITE3_TEXT    3
		*/

		// Nullstill liste og legg inn ny data.
		Rows.clear();
		while (sqlite3_step(STMT) == SQLITE_ROW) {

			// Opprett midlertidig rad-vektor.
			std::vector<dbdata> tmpRow;

			for (int n=0; n<sqlite3_column_count(STMT); n++) {

				// Sjekk type for alle kolonner og legg til et dbdata objekt i den 
				// midlertidige raden som kan representere hvilken som helst type.
				dbdata dbd;
				int coltype = sqlite3_column_type(STMT, n);
				switch (coltype) 
				{	
					case SQLITE_INTEGER:
						dbd._type = coltype;
						dbd._integer = sqlite3_column_int64(STMT,n);
						break;

					case SQLITE_FLOAT:
						dbd._type = coltype;
						dbd._real = (float)sqlite3_column_double(STMT,n);
						break;

					case SQLITE3_TEXT:
						dbd._type = coltype;
						dbd._text = std::wstring((const wchar_t*)sqlite3_column_text16(STMT,n));
						break;

					case SQLITE_NULL:
						dbd._type = 0;
						break;
				}

				// Legg til kolonnedata til midlertidig rad.
				tmpRow.push_back(dbd);
			}

			// Legg til nyopprettet rad i rows.
			Rows.push_back(tmpRow);
		}
		
		// Rydd opp statement objektet.
		if ((ecode = sqlite3_finalize(STMT)) != SQLITE_OK) {
			this->Close();
			GetErrorW(L"Could not finalize database statement in QuerySelect(). Code "+ std::to_wstring(ecode),true);
		}
	}

	// Funksjon for lukking av tilkobling.
	void Close()
	{
		sqlite3_close_v2(DB);
	}	

	//
	// lpszFunction = Manuell feilbeskjed.
	// HandleExit = Exit(EXIT_FAILURE).
	//
	//	https://www.sqlite.org/rescode.html // SQLITE3 feilkoder.
	//
	private:
	static void GetErrorW(std::wstring lpszFunction, bool HandleExit)
	{
		unsigned long err = GetLastError();
		std::wstring lpDisplayBuf;
		wchar_t* lpMsgBuf;

		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&lpMsgBuf,
			0,
			NULL
		);

		lpDisplayBuf.append(lpszFunction + L"\n\n");
		lpDisplayBuf.append(L"Details: (" + std::to_wstring(err) + L") ");
		lpDisplayBuf.append(lpMsgBuf);

		MessageBoxW(
			NULL,
			(LPCWSTR)lpDisplayBuf.c_str(),
			L"Critical Message",
			MB_OK | MB_ICONINFORMATION
		);

		if (HandleExit) {
			// EXIT_FAILURE = 1. 
			// EXIT_SUCCESS = 0.
			exit(EXIT_FAILURE);
		}
	}
};