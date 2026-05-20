#ifndef MAPPED_FILE_H
#define MAPPED_FILE_H

class MappedFile {
public:
    MappedFile();
    ~MappedFile();
    bool open(const char* path, long expected_size);
    void close();
    const unsigned char* data() const;
    long size() const;
    bool is_missing() const;

private:
    MappedFile(const MappedFile&);
    MappedFile& operator=(const MappedFile&);

#if defined(_WIN32)
    void* file_handle_;
    void* mapping_handle_;
#else
    int fd_;
#endif
    const unsigned char* data_;
    long  size_;
    bool  missing_;
};

#endif /* MAPPED_FILE_H */
