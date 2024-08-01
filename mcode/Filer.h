#pragma once

#include <memory>
#include <string>

class Filer {
public:
    Filer();

    void send(const std::string& name, const std::string& attributes, const std::string& payload, const size_t& fmt);
    bool read(const std::string& name, std::string& attributes, std::string& payload, size_t& fmt);
    bool remove(const std::string& name);
    
private:
    struct Impl;
    std::shared_ptr<Impl>   pImpl;
};