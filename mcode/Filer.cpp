#include "Filer.h"

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/inotify.h>
#include <cstring>
#include <sstream>
#include <system_error>
#include <chrono>
#include <thread>
#include <string_view>
#include <iostream>

using namespace std;
using namespace std::filesystem;

struct MyExcept : public filesystem_error {
    MyExcept(const string& what, const path& path)
        : filesystem_error(what, path, error_code(errno, generic_category())) {}
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
    path temp_dir_;
    path data_dir_;
    int temp_fd_;
    int data_fd_;
    int note_fd_;
    int watch_token_;
    thread* tid;

    Impl(const path& root_dir)
    {
        data_dir_ = root_dir;
        temp_dir_ = root_dir / "temp";
        create_directories(temp_dir_);
        if ((temp_fd_ = open(temp_dir_.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open", temp_dir_);
        if ((data_fd_ = open(data_dir_.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open", data_dir_);
        note_fd_ = inotify_init();
    }

    ~Impl()
    {
        if (tid) {
            tid->join();
            delete tid;
        }
        close(temp_fd_);
        close(data_fd_);
        close(note_fd_);
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
            throw MyExcept("openat", temp_dir_ / file_name);
        }
        ftruncate(fd, byte_count);
        auto orig_ptr = mmap(0, byte_count, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (orig_ptr == MAP_FAILED) {
            throw MyExcept("mmap", temp_dir_ / file_name);
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
            throw MyExcept("mumap", temp_dir_ / file_name);
        }
        if (linkat(temp_fd_, file_name, data_fd_, name.c_str(), 0) < 0) {
            throw MyExcept("linkat", data_dir_ / name);
        }
        if (unlinkat(temp_fd_, file_name, 0) < 0) {
            throw MyExcept("unlinkat", temp_dir_ / file_name);
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
            throw MyExcept("mumap", data_dir_ / name);
        }
        return true;
    }

    bool remove(const string& name)
    {
        return unlinkat(data_fd_, name.c_str(), 0) >= 0;
    }

    void watch(bool state)
    {
        if (state) {
            if ((watch_token_ = inotify_add_watch(note_fd_, data_dir_.c_str(), IN_OPEN|IN_CREATE)) < 0) {
                throw MyExcept("inotify_add_watch", data_dir_);
            }
            tid = new thread([&]() { this->wait(); });
        }
        else {
            if (inotify_rm_watch(note_fd_, watch_token_) < 0) {
                throw MyExcept("inotify_rm_watch", data_dir_);
            }
        }
    }

    void wait()
    {
        while (true) {
            char buf[4096];
            int ret = ::read(note_fd_, buf, sizeof(buf));
            if (ret >=  0) {
                auto ie = (struct inotify_event *)buf;
                cout << "wd=" << ie->wd
                    << " mask=" << hex << ie->mask << dec
                    << " name=" << string_view(ie->name, ie->len)
                    << endl;
            }
            else {
                cout << "read returned ret=" << ret << " errno=" << errno << endl;
            }
        }
    }
};

Filer::Filer(const filesystem::path& root_dir) : pImpl(new Impl(root_dir))
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

void Filer::watch(bool state)
{
    pImpl->watch(state);
}

void Filer::wait()
{
    pImpl->wait();
}