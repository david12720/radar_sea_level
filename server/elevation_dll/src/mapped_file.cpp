#include "mapped_file.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

MappedFile::MappedFile()
    : file_handle_(INVALID_HANDLE_VALUE)
    , mapping_handle_(NULL)
    , data_(NULL)
    , size_(0)
    , missing_(false)
{}

MappedFile::~MappedFile()
{
    close();
}

bool MappedFile::open(const char* path, long expected_size)
{
    close();
    missing_ = false;

    HANDLE fh = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
            missing_ = true;
        return false;
    }

    LARGE_INTEGER sz;
    if (!GetFileSizeEx(fh, &sz) || sz.QuadPart != (LONGLONG)expected_size) {
        CloseHandle(fh);
        return false;
    }

    HANDLE mh = CreateFileMappingA(fh, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mh) {
        CloseHandle(fh);
        return false;
    }

    const void* ptr = MapViewOfFile(mh, FILE_MAP_READ, 0, 0, 0);
    if (!ptr) {
        CloseHandle(mh);
        CloseHandle(fh);
        return false;
    }

    file_handle_    = fh;
    mapping_handle_ = mh;
    data_           = static_cast<const unsigned char*>(ptr);
    size_           = expected_size;
    return true;
}

void MappedFile::close()
{
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = NULL;
    }
    if (mapping_handle_) {
        CloseHandle(static_cast<HANDLE>(mapping_handle_));
        mapping_handle_ = NULL;
    }
    if (file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(file_handle_));
        file_handle_ = INVALID_HANDLE_VALUE;
    }
    size_    = 0;
    missing_ = false;
}

#else /* POSIX */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

MappedFile::MappedFile()
    : fd_(-1)
    , data_(NULL)
    , size_(0)
    , missing_(false)
{}

MappedFile::~MappedFile()
{
    close();
}

bool MappedFile::open(const char* path, long expected_size)
{
    close();
    missing_ = false;

    int fd = ::open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT)
            missing_ = true;
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size != (off_t)expected_size) {
        ::close(fd);
        return false;
    }

    void* ptr = mmap(NULL, (size_t)expected_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        ::close(fd);
        return false;
    }

    fd_   = fd;
    data_ = static_cast<const unsigned char*>(ptr);
    size_ = expected_size;
    return true;
}

void MappedFile::close()
{
    if (data_) {
        munmap(const_cast<unsigned char*>(data_), (size_t)size_);
        data_ = NULL;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    size_    = 0;
    missing_ = false;
}

#endif /* _WIN32 / POSIX */

const unsigned char* MappedFile::data() const  { return data_; }
long                 MappedFile::size() const   { return size_; }
bool                 MappedFile::is_missing() const { return missing_; }
