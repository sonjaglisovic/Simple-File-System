#include"kernfile.h"


ElemK* sadrzaj[256];
int indeksZamena;

KernelFile::KernelFile(BytesCnt velicina, int indeksPrvogNivoa, Partition* particija, char mode, char* fname) {
	vel = velicina;
	char*tmp = new char[2049];
	while (!particija->readCluster(indeksPrvogNivoa, tmp));
	this->indeksPrvogNivoa = (unsigned long*)tmp;
	for (int i = 0; i < 512; i++) {
		if (this->indeksPrvogNivoa[i] != 0) {
			char* temp = new char[2049];
			while (!particija->readCluster(this->indeksPrvogNivoa[i], temp));
			Elem2* el2 = new Elem2((unsigned long*)temp, this->indeksPrvogNivoa[i]);
			indeksi.push_back(el2);
		}
	}
	ElemN* elemn = new ElemN();
	elemn->korisnik = GetCurrentThreadId();
	if (mode == 'a') {
		elemn->poz = vel;
	}
	else
	{
		elemn->poz = 0;
	}
	niti.push_back(elemn);
	this->mode = mode;
	indeksPrvi = indeksPrvogNivoa;
	this->part = particija;
	InitializeSRWLock(&mutex);
	InitializeSRWLock(&rw);
	mojFajlSistem = new FS();
	brojNiti = 1;
	this->fname = fname;
	izmenjen = false;
}
File * KernelFile::napravi() {
	return new File();
}
ElemN* KernelFile::izbaci() {
	list<ElemN*>::iterator it;
	for (it = niti.begin(); it != niti.end(); it++) {
		if ((*it)->korisnik == GetCurrentThreadId()) {
			return (*it);
		}
	}
}
KernelFile::~KernelFile(){
	while (indeksi.size() != 0) {
		Elem2* el = indeksi.back();
		indeksi.remove(el);
		while (!part->writeCluster(el->klaster, (char*)el->indeks));
		char*tmp =(char*) el->indeks;
		delete[] tmp;
		delete el;
    }
	for (int i = 0; i < 256; i++) {
		if (sadrzaj[i] != nullptr) {
			if (strcmp(sadrzaj[i]->fname, fname) == 0) {
			if(sadrzaj[i]->izmenjen)
			while (!part->writeCluster(sadrzaj[i]->klaster, sadrzaj[i]->sadrzaj));
					
			
				delete[] sadrzaj[i]->sadrzaj;
				delete sadrzaj[i];
				sadrzaj[i] = nullptr;
			}
		}
	}
	while (!part->writeCluster(indeksPrvi, (char*)indeksPrvogNivoa));
	char*tmp = (char*)indeksPrvogNivoa;
	delete[] tmp;
	delete mojFajlSistem;
}
int KernelFile::tekucaPoz() {
	list<ElemN*>::iterator it;
	for (it = niti.begin(); it != niti.end(); it++) {
		if ((*it)->korisnik == GetCurrentThreadId()) {
			return (*it)->poz;
		}
	}
	return -1;
}
void KernelFile::azurirajPoz(int cnt) {
	list<ElemN*>::iterator it;
	for (it = niti.begin(); it != niti.end(); it++) {
		if ((*it)->korisnik == GetCurrentThreadId()) {
			(*it)->poz=cnt;
		}
	}
}
char KernelFile::write(BytesCnt cnt, char* buffer) { 
	if (mode != 'w' && mode != 'a') {
		return 0;
	}
	int pozicija = tekucaPoz();
	if (pozicija == -1)
		return 0;
	if (pozicija + cnt > vel) {
		int noviIndeksi =((pozicija+cnt)/2048+((pozicija+cnt)%2048 ? 1 : 0)) - (vel / 2048 + (vel % 2048 ? 1 : 0));
		for (int i = 0; i < noviIndeksi; i++) {
			list<Elem2*>::iterator it;
			boolean alociran = false;
			for (it = indeksi.begin(); it != indeksi.end(); it++) {
				for (int j = 0; j < 512; j++) {
					if ((*it)->indeks[j] == 0) {
						
						(*it)->indeks[j] = mojFajlSistem->myImpl->nadjiBlok();
						int c = (*it)->indeks[j];
						alociran = true;
						break;
					}
				}
				if (alociran)
					break;
			}
			if (!alociran) {
				for (int i = 0; i < 512; i++) {
					if (indeksPrvogNivoa[i] == 0) {
						indeksPrvogNivoa[i] = mojFajlSistem->myImpl->nadjiBlok();
						char*tmp = new char[2049];
						while (!part->readCluster(indeksPrvogNivoa[i], tmp));
						unsigned long* ul = (unsigned long*)tmp;
						ul[0] = mojFajlSistem->myImpl->nadjiBlok();
						for (int i = 1; i < 512; i++) {
							ul[i] = 0;
						}
						alociran = true;
						Elem2* el2 = new Elem2(ul, indeksPrvogNivoa[i]);
						indeksi.push_back(el2);
						break;
					}
				}
				
			}

			if (!alociran)
				return 0;
		}
	}
	int redniBr = pozicija / 2048;
	int poslednji = (pozicija + cnt) / 2048;
	int prom = 0;
	for (int i = redniBr; i <= poslednji; i++) {
		ElemK*povratna = proveri(i, fname);
		if (i == redniBr) {                         
			for (int j = pozicija%2048; j < 2048 && prom < cnt; j++) {
				povratna->sadrzaj[j] = buffer[prom];
				povratna->izmenjen = true;
				prom++;
				
			}
			if (prom == cnt)
				break;
		}
			else {
				for (int j = 0; j < 2048 && prom < cnt; j++) {
					povratna->sadrzaj[j] = buffer[prom];
					povratna->izmenjen = true;
					prom++;
					
				}
				if (prom == cnt)
					break;
			}
	}
	if (vel < (pozicija + cnt))
		vel = pozicija + cnt;
	izmenjen = true;
	azurirajPoz(cnt+pozicija);
	return 1; 
}
void KernelFile::postaviMode(char mode) {
	this->mode = mode;
	ElemN* elemn = new ElemN();
	elemn->korisnik = GetCurrentThreadId();
	if (mode == 'a') {
		elemn->poz = vel;
	}
	else
	{
		elemn->poz = 0;
	}
	niti.push_back(elemn);
}
ElemK*KernelFile::proveri(int i, char*fname) {
	AcquireSRWLockExclusive(&mojFajlSistem->myImpl->mutex);
	for (int j = 0; j < 256; j++) {
		if (sadrzaj[j] != nullptr) {
			if (strcmp(sadrzaj[j]->fname, fname) == 0 && sadrzaj[j]->redniBr == i) {
				ReleaseSRWLockExclusive(&mojFajlSistem->myImpl->mutex);
				return  sadrzaj[j];
			}
		}
	}
	char*ret = new char[2049];
	int klaster = i / 512;
	int kl = 0;
	list<Elem2*>::iterator it;
	int j = 0;
	for (it = indeksi.begin(); it != indeksi.end(); ++it) {
		if (j == klaster) {
			kl = (*it)->indeks[i % 512];
			break;
		}
		j++;
	}
	while (!part->readCluster(kl, ret));
	ElemK* el = new ElemK();
	el->klaster = kl;
	el->redniBr = i;
	el->fname = fname;
	el->sadrzaj = ret;
	el->izmenjen = false;
	if (sadrzaj[indeksZamena] != nullptr) {
		if (sadrzaj[indeksZamena]->izmenjen) {
			while (!part->writeCluster(sadrzaj[indeksZamena]->klaster, sadrzaj[indeksZamena]->sadrzaj));
		
		}
		delete[] sadrzaj[indeksZamena]->sadrzaj;
		delete sadrzaj[indeksZamena];
		sadrzaj[indeksZamena] = nullptr;
		
	}
	sadrzaj[indeksZamena] = el;
	indeksZamena = (indeksZamena + 1) % 256;
	ReleaseSRWLockExclusive(&mojFajlSistem->myImpl->mutex);
	return el;
}
BytesCnt KernelFile::read(BytesCnt cnt, char* buffer) {
	AcquireSRWLockExclusive(&mutex);
	int pozicija = tekucaPoz();
	if (pozicija == -1)
		return 0;
	if (pozicija + cnt > vel) {
		cnt = vel - pozicija;
	}
	if (cnt == 0)
		return 0;
	int redniBr = pozicija / 2048;
	int poslednji = (pozicija + cnt) / 2048;
	int prom = 0;
	for (int i = redniBr; i <= poslednji; i++) {
		ElemK*povratna = proveri(i, fname);
		if (i == redniBr) {
			for (int j = pozicija%2048; j < 2048 && prom < cnt; j++) {
				buffer[prom] = povratna->sadrzaj[j];
				prom++;
			}
		}
		else {
			for (int j = 0; j < 2048 && prom < cnt; j++) {
				buffer[prom] = povratna->sadrzaj[j];
				char x = buffer[prom];
				prom++;
			}
		}
			
	}
	azurirajPoz(pozicija + cnt);
	ReleaseSRWLockExclusive(&mutex);
	return cnt;
}

