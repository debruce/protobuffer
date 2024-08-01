#include "Filer.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/inotify.h>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <chrono>
#include <iostream>

using namespace std;

struct MyExcept : public std::system_error {
    MyExcept(const string& scall_name)
        : system_error(error_code(errno, generic_category()), scall_name) {}
};

using DataVector = Filer::DataVector;

size_t chunk_size(size_t bytes, size_t alignment)
{
    return ((bytes + alignment - 1) / alignment) * alignment;
}

inline void* advance(void* ptr, size_t bytes)
{
    return static_cast<char*>(ptr) + bytes;
}

inline size_t& as_size_t(void* ptr)
{
    return *static_cast<size_t*>(ptr);
}

struct Filer::Impl {
    filesystem::path root_dir;
    filesystem::path data_dir;
    int temp_fd_, data_fd_, note_fd_;

    Impl()
    {
        root_dir = "/run/user/";
        root_dir /= to_string(getuid());
        data_dir = root_dir / "filer";
        if (filesystem::exists(data_dir) && !filesystem::is_directory(data_dir)) {
            stringstream ss;
            ss << '"' << data_dir << "\" exists and is not a directory.";
            throw runtime_error(ss.str());
        }
        if (!filesystem::exists(data_dir)) {
            filesystem::create_directory(data_dir);
        }
        if ((temp_fd_ = open(root_dir.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open");
        if ((data_fd_ = open(data_dir.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open");
        note_fd_ = inotify_init();
    }

    ~Impl()
    {
        close(temp_fd_);
        close(data_fd_);
    }

    void send(const string& name, const DataVector& data)
    {
        size_t byte_count = sizeof(size_t); // the number of vectors
        for (const auto& v : data) {
            byte_count += sizeof(size_t); // the length of the next vector
            byte_count += chunk_size(v.size(), sizeof(size_t));
        }
        auto t = chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now().time_since_epoch()).count();
        char file_name[64];
        snprintf(file_name, sizeof(file_name), "%0lx", t);
        int fd = openat(temp_fd_, file_name, O_CREAT|O_RDWR, 0644);
        if (fd < 0) {
            throw MyExcept("openat");
        }
        ftruncate(fd, byte_count);
        auto orig_ptr = mmap(0, byte_count, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (orig_ptr == MAP_FAILED) {
            throw MyExcept("mmap");
        }
        auto ptr = orig_ptr;
        as_size_t(ptr) = data.size();
        ptr = advance(ptr, sizeof(size_t));
        for (const auto& v : data) {
            as_size_t(ptr) = v.size();
            ptr = advance(ptr, sizeof(size_t));
            memcpy(ptr, v.data(), v.size());
            size_t space = 0x10000000;
            ptr = align(sizeof(size_t), v.size(), ptr, space);
        }

        if (munmap(orig_ptr, byte_count) < 0) {
            throw MyExcept("mumap");
        }
        if (linkat(temp_fd_, file_name, data_fd_, name.c_str(), 0) < 0) {
            throw MyExcept("linkat");
        }
        if (unlinkat(temp_fd_, file_name, 0) < 0) {
            throw MyExcept("unlinkat");
        }
        close(fd);
    }

    bool read(const string& name, DataVector& data)
    {
        int fd = openat(data_fd_, name.c_str(), O_RDONLY);
        if (fd < 0) {
            return false;
        }
        struct stat ss;
        fstat(fd, &ss);
        auto orig_ptr = mmap(0, ss.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (orig_ptr == MAP_FAILED) {
            close(fd);
            return false;
        }
        close(fd);
        auto ptr = orig_ptr;
        size_t vcount = as_size_t(ptr);
        ptr = advance(ptr, sizeof(size_t));
        data.resize(vcount);
        for (auto i = 0; i < vcount; i++) {
            size_t bcount = as_size_t(ptr);
            ptr = advance(ptr, sizeof(size_t));
            auto p = static_cast<char*>(ptr);
            data[i] = string(p, bcount);
        }
        if (munmap(orig_ptr, ss.st_size) < 0) {
            throw MyExcept("mumap");
        }
        return true;
    }

    bool remove(const string& name)
    {
        return unlinkat(data_fd_, name.c_str(), 0) >= 0;
    }
};

Filer::Filer() : pImpl(new Impl())
{

}

void Filer::send(const string& name, const DataVector& data)
{
    pImpl->send(name, data);
}

bool Filer::read(const string& name, DataVector& data)
{
    return pImpl->read(name, data);
}

bool Filer::remove(const string& name)
{
    return pImpl->remove(name);
}