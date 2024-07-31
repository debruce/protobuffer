#pragma once

#include <memory>
#include <string>

class Filer {
public:
    Filer(const std::string& path);

    void send(const std::string& name, const std::string& attributes, const std::string& payload);
    bool read(const std::string& name, std::string& attributes, std::string& payload);
    
private:
    struct Impl;
    std::shared_ptr<Impl>   pImpl;
};