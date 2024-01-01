#include "Program.hpp"
#include <locale>

bool IsCopy1(char *type) {
    return strcmp(type, COPY_NAME_1) == 0;
}

bool IsCopy2(char *type) {
    return strcmp(type, COPY_NAME_2) == 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    Program program = Program(argv[0]);
#if defined(WIN32)
    if (argc > 1) {
        if (IsCopy1(argv[1])) {
            program.StartProgram(COPY_TYPE_1);
            return 0;
        }
        if (IsCopy2(argv[1])) {
            program.StartProgram(COPY_TYPE_2);
            return 0;
        }
    }
#endif
    program.StartProgram();
    return 0;
}