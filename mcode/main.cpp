#include "Filer.h"
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    Filer f;

    f.send("x", vector<string>{"x", "y"});

    vector<string> data;
    auto v = f.read("x", data);
    cout << "v=" << v << " first=" << data[0] << " second=" << data[1] << endl;
    f.remove("x");
}