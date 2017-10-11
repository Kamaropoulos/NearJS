#ifndef NEARJS_NEAR_HPP
#define NEARJS_NEAR_HPP

#include <cstring>

typedef void  (*NearOnloadCB)(void* isolate);
typedef void  (*NearOnunloadCB)(void* isolate, int exitCode);
typedef char* (*NearHostcallCB)(const char* name, const char* value);
typedef void  (*NearHostonCB)(int argc, char** argv);

extern "C" void nearInit(const char* processName,
                         const char* userScript,
                         NearOnloadCB,
                         NearOnunloadCB,
                         NearHostcallCB);
extern "C" int  nearJSCompile(const char* source, char* dest, size_t n);
extern "C" int  nearJSCall(const char* name, const char* value, char* dest, size_t n);
extern "C" int  nearJSEmit(const char* name, const char* value);
extern "C" int  nearHostOn(const char* name, NearHostonCB);

#endif //NEARJS_NEAR_HPP