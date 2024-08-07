#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

class FileRef : public std::vector<std::string_view> {
public:
    FileRef(int dir_fd, const std::filesystem::path& data_dir, const std::string& name);
    ~FileRef();
    void remove();
private:
    struct Impl;
    std::shared_ptr<Impl>   pImpl;
};

class Filer {
public:
    using DataVector = std::vector<std::string>;

    Filer(const std::filesystem::path& root_dir);

    void send(const std::string& name, const DataVector& data);
    std::shared_ptr<FileRef> read(const std::string& name);
    bool remove(const std::string& name);
    void watch(bool state = true);
    void wait();
    
private:
    struct Impl;
    std::shared_ptr<Impl>   pImpl;
};