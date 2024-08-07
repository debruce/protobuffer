#include "Filer.h"
#include <unistd.h>
#include <filesystem>
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    try {
        auto files = filesystem::path{"/run/user/1000"} / "filer";
        Filer f(files);
        cout << "after constructor" << endl;

        f.watch();

        for (auto i = 0; i < 5; i++) {
            sleep(1);
            f.send("x", vector<string>{"x", "y"});
            sleep(1);

            auto v = f.read("x");
            cout << "v=" << v << " first=" << (*v)[0] << " second=" << (*v)[1] << endl;
            v->remove();
        }
    }
    catch (const std::exception& ex) {
        cout << "Threw " << ex.what() << endl;
    }
}