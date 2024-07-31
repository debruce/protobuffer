#include "Filer.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdexcept>
#include <iostream>

using namespace std;

struct Filer::Impl {
    int tempfd, datafd;

    Impl(const string& path)
    {
        if ((tempfd = open(path.c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw std::runtime_error("Cannot open dir");
        if ((datafd = open((path+"/data").c_str(), O_DIRECTORY|O_RDONLY)) < 0)
            throw std::runtime_error("Cannot open dir");
    }

    ~Impl()
    {
        close(tempfd);
        close(datafd);
    }

    void send(const string& name, const string& attributes, const string& payload)
    {
        string temp_name = "tmp";
        int fd = openat(tempfd, temp_name.c_str(), O_CREAT|O_RDWR, 0644);
        if (fd < 0) {
            throw std::runtime_error("Cannot open file des");
        }
        if (write(fd, "hello\n", 6) < 0) {
            throw std::runtime_error("Cannot write file des");            
        }
        fsync(fd);
        if (linkat(tempfd, temp_name.c_str(), datafd, name.c_str(), 0) < 0) {
            throw std::runtime_error("Cannot linkat");
        }
        if (unlinkat(tempfd, temp_name.c_str(), 0) < 0) {
            throw std::runtime_error("Cannot unlinkat");
        }
        close(fd);
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