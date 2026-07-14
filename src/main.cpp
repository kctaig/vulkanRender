#include "core/Application.h"

int main(int argc, char** argv) {

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    vr::Application app;
    return app.run(argc, argv);
}
