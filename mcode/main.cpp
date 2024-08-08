#include "Filer.h"
#include <unistd.h>
#include <filesystem>
#include <iostream>

using namespace std;

ostream& operator<<(ostream& os, const vector<string_view>& data)
{
    os << '[';
    for (const auto& v : data) {
        os << " \"" << v << '"';
    }
    os << " ]";
    return os;
}

int main(int argc, char *argv[])
{
    try {
        auto files = filesystem::path{"/run/user/1000"} / "filer";
        Filer f(files);
        FilerWatcher w(f, [&](shared_ptr<FileRef> ref) {
            cout << "callback name=" << ref->getName() << endl;
            ref->remove();
            cout << *ref << endl;
        });

        for (auto i = 0; i < 5; i++) {
            // sleep(1);
            f.send("x", vector<string>{"hello with a long nothing", "smoke", "gets", "in", "your", "eyes"});
            // cout << "after send" << endl;
        }
    }
    catch (const std::exception& ex) {
        cout << "Threw " << ex.what() << endl;
    }
}