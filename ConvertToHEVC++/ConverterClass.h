#pragma once

//
// Relaterte globale variabler for hovedtråd. 
//
djDBI db;
std::wstring FileBeingWorkedOn = L"";
int FileBeingWorkedOnNum = 0;
int FerdigKonvertertAntall = 0;	
bool HasCompletedAQueue = false;
bool AcceptBiggerFiles = false;
bool DetectedFailedErrEver = false;
bool DetectedFailedBigEver = false;
bool DetectedFailedConversionSingleRoundERR = false;
bool DetectedFailedConversionSingleRoundBIG = false;
// HistoricStatClass HistoricalStats;
unsigned long long TotaleBytesBehandlet = 0;
unsigned long long TotaleBytesInnspart = 0;
unsigned long long TotaleBytesSomSkalBehandles = 0;
unsigned long long drTotaleBytesBehandlet = 0;	// Denne runden.
unsigned long long drTotaleBytesInnspart = 0;	// Denne runden.
unsigned long long drTotaleBytesKonvertert = 0; // Denne runden.

//
// ConvertHEVC - Klasse som igjen bruker ProcessPipeClass.
// Starter og overser FFmpeg prosessene som skal kjøres.
// Kommuniserer resultater via globale variabler til HovedLoop.
// Tar seg også av navnebytte på undertekstfiler.
//
class ConvertHEVC
{
	private: 
	ProcessPipeClass PPC;

