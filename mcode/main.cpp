#include "Filer.h"
#include <unistd.h>
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    Filer f;

    f.watch();

    for (auto i = 0; i < 5; i++) {
        sleep(1);
        f.send("x", vector<string>{"x", "y"});
        sleep(1);

        vector<string> data;
        auto v = f.read("x", data);
        // cout << "v=" << v << " first=" << data[0] << " second=" << data[1] << endl;
        f.remove("x");
    }
}