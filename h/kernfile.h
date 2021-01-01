#ifndef _kernfile_h_
#define _kernfile_h_
#include "file.h"
#include<Windows.h>

struct ElemK {
	char* sadrzaj;
	char*fname;
	unsigned long klaster;
	unsigned long redniBr;
	boolean izmenjen;
	ElemK():izmenjen(false), sadrzaj(nullptr){}
};
struct ElemN {
	DWORD korisnik;
	int poz;
};

//ALOKACIJA PROSTORA ZA SVE STO JE ZABORAVLJENO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
class Partition;
class KernelFS;
class KernelFile {
public:
	File* napravi();
	ElemK*proveri(int i, char*fname);
	void postaviMode(char mode);
	~KernelFile();
	char write(BytesCnt, char* buffer);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	BytesCnt filePos();
	char eof();
	BytesCnt getFileSize();
	char truncate();
	FS*mojFajlSistem;
	int brojNiti;
	char mode;
	char*fname;
	boolean izmenjen;
	int tekucaPoz();
	void azurirajPoz(int cnt);
	ElemN* izbaci();
	SRWLOCK rw;
	BytesCnt vel;
	KernelFile(BytesCnt velicina, int indeksPrvogNivoa, Partition* particija, char mode, char*fname);
private:
	list<ElemN*> niti;
	friend class File;
	unsigned long* indeksPrvogNivoa;
	int indeksPrvi;
	SRWLOCK mutex;
	friend class FS;
	friend class KernelFS;
	list<Elem2*> indeksi;
	Partition* part;
	
};
#endif
