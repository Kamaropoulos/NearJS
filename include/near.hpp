#ifndef NEARJS_NEAR_HPP
#define NEARJS_NEAR_HPP

#include <cstring>
#include <iostream>
#include "libplatform/libplatform.h"
#include "uv.h"
#include "node.h"

typedef const v8::FunctionCallbackInfo<v8::Value>& NearArguments;

typedef void  (*NearOnloadCB)(void *isolate);

typedef void  (*NearOnunloadCB)(void *isolate, int exitCode);

typedef char *(*NearHostcallCB)(const char *name, const char *value);

typedef void  (*NearHostonCB)(int argc, char **argv);

typedef void (*nearMethodPointer)(const v8::FunctionCallbackInfo<v8::Value> &);

extern "C" void nearInit(const char *processName,
                         const char *userScript,
                         NearOnloadCB,
                         NearOnunloadCB,
                         NearHostcallCB);
extern "C" int nearJSCompile(const char *source, char *dest, size_t n);
extern "C" int nearJSCall(const char *name, const char *value, char *dest, size_t n);
extern "C" int nearJSEmit(const char *name, const char *value);
extern "C" int nearHostOn(const char *name, NearHostonCB);
extern "C" void nearSetMethod(const char *methodName, nearMethodPointer pointer);
extern "C" void nearReturn(NearArguments &args, const char * returnValue);
extern "C" void nearReferenceError(NearArguments &args, const char * errorMessage);
extern "C" void nearSyntaxError(NearArguments &args, const char * errorMessage);
extern "C" void nearTypeError(NearArguments &args, const char * errorMessage);
extern "C" void nearError(NearArguments &args, const char * errorMessage);

#endif //NEARJS_NEAR_HPP