#ifndef _KernelFS_h_
#define _KernelFS_h_
#include "fs.h"
#include<list>
#include<iostream>
#include<Windows.h>
#include<synchapi.h>
using namespace std;

class Partition;

struct Elem2 {
	unsigned long* indeks;
	unsigned long klaster;
	Elem2() {}
	Elem2(unsigned long* ind, unsigned long kl) :indeks(ind), klaster(kl) {}
};
struct Elem10 {
	char naziv[8];
	char eksetenzija[3];
	char ch;
	unsigned long prviIndeks;
	unsigned long velicinaFajla;
	char slobodno[12];
};
class KernelFile;
class File;
class KernelFS {
public:
	~KernelFS() {}
	KernelFS();
	char mount(Partition* partition);//montira particiju
											 // vraca 0 u slucaju neuspeha ili 1 u slucaju uspeha
	char unmount(); //demontira particiju
						   // vraca 0 u slucaju neuspeha ili 1 u slucaju uspeha
	char format(); //formatira particiju;
						  // vraca 0 u slucaju neuspeha ili 1 u slucaju uspeha
	FileCnt readRootDir();
	// vraca -1 u slucaju neuspeha ili broj fajlova u slucaju uspeha
	char doesExist(char* fname); //argument je naziv fajla sa
										//apsolutnom putanjom

	File* open(char* fname, char mode);
	char deleteFile(char* fname);
	SRWLOCK mutex;
	CONDITION_VARIABLE cond;
	list<KernelFile*> otvoreniFajlovi;
private:
	friend class File;
	friend class KernelFile;
	struct El {
		int vel;
		int indeksPrvogNivoa;
		El(){}
	}; //cuva kako da updateujemo file sistem nakon upisa, vraca se samo ukoliko je mod upis inace null
	char nadjiIliNapravi(char*fname, int velicina, char mode, El* ret);
	int nadjiBlok();
	char oslobodiBlok(int brBloka);
	char brisi(char* fname);
	char exists(char* fname);
	boolean montirana = false;
	Partition* part = nullptr;
	int brojKlasteraZaVektor;
	//int brojFajlova = 0;
	list<Elem2*> listaRootIndeksa; 
	SRWLOCK mountSem;
	boolean otvori = true;
	unsigned char* bitVektor = nullptr;
};

#endif

