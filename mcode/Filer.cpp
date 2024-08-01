#include "Filer.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <chrono>
#include <iostream>

using namespace std;

struct Msg {
    size_t attr_size;
    size_t payload_size;
    size_t payload_fmt;
    char attr[512];
    char payload[4096 - 512 - 3 * sizeof(size_t)];
};

struct MyExcept : public std::system_error {
    MyExcept(const string& scall_name)
        : system_error(error_code(errno, generic_category()), scall_name) {}
};

struct Filer::Impl {
    filesystem::path root_dir;
    filesystem::path data_dir;
    int tempfd, datafd;

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
        if ((tempfd = open(root_dir.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open");
        if ((datafd = open(data_dir.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw MyExcept("open");
    }

    ~Impl()
    {
        close(tempfd);
        close(datafd);
    }

    void send(const string& name, const string& attributes, const string& payload, const size_t& fmt)
    {
        auto t = chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now().time_since_epoch()).count();
        char file_name[64];
        snprintf(file_name, sizeof(file_name), "%0lx", t);
        int fd = openat(tempfd, file_name, O_CREAT|O_RDWR, 0644);
        if (fd < 0) {
            throw MyExcept("openat");
        }
        ftruncate(fd, sizeof(Msg));
        void *ptr = mmap(0, sizeof(Msg), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            throw MyExcept("mmap");
        }
        auto msg = reinterpret_cast<Msg*>(ptr);
        memcpy(msg->attr, attributes.c_str(), attributes.size());
        msg->attr_size = attributes.size();
        memcpy(msg->payload, payload.c_str(), payload.size());
        msg->payload_size = payload.size();
        msg->payload_fmt = fmt;
        if (munmap(ptr, sizeof(Msg)) < 0) {
            throw MyExcept("mumap");
        }
        if (linkat(tempfd, file_name, datafd, name.c_str(), 0) < 0) {
            throw MyExcept("linkat");
        }
        if (unlinkat(tempfd, file_name, 0) < 0) {
            throw MyExcept("unlinkat");
        }
        close(fd);
    }

    bool read(const string& name, string& attributes, string& payload, size_t& fmt)
    {
        int fd = openat(datafd, name.c_str(), O_RDONLY);
        if (fd < 0) {
            return false;
        }
        void *ptr = mmap(0, sizeof(Msg), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            close(fd);
            return false;
        }
        close(fd);
        auto msg = reinterpret_cast<Msg*>(ptr);
        attributes.resize(msg->attr_size);
        memcpy(attributes.c_str(), msg->attr, msg->attr_size);
        payload.resize(msg->payload_size);
        memcpy(payload.c_str(), msg->payload, msg->payload_size);
        fmt = msg->payload_fmt;
        if (munmap(ptr, sizeof(Msg)) < 0) {
            throw MyExcept("mumap");
        }
        return true;
    }

    bool remove(const string& name)
    {
        return unlinkat(datafd, name.c_str(), 0) >= 0;
    }
};

Filer::Filer() : pImpl(new Impl())
{

}

void Filer::send(const string& name, const string& attributes, const string& payload, const size_t& fmt)
{
    pImpl->send(name, attributes, payload, fmt);
}

bool Filer::read(const string& name, string& attributes, string& payload, size_t& fmt)
{
    return pImpl->read(name, attributes, payload, fmt);
}

bool Filer::remove(const string& name)
{
    return pImpl->remove(name);
}