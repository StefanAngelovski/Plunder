#include "app/menuApp/include/MenuApplication.h"
#include <sys/stat.h>
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    MenuApplication app;
    if (!app.initialize()) return 1;
    app.run();
    return 0;
}
