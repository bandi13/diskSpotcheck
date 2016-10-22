#ifndef UTILFILE_H
#define UTILFILE_H

// Written by: Fekete Andras 2016

#include <stdio.h>
#include <fstream>
#include <sys/stat.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define UNUSED(expr) (void)(expr)
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#ifdef ENABLE_DEBUG_CODE
#define DEBUGPRINTLN(X) std::cerr << __FILE__ << ':' << __LINE__ << ": " << X << std::endl;
#else
#define DEBUGPRINTLN(X)
#endif

class File {
public:
	File() { fdSize = 0; fdBlockSize = 0; }
	virtual ~File() = default;
	virtual ssize_t read(char *buf, size_t len, off_t offset) { UNUSED(buf); UNUSED(len); UNUSED(offset); return -1; }
	virtual ssize_t write(const char *buf, size_t len, off_t offset) { UNUSED(buf); UNUSED(len); UNUSED(offset); return -1; }
	virtual int flush() { return 0; }
	void getFileInfo(size_t &fileSize, size_t &fileBlockSize) { fileSize = fdSize; fileBlockSize = fdBlockSize; }
	size_t getSize() { return fdSize; }
	size_t getBlockSize() { return fdBlockSize; }

protected:
	size_t fdSize;
	size_t fdBlockSize;
};

class FileUnbuffered : public File {
public:
	explicit FileUnbuffered(const char *filename) : FileUnbuffered(open(filename, O_RDWR | O_LARGEFILE)) {
		DEBUGPRINTLN("Opening unbuffered file: " << filename);
	}
	~FileUnbuffered() override { close(fd); }
protected:
	FileUnbuffered(int inFd) : File(), fd(inFd) {
		if(fd == -1) {
			fdSize = 0;
			fdBlockSize = 0;
			DEBUGPRINTLN("Can't open in file: " << strerror(errno));
		} else {
			fdSize = lseek(fd, 0, SEEK_END);

			if (ioctl(fd, BLKBSZGET, &fdBlockSize)) {
				DEBUGPRINTLN("Can't issue IOCTL to get blocksize. Assuming page size.");
				fdBlockSize = 4096;
			}
			if (fdBlockSize > fdSize) {
				DEBUGPRINTLN("Huge block size. Reducing to page size.");
				fdBlockSize = 4096;
			}
		}
		DEBUGPRINTLN("size=" << fdSize << " blockSize=" << fdBlockSize);
	}

public:
	ssize_t read(char *buf, size_t len, off_t offset) override {
		DEBUGPRINTLN("read(" << fd << ',' << len << ',' << offset << ")");
		if (unlikely(lseek(fd, offset, SEEK_SET) != offset)) { DEBUGPRINTLN("seek error" << strerror(errno)); return -1; }
		return ::read(fd,buf,len);
	}

	ssize_t write(const char *buf, size_t len, off_t offset) override {
		DEBUGPRINTLN("write(" << fd << ',' << len << ',' << offset << ")");
		if (unlikely(lseek(fd, offset, SEEK_SET) != offset)) { DEBUGPRINTLN("seek error" << strerror(errno)); return -1; }
		return ::write(fd,buf,len);
	}
	int flush() override { return fdatasync(fd); }
protected:
	int fd;
};

class FileDirect : public FileUnbuffered {
public:
	explicit FileDirect(const char *filename) : FileUnbuffered(open(filename,O_RDWR | O_LARGEFILE | O_DIRECT)) {
		DEBUGPRINTLN("Opening direct file: " << filename);
	}

	ssize_t read(char *buf, size_t len, off_t offset) override {
		assert((uint64_t)buf % 4096 == 0);
		assert(len % 4096 == 0);
		assert(offset % 4096 == 0);
		return FileUnbuffered::read(buf,len,offset);
	}
	ssize_t write(const char *buf, size_t len, off_t offset) override {
		assert((uint64_t)buf % 4096 == 0);
		assert(len % 4096 == 0);
		assert(offset % 4096 == 0);
		return FileUnbuffered::write(buf,len,offset);
	}
};

class FileBuffered : public File {
public:
	explicit FileBuffered(const char *filename) : File() {
		DEBUGPRINTLN("Opening buffered file: " << filename);
		fp = fopen(filename,"r+");
		if(fp == nullptr) { DEBUGPRINTLN("Can't open file: " << filename); return; }
		if((fseek(fp,0,SEEK_END) == 0)) fdSize = ftell(fp); else fdSize = 0;
		struct stat buf;
		fstat(fp->_fileno, &buf);
		if(fdSize < (size_t) buf.st_size) fdSize = buf.st_size;
		if(fdSize < (size_t) (buf.st_blocks * 512)) fdSize = buf.st_blocks * 512;
		fdBlockSize = buf.st_blksize;
	}
	~FileBuffered() override { fclose(fp); }

	ssize_t read(char *buf, size_t len, off_t offset) override {
		DEBUGPRINTLN("read(" << fp << ',' << len << ',' << offset << ")");
		if (unlikely(fseek(fp, offset, SEEK_SET) != 0)) { DEBUGPRINTLN("File::read(): seek error"); return -1; }
		return ::fread(buf, 1, len, fp);
	}
	ssize_t write(const char *buf, size_t len, off_t offset) override {
		DEBUGPRINTLN("write(" << fp << ',' << len << ',' << offset << ")");
		if (unlikely(fseek(fp, offset, SEEK_SET) != 0)) { DEBUGPRINTLN("File::write(): seek error"); return -1; }
		return ::fwrite(buf, 1, len, fp);
	}
	int flush() override { return fflush(fp); }
private:
	FILE *fp;
};

class FileRAM : public File {
public:
	explicit FileRAM(size_t size) : File() {
		DEBUGPRINTLN("Opening RAM file with size " << (uint64_t) size);
		mem = (char *) malloc(size);
		if(mem == nullptr) { DEBUGPRINTLN("Can't allocate enough memory."); return; }
		fdSize = size;
		fdBlockSize = 1;
	}
	~FileRAM() override { free(mem); }

	ssize_t read(char *buf, size_t len, off_t offset) override {
		DEBUGPRINTLN("read(" << (uint64_t)mem << ',' << len << ',' << offset << ")");
		if((uint64_t)(offset+len) > fdSize) { DEBUGPRINTLN("FileRAM::read(): out of bounds error"); errno = EFAULT; return -1; }
		memcpy(buf,mem+offset,len);
		return len;
	}
	ssize_t write(const char *buf, size_t len, off_t offset) override {
		DEBUGPRINTLN("write(" << (uint64_t)mem << ',' << len << ',' << offset << ")");
		if((uint64_t)(offset+len) > fdSize) { DEBUGPRINTLN("FileRAM::write(): out of bounds error"); errno = EFAULT; return -1; }
		memcpy(mem+offset,buf,len);
		return len;
	}
private:
	char *mem;
};

#endif
