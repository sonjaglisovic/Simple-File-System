#include<cstdlib>
#include<Windows.h>
#include"kernfile.h"


KernelFS::KernelFS() {
	InitializeConditionVariable(&cond);
	InitializeSRWLock(&mutex);
	InitializeSRWLock(&mountSem);

}
char KernelFS::mount(Partition* partition) {
	AcquireSRWLockExclusive(&mountSem);
	AcquireSRWLockExclusive(&mutex);
	part = partition;
	montirana = true;
	brojKlasteraZaVektor = partition->getNumOfClusters() / (2048*8) + (partition->getNumOfClusters() % (2048*8) ? 1 : 0);
	bitVektor = new unsigned char[brojKlasteraZaVektor*2048+1];
	int i;
	for (i = 0; i < brojKlasteraZaVektor; i++) {
		while (!part->readCluster(i, (char*)(bitVektor + i*2048)));
	}
	char* temp = new char[2049];
	while (!part->readCluster(i, temp));
	Elem2* el = new Elem2((unsigned long*)temp, i);
	listaRootIndeksa.push_back(el);
	for (int j = 0; j < 512; j++) {
		listaRootIndeksa.front()->indeks[j] = 0;
    }
	ReleaseSRWLockExclusive(&mutex);
	return 1;
}
char KernelFS::unmount() {
	AcquireSRWLockExclusive(&mutex);
	if (!montirana) {
		ReleaseSRWLockExclusive(&mutex);
		return 0;
	}
	otvori = false;
	while (otvoreniFajlovi.size() != 0) {
		SleepConditionVariableSRW(&cond, &mutex, INFINITE, 0);
	}
	montirana = false;
	otvori = true;
	for (unsigned long j = 0; j < brojKlasteraZaVektor; j++) {
		while (!(part->writeCluster(j, (char*)(bitVektor + j*2048))));

	}
	delete[] bitVektor;
	bitVektor = nullptr;
	while (listaRootIndeksa.size() != 0) {
		Elem2* elem = listaRootIndeksa.front();
		while (!(part->writeCluster(elem->klaster, (char*)elem->indeks)));
        listaRootIndeksa.remove(elem);
		char*tmp = (char*)elem->indeks;
		delete[] tmp;
		delete elem;
	}
	ReleaseSRWLockExclusive(&mutex);
	ReleaseSRWLockExclusive(&mountSem);
	return 1;
}
char KernelFS::format() {
	AcquireSRWLockExclusive(&mutex);
	if (!montirana) {
		ReleaseSRWLockExclusive(&mutex);
		return 0;
	}
	otvori = false;
	while (otvoreniFajlovi.size() != 0) {
		SleepConditionVariableSRW(&cond, &mutex, INFINITE, 0);
	}
	otvori = true;
	if (!montirana) {
		ReleaseSRWLockExclusive(&mutex);
		return 0;
	}
	int i = part->getNumOfClusters() / (2048 * 8) + (part->getNumOfClusters() % (2048 * 8) ? 1 : 0) + 1;  //inicijalizacija bit vektora
	int j;
	for (j = 0; j < i / 8; i++) {
		bitVektor[j] |= 255;
    }
	bitVektor[j] |= 255;
	bitVektor[j] <<= (8 - i % 8);
	int c;
	for (c = j + 1; c < part->getNumOfClusters() / 8; c++) {
		bitVektor[c] = 0;
	}
	if (part->getNumOfClusters() % 8) {
		bitVektor[c] = 0;
	}
	
	for (int i = 0; i < 512; i++) {
		listaRootIndeksa.front()->indeks[i] = 0;
	}
	while (listaRootIndeksa.size() != 1) {
		Elem2* elem = listaRootIndeksa.back();
		listaRootIndeksa.remove(elem);
		char*tmp = (char*)elem->indeks;
		delete[] tmp;
		delete elem;
	}
   ReleaseSRWLockExclusive(&mutex);
}
FileCnt KernelFS::readRootDir() {
	AcquireSRWLockExclusive(&mutex);
	if (!montirana or !otvori) {
		ReleaseSRWLockExclusive(&mutex);
		return -1;
	}
	list<Elem2*>::iterator it;
	int broj = 0;
	for (it = ++listaRootIndeksa.begin(); it != listaRootIndeksa.end(); ++it) {
		for (int i = 0; i< 512; i++){
			if ((*it)->indeks[i] == 0)
				continue;
			char*tmp = new char[2049];
			while (!part->readCluster((*it)->indeks[i], tmp));
			int k = 0;
			Elem10* tmp1 = (Elem10*)tmp;
			for(int j = 0; j < 64; j++){
				if (tmp1[j].naziv[0] != '\0')
					broj++;
			}
			delete[] tmp;
		}
		   
	}
	ReleaseSRWLockExclusive(&mutex);
	return broj;
}
char KernelFS::doesExist(char* fname) {
	AcquireSRWLockExclusive(&mutex);
	if (!montirana) {
		ReleaseSRWLockExclusive(&mutex);
		return 0;
	}
	char retval = exists(fname); 
	ReleaseSRWLockExclusive(&mutex);
	return retval;
}
char KernelFS::deleteFile(char* fname) {
	AcquireSRWLockExclusive(&mutex);
	if (!montirana) {
		ReleaseSRWLockExclusive(&mutex);
		return 0;
	}
	char retval = brisi(fname);
	ReleaseSRWLockExclusive(&mutex);
	return retval;
}
File* KernelFS::open(char* fname, char mode) {
	AcquireSRWLockExclusive(&mutex);
	if (!montirana or (mode!='r' && mode !='a' && mode!='w') or !otvori) {
		ReleaseSRWLockExclusive(&mutex);
		return nullptr;
	}
	if (mode == 'r' || mode == 'a') {
		list<KernelFile*>::iterator it;
		for (it = otvoreniFajlovi.begin(); it != otvoreniFajlovi.end(); ++it) {
			if (strcmp(fname, (*it)->fname) == 0) {
				(*it)->brojNiti++;
				ReleaseSRWLockExclusive(&mutex);
				File*ret = (*it)->napravi();
				ret->myImpl = (*it);
				if (mode == 'a') {
					AcquireSRWLockExclusive(&ret->myImpl->rw);
					(*it)->postaviMode('a');
				}
				else {
					AcquireSRWLockShared(&ret->myImpl->rw);
					(*it)->postaviMode('r');
				}
				return ret;
			}
		}
	}
	char c = exists(fname);
	if (!c && (mode == 'a' || mode == 'r')) {
		ReleaseSRWLockExclusive(&mutex);
		return nullptr;
	}
	if (mode == 'w' && c) {
		if (!brisi(fname)) {
			ReleaseSRWLockExclusive(&mutex);
			return nullptr;
		}
	}
	if (mode == 'w') {
		El* el = new El();
		if (!nadjiIliNapravi(fname, -1, 'w', el)) {
			ReleaseSRWLockExclusive(&mutex);
			return 0;
		}
		char* tmp = new char[2048];
		while (!(part->readCluster(el->indeksPrvogNivoa, tmp)));
		unsigned long* u = (unsigned long*)tmp;
		for (int i = 0; i < 512; i++) {
			u[i] = 0;
		}
		while (!(part->writeCluster(el->indeksPrvogNivoa, tmp)));
		delete[] tmp;
		KernelFile* kf = new KernelFile(el->vel, el->indeksPrvogNivoa, part, mode, fname);
		File*ret = kf->napravi();
		ret->myImpl = kf;
		delete el;
		otvoreniFajlovi.push_back(kf);
		AcquireSRWLockExclusive(&ret->myImpl->rw);
		ReleaseSRWLockExclusive(&mutex);
		return ret;
	}
	if (c && (mode == 'a' || mode == 'r')) {
		if (mode == 'a') {
			El* el = new El();
			if (!nadjiIliNapravi(fname, -1, 'a', el)) {
				ReleaseSRWLockExclusive(&mutex);
				return 0;
			}
			KernelFile* kf = new KernelFile(el->vel,el->indeksPrvogNivoa, part, mode, fname);
			File* ret = kf->napravi();
			ret->myImpl = kf;
			delete el;
			otvoreniFajlovi.push_back(kf);
			AcquireSRWLockExclusive(&ret->myImpl->rw);
			ReleaseSRWLockExclusive(&mutex);
			return ret;
		}
		else {
			El* el = new El();
			if (!nadjiIliNapravi(fname, -1, 'r', el)) {
				return 0;
			}
			KernelFile*kf = new KernelFile(el->vel, el->indeksPrvogNivoa, part, mode, fname);
			File* ret = kf->napravi();
			ret->myImpl = kf;
			delete el;
			otvoreniFajlovi.push_back(kf);
			AcquireSRWLockShared(&ret->myImpl->rw);
			ReleaseSRWLockExclusive(&mutex);
			return ret;
		}
	}
}
char KernelFS::nadjiIliNapravi(char*fname, int velicina, char mode, El* ret) {
	if (mode == 'w') {
		char ext[3];
		char fn[8];
		int ii;
		for (ii = 1; fname[ii] != '.'; ii++) {
			fn[ii - 1] = fname[ii];
		}
		for (int jj = ii - 1; jj < 8; jj++) {
			fn[jj] = '\0';
		}
		int jj;
		for (jj = 0; jj < 3; jj++) {
			if (fname[ii] == '\0')
				break;
			ext[jj] = fname[ii + 1 + jj];
		}
		while (jj < 3) {
			ext[jj] = '\0';
			jj++;
		}
		list<Elem2*>::iterator it;
		int ind = 0;
		for (it = ++listaRootIndeksa.begin(); it != listaRootIndeksa.end(); it++) {
			for (int i = 0; i < 512; i++) {
				if ((*it)->indeks[i] == 0) {
					(*it)->indeks[i] = nadjiBlok();
					char*t = new char[2049];
					while (!part->readCluster((*it)->indeks[i], t));
					Elem10*t1 = (Elem10*)t;
					for (int j2 = 0; j2 < 8; j2++) {
						t1[0].naziv[j2] = fn[j2];
					}
					for (int i1 = 0; i1 < 3; i1++) {
						t1[0].eksetenzija[i1] = ext[i1];
					}
					t1[0].ch = '\0';
					t1[0].prviIndeks = nadjiBlok();
					if (t1[0].prviIndeks == 0) {
						return 0;
					}
					for (int in = 1; in < 64; in++) {
						t1[in].naziv[0] = '\0';
					}
					t1[0].velicinaFajla = 0;
					while (!part->writeCluster((*it)->indeks[i], t));
					ret->vel = t1[0].velicinaFajla;
					ret->indeksPrvogNivoa = t1[0].prviIndeks;
					delete[] t;
					return 1;
                }
				char*tmp = new char[2049];
				while (!part->readCluster((*it)->indeks[i], tmp));
				Elem10* el = (Elem10*)tmp;
				for (int j = 0; j < 64; j++) {
					if (el[j].naziv[0] == '\0') {
						for (int j2 = 0; j2 < 8; j2++) {
							el[j].naziv[j2] = fn[j2];
						}
						for (int i1 = 0; i1 < 3; i1++) {
							el[j].eksetenzija[i1] = ext[i1];
						}
						el[j].ch = '\0';
						el[j].prviIndeks = nadjiBlok();
						if (el[j].prviIndeks == 0) {
							return 0;
						}
						el[j].velicinaFajla = 0;
						while (!part->writeCluster((*it)->indeks[i], tmp));
						ret->vel = el[j].velicinaFajla;
						ret->indeksPrvogNivoa = el[j].prviIndeks;
						delete[] tmp;
						return 1;
					}
				}
				delete[] tmp;
			}
		}
		if (listaRootIndeksa.size() < 513) {
			Elem2* el2 = new Elem2();
			el2->klaster = nadjiBlok();
			char*tmpp = new char[2049];
			while (!part->readCluster(el2->klaster, tmpp));
			el2->indeks = (unsigned long*)tmpp;
			for (int y = 0; y < 512; y++) {
				if (listaRootIndeksa.front()->indeks[y] == 0) {
					listaRootIndeksa.front()->indeks[y] = el2->klaster;
					break;
				}
			
			}
			el2->indeks[0] = nadjiBlok();
			if(el2->indeks[0] == 0){
				return 0;
            }
			for (int k = 1; k < 512; k++) {
				el2->indeks[k] = 0;
			}
			char* tmp = new char[2049];
			while (!part->readCluster(el2->indeks[0], tmp));
			Elem10* temp = (Elem10*)tmp;
			for (int j2 = 0; j2 < 8; j2++) {
				temp[0].naziv[j2] = fn[j2];
			}
			for (int i1 = 0; i1 < 3; i1++) {
				temp[0].eksetenzija[i1] = ext[i1];
			}
			temp[0].ch = '\0';
			temp[0].prviIndeks = nadjiBlok();
			if (temp[0].prviIndeks == 0) {
				return 0;
			}
			temp[0].velicinaFajla = 0;
			listaRootIndeksa.push_back(el2);
			ret->vel = temp[0].velicinaFajla;
			ret->indeksPrvogNivoa = temp[0].prviIndeks;
			for (int xy = 1; xy < 64; xy++) {
				temp[xy].naziv[0] = '\0';
			}
			while (!part->writeCluster(el2->indeks[0], tmp));
			delete[] tmp;
			return 1;
		}
		else {
			return 0;
		}
	}
	char ext[3];
	char fn[8];
	int ii;
	for (ii = 1; fname[ii] != '.'; ii++) {
		fn[ii - 1] = fname[ii];
	}
	for (int jj = ii - 1; jj < 8; jj++) {
		fn[jj] = '\0';
	}
	int jj;
	for (jj = 0; jj < 3; jj++) {
		if (fname[ii] == '\0')
			break;
		ext[jj] = fname[ii + 1 + jj];
	}
	while (jj < 3) {
		ext[jj] = '\0';
		jj++;
	}
	list<Elem2*>::iterator it;

	for (it = ++listaRootIndeksa.begin(); it != listaRootIndeksa.end(); it++) {
		for (int i = 0; i < 512; i++) {
			if ((*it)->indeks[i] == 0) continue;
			char*tmp = new char[2049];
			while (!part->readCluster((*it)->indeks[i], tmp));
			Elem10* el = (Elem10*)tmp;
			for (int j = 0; j < 64; j++) {
				int j1;
				for (j1 = 0; j1 < 8; j1++) {
					if (el[j].naziv[j1] != fn[j1])
						break;
				}
				if (j1 == 8) {
					int j2;
					for (j2 = 0; j2 < 3; j2++) {
					  if (el[j].eksetenzija[j2] != ext[j2])
							break;
					}
					if (j2 == 3) {
						if (mode == 'b' && velicina > 0) {
							el[j].velicinaFajla = velicina;
							while (!part->writeCluster((*it)->indeks[i], tmp));
						}
						else {
							ret->vel = el[j].velicinaFajla;
							ret->indeksPrvogNivoa = el[j].prviIndeks;
						}
						delete[] tmp;
						return 1;
					}
				}
				
			}
			delete[] tmp;
		}
	}
	return 0;
}
char KernelFS:: brisi(char* fname) {
	list<KernelFile*>::iterator it;
	for (it = otvoreniFajlovi.begin(); it != otvoreniFajlovi.end(); ++it) {
		if (strcmp(fname, (*it)->fname) == 0) {
			return 0;
		}
	}
	char ext[3];
	char fn[8];
	int ii;
	for (ii = 1; fname[ii] != '.'; ii++) {
		fn[ii - 1] = fname[ii];
	}
	for (int jj = ii - 1; jj < 8; jj++) {
		fn[jj] = '\0';
	}
	int jj;
	for (jj = 0; jj < 3; jj++) {
		if (fname[ii] == '\0')
			break;
		ext[jj] = fname[ii + 1 + jj];
	}
	while (jj < 3) {
		ext[jj] = '\0';
		jj++;
	}
    list<Elem2*>::iterator it1;
	for (it1 = ++listaRootIndeksa.begin(); it1 != listaRootIndeksa.end(); ++it1) {
		for (int i = 0; i< 512; i++) {
			if ((*it1)->indeks[i] == 0)
				continue;
			char*tmp= new char[2049];
			while (!part->readCluster((*it1)->indeks[i], tmp));
			int k = 0;
			Elem10* tmp1 = (Elem10*)tmp;
			for (k = 0; k < 64; k++) {
				int j;
				for (j = 0; j < 8; j++) {
					if (tmp1[k].naziv[j] != fn[j])
						break;
				}
				if (j == 8) {
					int c;
					for (c = 0; c < 3; c++) {
						if (tmp1[k].eksetenzija[c] != ext[c]) {
							break;
						}
					}
					if (c == 3) {
						for (int k1 = 0; k1 < 8; k1++) {
							tmp1[k].naziv[k1] = '\0';

						}
						unsigned long l = tmp1[k].prviIndeks;
						char*tmpp = new char[2049];
						while (!part->readCluster(l, tmpp));
						unsigned long* ul = (unsigned long*)tmpp;
						for (int i5 = 0; i5 < 512; i5++) {
							if (ul[i5] == 0)
								continue;
							char*tmpp1 = new char[2049];
							while (!part->readCluster(ul[i], tmpp1));
							unsigned long*ul1 = (unsigned long*)tmpp1;
							for (int j5 = 0; j5 < 512; j5++) {
								if (ul1[j5] != 0) {
									oslobodiBlok(ul1[j5]);
								}
							}
							delete[] tmpp1;
							oslobodiBlok(ul[i5]);
                        }
						oslobodiBlok(l);
						delete[] tmpp;
						int i1;
						for (i1 = 0; i1 < 64; i1++) {
							int i2;
							for (i2 = 0; i2 < 8; i2++) {
								if (tmp1[i1].naziv[i2] != '\0')
									break;
							}
							if (i2 < 8)
								break;
						}
						if (i1 == 64) {
							oslobodiBlok((*it1)->indeks[i]);
							(*it1)->indeks[i] = 0;
						}
						while (!part->writeCluster((*it1)->indeks[i], tmp));
						return 1;
					}
				}
			}
			delete[] tmp;
		}
	}
	return 0;
}
int KernelFS::nadjiBlok() {
	int c = part->getNumOfClusters() / 8 + (part->getNumOfClusters() % 8 ? 1 : 0);
	for (int i = 0; i < c; i++) {
		//if (i == 1874)
			//cout << "SRANJEEE" << endl;
		unsigned int c1 = (unsigned int)bitVektor[c - 1];
		if (bitVektor[i] != 255) {
			unsigned char temp = 128;
			int k = 0;
			while (temp & bitVektor[i]) {
				k++;
				temp >>= 1;
			}
			bitVektor[i] |= temp;
			return i * 8 + k;
		}
	}
	return 0;
}
char KernelFS::oslobodiBlok(int brBloka) {
	if (brBloka < 0 || brBloka > part->getNumOfClusters())
		return 0;
	int pozicija = brBloka/8;
	int offset = brBloka % 8;
	unsigned char maska = (unsigned char)255 >> (offset + 1);
	unsigned char ma2 = (unsigned char)255 << (8 - offset);
	unsigned char msk = maska | ma2;
	bitVektor[pozicija] &= msk;
	return 1;
}
char KernelFS::exists(char* fname) {
	char ext[3];
	char fn[8];
	int ii;
	for (ii = 1;  fname[ii] != '.'; ii++) {
		fn[ii - 1] = fname[ii];
	}
	for (int jj = ii - 1; jj < 8; jj++) {
		fn[jj] = '\0';
	}
	int jj;
	for (jj = 0; jj < 3; jj++) {
		if (fname[ii] == '\0')
			break;
		ext[jj] = fname[ii + 1 + jj];
	}
	while (jj < 3) {
		ext[jj] = '\0';
		jj++;
	}
    list<Elem2*>::iterator it;
	for (it = ++listaRootIndeksa.begin(); it != listaRootIndeksa.end(); ++it) {
		for (int i = 0; i < 512; i++) {
			if ((*it)->indeks[i] == 0)
				continue;
			char*tmp = new char[2049];
			while (!part->readCluster((*it)->indeks[i], tmp));
			int k = 0;
			Elem10* tmp1 = (Elem10*)tmp;
			for (k = 0; k < 64; k++) {
				int j;
				for (j = 0; j < 8; j++) {
					if (tmp1[k].naziv[j] != fn[j])
						break;
				}
				if (j == 8) {
					int c;
					for (c = 0; c < 3; c++) {
						if (tmp1[k].eksetenzija[c] != ext[c]) {
							break;
						}
					}

					if (c == 3) {
						return 1;
					}
				}
			}
			delete[] tmp;
		}
	}
	return 0;
}