	public:
	// Tar seg av filavhending til egne prosesser i rekkefølge.
	void Start(
		std::vector<std::wstring>* MediaFiles,
		std::vector<std::wstring>* Subtitles,
		std::wstring Skalering, 
		std::wstring Kvalitet
	){
		for (int n=0; n<(int)MediaFiles->size(); n++) {

			// Fortell hovedtråd hvilken fil det jobbes med.
			FileBeingWorkedOn = (*MediaFiles)[n];
			FileBeingWorkedOnNum += 1;

			// Hopp over hvis filen ikke lenger finnes.
			if (!MiscStaticFuncsClass::FileExistsW((*MediaFiles)[n].c_str()))
				continue;

			std::wstring exefile, params, scaling, outfile, mediaext;
			unsigned long long pos = 0;

			// Skalering.
			if (Skalering == L"No Scaling") {
				scaling = L"";
			} else {
				scaling = L"-vf scale_cuda=-2:";
				scaling.append(Skalering.substr(Skalering.find(L'x')+1));
			}

			// Sjekk om fil allerede har riktig endelse. Ta det evt. vekk for ny behandling.
			if (
				(pos = (*MediaFiles)[n].find(L".HBYT.mp4")) != std::wstring::npos || 
				(pos = (*MediaFiles)[n].find(L".HBYT.MP4")) != std::wstring::npos || 
				(pos = (*MediaFiles)[n].find(L".HBYT.mkv")) != std::wstring::npos ||
				(pos = (*MediaFiles)[n].find(L".HBYT.MKV")) != std::wstring::npos
			) {
				// Slett fila hvis det finnes en til fil med navn uten.
				// Det indikerer at det er en avbrutt/uferdig behandlet fil.
				outfile = (*MediaFiles)[n].substr(0,pos) + (*MediaFiles)[n].substr(pos+5,4);

				// Sjekk om orginalnavnet allerede finnes i registeret over filer som skal behandles.
				bool SameFileExistsTwice = false;
				for (int n2=0; n2<(int)(*MediaFiles).size();n2++) {
					if ((*MediaFiles)[n2].compare(outfile) == 0) {
						SameFileExistsTwice = true;
						break;
					}
				}
				if (SameFileExistsTwice)
					continue;

				// Forandre selve filen hvis det er en alenestående HBYT fil.
				MoveFileW((*MediaFiles)[n].c_str(), outfile.c_str());
				(*MediaFiles)[n].replace(pos,5,L"");
			}

			// Filnavn trenger HBYT tag.
			if ((pos = (*MediaFiles)[n].find(L".mp4")) != std::wstring::npos) 
				outfile = (*MediaFiles)[n].substr(0,pos) + L".HBYT.mp4";
			else if ((pos = (*MediaFiles)[n].find(L".MP4")) != std::wstring::npos) 
				outfile = (*MediaFiles)[n].substr(0,pos) + L".HBYT.mp4";
			else if ((pos = (*MediaFiles)[n].find(L".mkv")) != std::wstring::npos) 
				outfile = (*MediaFiles)[n].substr(0,pos) + L".HBYT.mkv";
			else if ((pos = (*MediaFiles)[n].find(L".MKV")) != std::wstring::npos) 
				outfile = (*MediaFiles)[n].substr(0,pos) + L".HBYT.mkv";
			

			// Bytt ut h264 betegnelser med HEVC.
			// HUSK. find_last_of leter etter karakterer, ikke full streng.
			//       bruk rfind for å finne siste strenger av noe.
			unsigned long long FilenameStartPos = outfile.find_last_of('\\')+1;

			// Pass på at siste funn er etter starten på filnavn og ikke mappe.
			if ((pos = outfile.rfind(L"x264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,4,L"HEVC");
			} else if ((pos = outfile.rfind(L"x.264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,5,L"HEVC");
			} else if ((pos = outfile.rfind(L"X264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,4,L"HEVC");
			} else if ((pos = outfile.rfind(L"X.264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,5,L"HEVC");
			} else if ((pos = outfile.rfind(L"h.264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,5,L"HEVC");
			} else if ((pos = outfile.rfind(L"H264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,4,L"HEVC");
			} else if ((pos = outfile.rfind(L"h264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,4,L"HEVC");
			} else if ((pos = outfile.rfind(L"H.264")) != std::wstring::npos) {
				if (FilenameStartPos < pos)
					outfile.replace(pos,5,L"HEVC");
			}

			// Programfil.
			exefile = std::wstring(ExePath).substr(0,std::wstring(ExePath).find_last_of(L'\\')+1) +L"ffmpeg.exe";

			// Parametre til program.
			params = L" "
				"-y "
				"-hwaccel cuda "
				"-hwaccel_output_format cuda "
				"-i \""+ (*MediaFiles)[n] +L"\" "+
				scaling +L" "
				"-map 0 -map -v -map V "
				"-max_muxing_queue_size 8192 "
				"-c copy -c:v hevc_nvenc "
				"-preset "+ Kvalitet +L" "+
				L"\""+ outfile +L"\"";
			
			// (Beholdes for debugging av parametere).
			//MiscStaticFuncsClass::BeskjedW(params.c_str());
			PPC.CreateAndRunChildProcess(exefile, params);
			
			// Oppdater total fremgang for hovedtråd.
			FerdigKonvertertAntall += 1;

			// Hopp over videre behandling hvis filen allerede var i HEVC format.
			if (HEVCFoundThisRound) {
				HEVCFoundThisRound = false;
				HEVCExistingFilesFound += 1; 

				// Tagg filen så den ikke blir behandlet mer.
				// Vent litt på at drept tråd skal slippe filen.
				Sleep(500);

				// En tidligere runde kan ha oppretta en 0-byte fil.
				// Denne vil i så tilfelle få MoveFileW til å feile.
				DeleteFileW(outfile.c_str()); 
				
				// Vent til ffmpeg har sluppet filen og flytt til nytt navn.
				while(MoveFileW((*MediaFiles)[n].c_str(), outfile.c_str()) == 0)
					Sleep(250);

				// Lås prosent, siden hovedtråd også modifiserer den.
				gMutexLock.lock();
				FFmpegPercentage = -1;
				gMutexLock.unlock();

				// Avvent til hovedtråd har satt prosent fra -1 til 0 før neste runde.
				while (FFmpegPercentage == -1) 
					Sleep(5);

				continue;
			}

			//
			// Sjekk filstørrelser for statistikk og slett gammel fil hvis den 
			// er større enn orginal. Hvis ikke tagger vi gammel med HBYT og 
			// fjerner heller den nye. Da vil ikke den gamle bli behandlet igjen.
			// Ingen statistikk blir oppdatert heller hvis den gamle beholdes.
			//
			unsigned long long OldSize = MiscStaticFuncsClass::FileSizeInBytesW((*MediaFiles)[n].c_str());
			unsigned long long NewSize = MiscStaticFuncsClass::FileSizeInBytesW(outfile.c_str());

			// Hvis prosent spart utgjør mer enn 99% så antar vi at det skjedde noe feil.
			if ((100-(int)(((float)NewSize/(float)OldSize)*100)) < 99) {

				// Lagre for sluttrapport kun for denne runden.
				// Se forskjell på god konvertering og [Bigger].
				if (OldSize < NewSize && !AcceptBiggerFiles) {

					// Ny er større enn gammel.
					DeleteFileW(outfile.c_str());
					MoveFileW((*MediaFiles)[n].c_str(), outfile.c_str());
					DetectedFailedConversionSingleRoundBIG = true;
					DetectedFailedBigEver = true;

				} else {
					// Gammel er større en ny.
					for (int n2=0; n2<=20; n2++) {
						Sleep(250);

						// Prøv opp til 10 ganger å slette gammel fil. 
						if (DeleteFileW((*MediaFiles)[n].c_str()) != 0)
							break;
					}

					drTotaleBytesBehandlet += OldSize;
					drTotaleBytesInnspart += (NewSize>OldSize ? 0 : (OldSize-NewSize));
					drTotaleBytesKonvertert += NewSize;
				
					// Lagre statistikk i databasen.
					if (drTotaleBytesBehandlet > 0) {
						db.Open(DbPathA.c_str());

						// Oppgrader databasen med kolonnen WorkDoneAt dersom den ikke finnes fra før.
						// Fra 21.04.2021 iht. programoppdatering for å spore brainfarts (>ULL registreringer).
						db.UpgradeQueryExecute(L"ALTER TABLE statistics ADD COLUMN WorkDoneAt DATETIME default NULL");

						// Selvreparasjon.
						// Slett evt. oppføringer som har saved feltet på en enkelt rad på over 15TB bytes. 
						// Betyr mest sannsynlig brainfart som Ole opplevde hvor unsigned long long ble registrert.
						db.QueryExecute(L"DELETE FROM statistics WHERE TotalBytesSaved > 16492674416640");

						// Utfør spørring.
						std::wstring LogBytesQuery = L""
							L"INSERT INTO statistics "
							L"(TotalBytesProcessed,TotalBytesSaved,WorkDoneAt) "
							L"VALUES "
							L"("+ std::to_wstring(OldSize) +L","+ std::to_wstring((NewSize>OldSize ? 0 : (OldSize-NewSize))) +L",'"+ MiscStaticFuncsClass::DatotidFullW() +L"')";
						db.QueryExecute(LogBytesQuery.c_str());
						db.Close();
					}
				} 

				// Oppdater evt. tilhørende subtitles.
				for (int n2=0; n2<(*Subtitles).size(); n2++) {

					// Finnes orginalt medianavn uten ext i subtitle navnet?
					if ((pos = (*Subtitles)[n2].find((*MediaFiles)[n].substr(0,(*MediaFiles)[n].length()-4))) != std::wstring::npos) {
						
						// Lagre det som kommer etter medianavnet, sånn at vi kan legge til etterpå.
						std::wstring LastSubPart = (*Subtitles)[n2].substr(pos+((*MediaFiles)[n].length()-4));
						
						// Setter sammen nytt navn.
						std::wstring NewSubname = L""+
							outfile.substr(0,outfile.find(L".HBYT.")+5) +
							LastSubPart;

						// Bytt navn.
						MoveFileW((*Subtitles)[n2].c_str(), NewSubname.c_str());
					}
				}

			// Ny fil har 0 bytes = noe galt skjedde.
			} else {
				DeleteFileW(outfile.c_str());
				//if ((pos = outfile.find(L"HEVC")) != std::wstring::npos) {
				//	outfile.replace(pos,4,L"ORIG");
				//}
				//MoveFileW((*MediaFiles)[n].c_str(), outfile.c_str());
				DetectedFailedConversionSingleRoundERR = true;
				DetectedFailedErrEver = true;
			}

			// Lås prosent, siden hovedtråd også modifiserer den.
			gMutexLock.lock();
			FFmpegPercentage = -1;
			gMutexLock.unlock();

			// Avvent til hovedtråd har satt prosent fra -1 til 0 før neste runde.
			while (FFmpegPercentage == -1)
				Sleep(5);
		}

		HasCompletedAQueue = true;
	}
};