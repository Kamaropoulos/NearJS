#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_ONE_SECOND Sleep(1000);
#else

#include <unistd.h>

#define SLEEP_ONE_SECOND usleep(1000*1000);
#endif

#include <near.hpp>

using namespace std;

void nearOnLoad(void *isolate) {
    cout << "\e[32m" << "** topyOnLoad : " << isolate << endl;

    // custom source
    const char *source = "function _f(x) {"
            "  return x ? x : ':)';"
            "};"
            "var _v = 43;";

    char data[1024];
    int ret = nearJSCompile(source, data, sizeof(data));
    if (ret < 0)
        cout << "** nearJSCompile error - code : " << ret << " , data : " << data << endl;
    else
        cout << "** nearJSCompile : " << data << endl;

    ret = nearJSCall("_f", "", data, sizeof(data));
    if (ret < 0)
        cout << "** nearJSCall error - code : " << ret << " , data : " << data << endl;
    else
        cout << "** nearJSCall : " << data;

    cout << "\e[0m" << endl << flush;
}

void nearOnUnload(void *isolate, int exitCode) {
    cout << "\e[31m" << "** nearOnUnload : " << isolate;
    cout << " exitCode : " << exitCode << endl;
    cout << "\e[0m" << endl << flush;

    _exit(exitCode);
}

char *nearHostCall(const char *name, const char *value) {
    cout << "\e[93m" << "** from javascript. name = " << name;
    cout << " , value = " << value << "\e[0m";
    cout << endl << flush;

    char *data = new char[10];
    strcpy(data, "hi there");
    return data;
}


int main(int argc, char *argv[]) {
    const char *userScript = "require('./example1.js');";

    // near(processName, userScript, onloadCB, onunloadCB, hostCallCB)
    nearInit(argv[0],
             userScript,
             nearOnLoad,
             nearOnUnload,
             nearHostCall);

    nearHostOn("exit", [](int argc, char **argv) {
        printf("nearHostOn - argc : %d\n", argc);
        for (int i = 0; i < argc; i++)
            printf("nearHostOn - argv[%d] = %s\n", i, argv[i]);
    });

    // dummy loop in host
    static int i = 0;
    while (true) {
        SLEEP_ONE_SECOND;
        addMessage("test", "{key:\"value\"}");
    }

    return 0;
}
