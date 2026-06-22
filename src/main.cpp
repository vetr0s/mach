#include "mach/engine.h"

int main(int /*argc*/, char * /*argv*/[]) {
    mach::Engine engine;
    if (!engine.init("mach", 1280, 720)) {
        return 1;
    }
    return engine.run();
}
