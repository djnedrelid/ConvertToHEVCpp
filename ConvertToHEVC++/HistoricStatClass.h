#pragma once

class HistoricStatClass
{
	public:

	// Fylles med rad-data fra databasen, logget per kjøring.
	std::vector<unsigned long long> ProcessedBytes, SavedBytes;

	// Regn ut innspart prosent.
	std::wstring GetSavedPercentage()
	{
		
	}

	// Teller opp og sorterer antall bytes ved referanse, returnert i en streng.
	std::wstring GetBytesAsHumanString(std::vector<unsigned long long> &Bytes)
	{
		// Holdere for datastørrelser.
		unsigned long long B=0, KB=0, MB=0, GB=0, TB=0, tmpBytes=0, tmpBig=0;

		// Løp gjennom hver rad/konverteringsjobb som er registrert.
		for (int n=0; n<Bytes.size(); n++) {
			
			// Legg til overskytende bytes fra forrige runde.
			tmpBytes = Bytes[n] + B;
			B = 0;

			// Registrer antall bytes i respektive størrelser.
			if (tmpBytes >= (1024ULL*1024ULL*1024ULL*1024ULL)) { // TB.
				tmpBig = tmpBytes / (1024ULL*1024ULL*1024ULL*1024ULL);
				TB += tmpBig;
				B = tmpBytes - (tmpBig * (1024ULL*1024ULL*1024ULL*1024ULL));

			} else if (tmpBytes >= (1024*1024*1024)) { // GB.
				tmpBig = tmpBytes / (1024*1024*1024);
				GB += tmpBig;
				B = tmpBytes - (tmpBig * (1024*1024*1024));

				// DEBUGGING
				/*
				std::wstring tmpBeskjed = L""+ std::to_wstring(tmpBytes) +L" >= "+ std::to_wstring((1024*1024*1024)) +L"\n"
					L""+ std::to_wstring(tmpBig) +L" = "+ std::to_wstring(tmpBytes) +L" / "+ std::to_wstring((1024*1024*1024)) +L";\n"
					L""+ std::to_wstring(GB) +L" += "+ std::to_wstring(tmpBig) +L";\n"
					L""+ std::to_wstring(B) +L" = "+ std::to_wstring(tmpBytes) +L" - ("+ std::to_wstring(tmpBig) +L" * "+ std::to_wstring((1024*1024*1024)) +L");";

				MiscStaticFuncsClass::BeskjedW(tmpBeskjed.c_str());
				*/

			} else if (tmpBytes >= (1024*1024)) { // MB.
				tmpBig = tmpBytes / (1024*1024);
				MB += tmpBig;
				B = tmpBytes - (tmpBig * (1024*1024));

			} else if (tmpBytes >= 1024) { // kB.
				tmpBig = tmpBytes / 1024;
				KB += tmpBig;
				B = tmpBytes - (tmpBig * 1024);

			} else { // B.
				B = tmpBytes;
			}
		}

		// Flyt opp alle restverdier.
		while (B > 1024) {
			KB += 1;
			B -= 1024;
		}

		while (KB >= 1024) {
			MB += 1;
			KB -= 1024;
		}

		while (MB >= 1024) {
			GB += 1;
			MB -= 1024;
		}

		while (GB >= 1024) {
			TB += 1;
			GB -= 1024;
		}
		

		// Formater og returner streng som representerer totale antall bytes.
		std::wstring r = L"";
		std::wstring tmp = L"";

		if (TB > 0) {
			tmp = std::to_wstring((float)((float)GB/1024));
			r = std::to_wstring(TB) + tmp.substr(tmp.find_first_of(L",."),3);
			r += L" TB";

			// DEBUGGING
			//r += L" TB, "+ std::to_wstring(GB) +L"GB, "+ std::to_wstring(MB) +L"MB, "+std::to_wstring(KB) +L"KB, "+std::to_wstring(B) +L"B. ("+ std::to_wstring(Bytes.size()) +L")";

		} else if (GB > 0) {
			tmp = std::to_wstring((float)(MB/1024));
			r = std::to_wstring(GB) + tmp.substr(tmp.find_first_of(L",."),3);
			r += L" GB";

		} else if (MB > 0) {
			tmp = std::to_wstring((float)(KB/1024));
			r = std::to_wstring(MB) + tmp.substr(tmp.find_first_of(L",."),3);
			r += L" MB";

		} else if (KB > 0) {
			tmp = std::to_wstring((float)(B/1024));
			r = std::to_wstring(KB) + tmp.substr(tmp.find_first_of(L",."),3);
			r += L" KB";

		} else {
			r = std::to_wstring(B) + L" B";
		}

		return r;
	}
};