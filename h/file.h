#ifndef _file_h_
#define _file_h_
#include"KernelFS.h"

class KernelFile;
class File {
public:
	~File(); //zatvaranje fajla
	char write(BytesCnt, char* buffer);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	BytesCnt filePos();
	char eof();
	BytesCnt getFileSize();
	char truncate();
private:
	friend class FS;
	friend class KernelFS;
	friend class KernelFile;
	File(); //objekat fajla se mo�e kreirati samo otvaranjem
	KernelFile *myImpl;
};

#endif
