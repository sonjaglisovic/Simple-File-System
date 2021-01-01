#include"kernfile.h"


extern ElemK* sadrzaj[256];
extern int indeksZamena;
File::File(){}
File::~File() {
	AcquireSRWLockExclusive(&(myImpl->mojFajlSistem->myImpl->mutex));
	myImpl->brojNiti--;
	if (myImpl->brojNiti == 0) {
		myImpl->mojFajlSistem->myImpl->otvoreniFajlovi.remove(myImpl);
		if (myImpl->mojFajlSistem->myImpl->otvoreniFajlovi.size() == 0) {
			WakeConditionVariable(&myImpl->mojFajlSistem->myImpl->cond);
		}
		if (myImpl->izmenjen) {
			myImpl->mojFajlSistem->myImpl->nadjiIliNapravi(myImpl->fname, myImpl->vel, 'b', nullptr);
		}
		ElemN* el = myImpl->izbaci();
		myImpl->niti.remove(el);

		delete myImpl;

	}
	else {
		ElemN* el = myImpl->izbaci();
		myImpl->niti.remove(el);
		if (myImpl->mode == 'w' || myImpl->mode == 'a') {
			ReleaseSRWLockExclusive(&myImpl->rw);
		}
		else {
			ReleaseSRWLockShared(&myImpl->rw);
		}

	}

	ReleaseSRWLockExclusive(&(myImpl->mojFajlSistem->myImpl->mutex));
}
char File::write(BytesCnt cnt, char* buffer) {
	return myImpl->write(cnt, buffer);
}
BytesCnt File::read(BytesCnt cnt, char* buffer) {
	return myImpl->read(cnt, buffer);
}
BytesCnt File::filePos() {
	return myImpl->filePos();
}
char File::eof() {
	return myImpl->eof();
}
BytesCnt File::getFileSize() {
	return myImpl->getFileSize();
}
char File::truncate() {
	return myImpl->truncate();
}
char File::seek(BytesCnt cnt) {
	return myImpl->seek(cnt);
}