#include "Filer.h"
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    Filer f;

    f.send("x", "y", "z", 5);

    string first, second;
    size_t fmt;
    f.read("x", first, second, fmt);
    cout << "first=" << first << " second=" << second << " fmt=" << fmt << endl;
    f.remove("x");
}