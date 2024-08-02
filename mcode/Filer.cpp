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
#include <filesystem>
#include <sstream>
#include <system_error>
#include <chrono>
#include <thread>
#include <string_view>
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
    filesystem::path root_dir_, data_dir_;
    int temp_fd_, data_fd_, note_fd_;
    int watch_fd_data_, watch_fd_root_;
    thread* tid;

    Impl()
    {
        root_dir_ = "/run/user/";
        root_dir_ /= to_string(getuid());
        data_dir_ = root_dir_ / "filer";
        if (filesystem::exists(data_dir_) && !filesystem::is_directory(data_dir_)) {
            stringstream ss;
            ss << '"' << data_dir_ << "\" exists and is not a directory.";
            throw runtime_error(ss.str());
        }
        if (!filesystem::exists(data_dir_)) {
            filesystem::create_directory(data_dir_);
        }
        if ((temp_fd_ = open(root_dir_.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open");
        if ((data_fd_ = open(data_dir_.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open");
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

    void watch(bool state)
    {
        if (state) {
            if ((watch_fd_data_ = inotify_add_watch(note_fd_, data_dir_.c_str(), IN_OPEN|IN_CREATE)) < 0) {
                throw MyExcept("inotify_add_watch");
            }
            tid = new thread([&]() { this->wait(); });
        }
        else {
            if (inotify_rm_watch(note_fd_, watch_fd_data_) < 0) {
                throw MyExcept("inotify_rm_watch");
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

void Filer::watch(bool state)
{
    pImpl->watch(state);
}

void Filer::wait()
{
    pImpl->wait();
}