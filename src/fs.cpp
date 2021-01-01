#include"KernelFS.h"

KernelFS *FS::myImpl = new KernelFS();
char FS::mount(Partition* partition){
	return myImpl->mount(partition);
}
char FS::doesExist(char* fname) {
	return myImpl->doesExist(fname);
}
char FS::unmount() {
	return myImpl->unmount();
}
char FS::format() {
	return myImpl->format();
}
FileCnt FS::readRootDir() {
	return myImpl->readRootDir();
}
File* FS::open(char* fname, char mode) {
	return myImpl->open(fname, mode);
}
char FS::deleteFile(char* fname) {
	return myImpl->deleteFile(fname);
}
FS::FS() {}
