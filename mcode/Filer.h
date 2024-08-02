#pragma once

#include <memory>
#include <string>
#include <vector>

class Filer {
public:
    using DataVector = std::vector<std::string>;

    Filer();

    void send(const std::string& name, const DataVector& data);
    bool read(const std::string& name, DataVector& data);
    bool remove(const std::string& name);
    void watch(bool state = true);
    void wait();
    
private:
    struct Impl;
    std::shared_ptr<Impl>   pImpl;
};