char KernelFile::seek(BytesCnt cnt) {
	AcquireSRWLockExclusive(&mutex);
	if (cnt > vel || cnt < 0) {
		ReleaseSRWLockExclusive(&mutex);
		return 0;
	}
	int pozicija = tekucaPoz();
	if (pozicija == -1) {
		ReleaseSRWLockExclusive(&mutex);
		return 0;
	}
	azurirajPoz(cnt);
	ReleaseSRWLockExclusive(&mutex);
	return 1;
}
BytesCnt KernelFile::filePos() {
	AcquireSRWLockExclusive(&mutex);
	BytesCnt poz = tekucaPoz();
	ReleaseSRWLockExclusive(&mutex);
	return poz;
}
char KernelFile::eof() { 
	AcquireSRWLockExclusive(&mutex);
	int pozicija = tekucaPoz();
	char ret;
	if (pozicija == -1)
		ret = 1;
	if (pozicija == vel) {
		ret = 2;
	}
	else {
		ret = 0;
	}
	ReleaseSRWLockExclusive(&mutex);
	return ret;
}
BytesCnt KernelFile::getFileSize() {
	int pozicija = tekucaPoz();
	if (pozicija == -1)
		return -1;
	else
	return vel;
}
char KernelFile::truncate() { 
	int pozicija = tekucaPoz();
	if (pozicija > vel || pozicija < 0) {
		return 0;
	}
	if (mode == 'w' || mode == 'a' ) {
		int redniBr = pozicija / 2048;
		int poslednji = vel / 2048;
		for (int i = 0; i < 256; i++) {
			if (sadrzaj[i] != nullptr) {
				if (sadrzaj[i]->redniBr > redniBr && sadrzaj[i]->redniBr <= poslednji && strcmp(sadrzaj[i]->fname, fname)==0) {
					delete[] sadrzaj[i]->sadrzaj;
					delete sadrzaj[i];
					sadrzaj[i] = nullptr;
				}
			}
		}
		int klaster = redniBr / 512 + redniBr % 512 ? 1 : 0;
		int kl;
		list<Elem2*>::iterator it;
		int j = 0;
		boolean brisi = false;
		for (it = indeksi.begin(); it != indeksi.end(); ++it) {
			if (brisi) {
				for (int j = 0; j < 512; j++) {
					if ((*it)->indeks[j] != 0) {
						mojFajlSistem->myImpl->oslobodiBlok((*it)->indeks[j]);
						(*it)->indeks[j] = 0;
					}
				}
			}
			if (j == klaster) {
				brisi = true;
				for (int i = redniBr % 512; i < 512; i++) {
					if ((*it)->indeks[i] != 0) {
						mojFajlSistem->myImpl->oslobodiBlok((*it)->indeks[i]);
						(*it)->indeks[i] = 0;
					}
			    }
			}
			j++;
		}
		vel = pozicija;
		izmenjen = true;
		return 1;
	}
	return 0;
}