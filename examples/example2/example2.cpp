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

static void helloWorld(NearArguments args) {
    cout << "Hello World!" << endl;
    nearReturn(args, to_string(5).c_str());
}

static void getString(NearArguments args){
    nearReturn(args, "Hey there!");
}

static void badMethod(NearArguments args){
    nearError(args, "An error occured");
}

int main(int argc, char *argv[]) {
    const char *userScript = "require('./example2.js');";

    nearSetMethod("helloWorld", helloWorld);
    nearSetMethod("getString", getString);
    nearSetMethod("badMethod", badMethod);

    nearInit(nullptr, userScript, nullptr, nullptr, nullptr);

    SLEEP_ONE_SECOND;

    return 0;
}
