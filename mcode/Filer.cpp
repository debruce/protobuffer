#include "Filer.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>

using namespace std;

struct Filer::Impl {
    int dirfd;

    Impl(const string& path)
    {
        if ((dirfd = open(path.c_str(), O_DIRECTORY)) < 0)
            throw std::runtime_error("Cannot open dir");
    }

    ~Impl()
    {
        close(dirfd);
    }

    void send(const string& name, const string& attributes, const string& payload)
    {
        cout << __PRETTY_FUNCTION__ << ' ' << name << endl;
        auto p = dir_path + path;
        int fd = openat(dirfd, name.c_str(), O_TMPFILE|O_RDWR, 0644);
        if (linkat(fd, 0, dirfd, name.c_str(), AT_EMPTY_PATH) < 0)
            throw std::runtime_error("Cannot open linkat");
    }

    bool read(const string& name, string& attributes, string& payload)
    {
        cout << __PRETTY_FUNCTION__ << ' ' << name << endl;
        return true;
    }
};

Filer::Filer(const string& path) : pImpl(new Impl(path))
{

}

void Filer::send(const string& name, const string& attributes, const string& payload)
{
    pImpl->send(name, attributes, payload);
}

bool Filer::read(const string& name, string& attributes, string& payload)
{
    return pImpl->read(name, attributes, payload);
}