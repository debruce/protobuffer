#include "Filer.h"
#include <iostream>

int main(int argc, char *argv[])
{
    Filer f("/run/user/1000");

    f.send("x", "y", "z");
}