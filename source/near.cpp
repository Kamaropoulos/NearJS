#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <cstring>

#include <map>
#include <vector>

#include "libplatform/libplatform.h"
#include "uv.h"
#include "node.h"

#ifdef _WIN32
#define NEAR_EXTERN __declspec(dllexport)
#else
#define NEAR_EXTERN /* nothing */
#endif

#define NEAR_VERSION_MAJOR 0
#define NEAR_VERSION_MINOR 1
#define NEAR_VERSION_PATCH 9

#define NEAR_VERSION  NODE_STRINGIFY(NEAR_VERSION_MAJOR) "." \
                      NODE_STRINGIFY(NEAR_VERSION_MINOR) "." \
                      NODE_STRINGIFY(NEAR_VERSION_PATCH)

#define NEAR_OK              0
#define NEAR_ERROR         (-1)
#define NEAR_COMPILE_ERROR (-2)
#define NEAR_RUNTIME_ERROR (-3)
#define NEAR_BUFFER_ERROR  (-4)

namespace near {

    using namespace std;
    using namespace node;
    using namespace v8;

    typedef void  (*NearOnloadCallback)(void *);

    typedef void  (*NearOnunloadCallback)(void *, int);

    typedef char *(*NearHostCallCallback)(const char *, const char *);

    typedef void  (*NearHostonCB)(int argc, char **argv);

    typedef void  (*methodPointer)(const FunctionCallbackInfo<Value> &);

    extern "C" NEAR_EXTERN int nearJSCompile(const char *source, char *dest, size_t n);
    extern "C" NEAR_EXTERN int nearJSCall(const char *name, const char *value, char *dest, size_t n);
    extern "C" NEAR_EXTERN int nearJSEmit(const char *name, const char *value);
    extern "C" NEAR_EXTERN int nearHostOn(const char *name, NearHostonCB);
    extern "C" NEAR_EXTERN void nearInit(const char *processName,
                                         const char *userScript,
                                         NearOnloadCallback _nearOnLoad,
                                         NearOnunloadCallback _nearOnUnload,
                                         NearHostCallCallback _nearHostCall);
    extern "C" NEAR_EXTERN void nearSetMethod(const char *methodName, methodPointer pointer);
    extern "C" NEAR_EXTERN void nearReturn(v8::FunctionCallbackInfo<v8::Value> &args, const char * returnValue);

    class ArrayBufferAllocator : public ArrayBuffer::Allocator {
    public:
        virtual void *Allocate(size_t length) {
            void *data = AllocateUninitialized(length);
            return data == NULL ? data : memset(data, 0, length);
        }

        virtual void *AllocateUninitialized(size_t length) { return malloc(length); }

        virtual void Free(void *data, size_t) { free(data); }
    };

    static uv_loop_t *loop;
    static Isolate *isolate_;

    static Local<Context> context;

    struct UserMethod {
        const char *methodName;
        methodPointer method;

        UserMethod(const char *methodName, void (*method)(const FunctionCallbackInfo<Value> &)) : methodName(
                methodName), method(method) {}
    };

    static vector<UserMethod> userMethods;

    NearOnloadCallback nearOnLoad;
    NearOnunloadCallback nearOnUnload;
    NearHostCallCallback nearHostCall;

// FIXME : vardic arguments? multiple listeners?
    using Callback = std::map<std::string, Persistent<Function>>;
    static Callback *eventListeners;

    using HostCallback = std::map<std::string, NearHostonCB>;
    static HostCallback *hostEventListeners;

    static Local<Value> GetValue(Isolate *isolate, Local<Context> context,
                                 Local<Object> object, const char *property) {
        Local<String> v8_str =
                String::NewFromUtf8(isolate, property, NewStringType::kNormal)
                        .ToLocalChecked();
        return object->Get(context, v8_str).ToLocalChecked();
    }

    static Local<Value> Stringify(Isolate *isolate, Local<Context> context,
                                  Local<Value> object) {
        auto global = context->Global();

        Local<Value> result;
        auto JSON = GetValue(isolate, context, global, "JSON");
        auto stringify = GetValue(isolate, context, JSON.As<Object>(), "stringify");

        std::vector<Local<Value>> argv;
        argv.push_back(object);

        auto method = stringify.As<Function>();
        result = MakeCallback(isolate, global, method, argv.size(), argv.data());

        return result;
    }

    int nearJSCompile(const char *source, char *dest, size_t n) {
        Isolate *isolate = static_cast<Isolate *>(isolate_);
        Local<Value> result;

        HandleScope handle_scope(isolate);
        TryCatch try_catch(isolate);

        Local<Context> context(isolate->GetCurrentContext());

        Local<String> script = String::NewFromUtf8(isolate, source);
        Local<Script> compiled_script;
        if (!Script::Compile(context, script).ToLocal(&compiled_script)) {
            String::Utf8Value error(try_catch.Exception());
            strncpy(dest, *error, n);
            return NEAR_COMPILE_ERROR;
        }

        if (!compiled_script->Run(context).ToLocal(&result)) {
            String::Utf8Value error(try_catch.Exception());
            strncpy(dest, *error, n);
            return NEAR_RUNTIME_ERROR;
        }

        result = Stringify(isolate, context, result);
        String::Utf8Value ret(result);

        int size = strlen(*ret);
        strncpy(dest, *ret, n);
        if (size >= n)
            return NEAR_BUFFER_ERROR;

        return size;  // NEAR_OK
    }

    int nearJSCall(const char *name, const char *value, char *dest, size_t n) {
        Isolate *isolate = static_cast<Isolate *>(isolate_);

        auto context = isolate->GetCurrentContext();
        auto global = context->Global();

        Local<Value> result;
        auto func = GetValue(isolate, context, global, name);

        if (func->IsFunction()) {
            std::vector<Local<Value>> argv;
            Local<Value> argument = String::NewFromUtf8(isolate, value);
            argv.push_back(argument);

            TryCatch try_catch(isolate);

            auto method = func.As<Function>();
            result = MakeCallback(isolate,
                                  isolate->GetCurrentContext()->Global(), method,
                                  argv.size(), argv.data());

            if (try_catch.HasCaught()) {
                String::Utf8Value error(try_catch.Exception());
                strncpy(dest, *error, n);
                return NEAR_RUNTIME_ERROR;
            }
        } else {
            result = Undefined(isolate);
        }

        result = Stringify(isolate, context, result);
        String::Utf8Value ret(result);

        int size = strlen(*ret);
        strncpy(dest, *ret, n);
        if (size >= n)
            return NEAR_BUFFER_ERROR;

        return size;  // NEAR_OK
    }

    int nearHostOn(const char *name, NearHostonCB cb) {
        (*hostEventListeners)[std::string(name)] = cb;
        return 0;
    }

    static void HostCallMethod(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();

        if (!nearHostCall) {
            args.GetReturnValue().Set(Undefined(isolate));
            return;
        }

        HandleScope scope(isolate);
        Local<Value> result;

        auto context = isolate->GetCurrentContext();
        auto global = context->Global();

        if (args.Length() == 0 || !args[0]->IsString()) {
            args.GetIsolate()->ThrowException(String::NewFromUtf8(isolate, "Argument 1 must be a string"));
            return;
        }


        {
            // FIXME : better way to serialize/deserialize?
            result = Stringify(isolate, context, args[1]);

            String::Utf8Value key(args[0]);
            const char *c_key = *key;

            String::Utf8Value value(result);
            const char *c_value = *value;

            char *ret = nearHostCall(c_key, c_value);
            result = String::NewFromUtf8(isolate, ret, NewStringType::kNormal).ToLocalChecked();
        }

        args.GetReturnValue().Set(result);
    }


// /node/test/addons/async-hello-world/binding.cc
    struct async_req {
        uv_work_t req;
        std::string name;
        std::string value;
        Isolate *isolate;
        // Persistent<Function> callback;
    };

    static void DoAsync(uv_work_t *r) {
        async_req *req = reinterpret_cast<async_req *>(r->data);
        // printf("DoAsync\n");
    }


    static void AfterAsync(uv_work_t *r, int status) {
        // FIXME : check the node.js is still alive

        // printf("AfterAsync\n");
        async_req *req = reinterpret_cast<async_req *>(r->data);

        if (eventListeners->count(req->name) == 0) {
            delete req;
            return;
        }

        Isolate *isolate = req->isolate;
        auto context = isolate->GetCurrentContext();

        HandleScope scope(isolate);

        std::vector<Local<Value>> argv;
        Local<Value> argument = String::NewFromUtf8(isolate, req->value.c_str());  //value.c_str()
        argv.push_back(argument);


        TryCatch try_catch(isolate);
        Local<Value> result;

        Local<Function> callback = Local<Function>::New(isolate, eventListeners->at(req->name));
        result = callback->Call(context->Global(), argv.size(), argv.data());

        // // cleanup
        // req->callback.Reset();
        delete req;

        if (try_catch.HasCaught()) {
            FatalException(isolate, try_catch);
        }
    }

    int nearJSEmit(const char *name, const char *value) {
        async_req *req = new async_req;
        req->req.data = req;
        req->isolate = isolate_; // FIXME : ...

        req->name = std::string(name);  // FIXME : use pointer
        req->value = std::string(value);

        uv_queue_work(loop,
                      &req->req,
                      DoAsync,
                      AfterAsync);
        return NEAR_OK;
    }

    static void OnMethod(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        HandleScope scope(isolate);

        auto context = isolate->GetCurrentContext();
        auto global = context->Global();

        if (args.Length() != 2) {
            args.GetIsolate()->ThrowException(String::NewFromUtf8(isolate, "Wrong Arguments"));
            return;
        }
        if (!args[0]->IsString()) {
            args.GetIsolate()->ThrowException(String::NewFromUtf8(isolate, "Argument 1 must be a string"));
            return;
        }
        if (!args[1]->IsFunction()) {
            args.GetIsolate()->ThrowException(String::NewFromUtf8(isolate, "Argument 2 must be a function"));
            return;
        }

        String::Utf8Value name(args[0]);

        Local<Function> callback = Local<Function>::Cast(args[1]);
        (*eventListeners)[std::string(*name)].Reset(isolate, callback);

        // FIXME : remove it in removeListener()
        //eventListeners[name].Reset();
    }

    static void HostOnMethod(const FunctionCallbackInfo<Value> &args) {
        Isolate *isolate = args.GetIsolate();
        HandleScope scope(isolate);

        auto context = isolate->GetCurrentContext();
        auto global = context->Global();

        if (args.Length() != 1) {
            args.GetIsolate()->ThrowException(String::NewFromUtf8(isolate, "Wrong Arguments"));
            return;
        }
        if (!args[0]->IsString()) {
            args.GetIsolate()->ThrowException(String::NewFromUtf8(isolate, "Argument 1 must be a string"));
            return;
        }

        Local<FunctionTemplate> ft = v8::FunctionTemplate::New(isolate,
                                                               [](const FunctionCallbackInfo<Value> &args) -> void {
                                                                   Isolate *isolate = args.GetIsolate();
                                                                   auto context = isolate->GetCurrentContext();
                                                                   String::Utf8Value name(args.Data());

                                                                   if (hostEventListeners->count(*name) == 0)
                                                                       return;

                                                                   //FIXME : better way?
                                                                   char **cargv = new char *[args.Length()];
                                                                   for (int i = 0; i < args.Length(); i++) {
                                                                       Local<Value> result = Stringify(isolate, context,
                                                                                                       args[i]);
                                                                       String::Utf8Value arg(result);
                                                                       char *carg = new char[strlen(*arg) + 1];
                                                                       strcpy(carg, *arg);
                                                                       cargv[i] = carg;
                                                                   }

                                                                   // call the host function
                                                                   (*hostEventListeners)[std::string(*name)](
                                                                           args.Length(), cargv);

                                                                   //delete
                                                                   for (int i = 0; i < args.Length(); i++)
                                                                       delete cargv[i];
                                                                   delete[] cargv;
                                                               }, args[0]); // pass the name(args[0]) as data

        v8::Local<v8::Function> fn = ft->GetFunction();
        args.GetReturnValue().Set(fn);
    }

    static void atExitCB(void *arg) {
        Isolate *isolate = static_cast<Isolate *>(arg);
        HandleScope handle_scope(isolate);
        Local<Object> obj = Object::New(isolate);
        assert(!obj.IsEmpty());  // Assert VM is still alive.
        assert(obj->IsObject());
    }

    void nearSetMethod(const char *methodName, methodPointer pointer) {
        userMethods.push_back(UserMethod(methodName, pointer));
    }

    void nearReturn(v8::FunctionCallbackInfo<v8::Value> &args, const char * returnValue){
        v8::Isolate *isolate = args.GetIsolate();
        args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, returnValue));
    }

    static void init(Local<Object> exports) {
        AtExit(atExitCB, exports->GetIsolate());

        NODE_SET_METHOD(exports, "hostCall", HostCallMethod);
        NODE_SET_METHOD(exports, "hostOn", HostOnMethod);
        NODE_SET_METHOD(exports, "on", OnMethod);

        for (auto method = userMethods.begin(); method != userMethods.end(); ++method) {
            NODE_SET_METHOD(exports, method->methodName, method->method);
        }

        exports->DefineOwnProperty(
                exports->GetIsolate()->GetCurrentContext(),
                String::NewFromUtf8(exports->GetIsolate(), "version"),
                String::NewFromUtf8(exports->GetIsolate(), NEAR_VERSION),
                v8::ReadOnly).FromJust();

        // call the host's OnLoad()
        if (nearOnLoad)
            nearOnLoad(isolate_);
    }

    static void _node(const char *processName, const char *userScript) {
        // argv memory should be adjacent.
        // libuv/src/unix/proctitle.c
        int _argc = 3;
        char *_argv[3]; //_argc

        // FIXME : leave empty string in evalScript(-e "")
        char evalScript[] = "";
        char nodeOptions[] = "-e";

        int size = 0;
        size += strlen(processName) + 1;
        size += strlen(nodeOptions) + 1;
        size += strlen(evalScript) + 1;

        char *buf = new char[size];
        int i = 0;

        _argv[0] = buf + i;
        strncpy(buf + i, processName, strlen(processName));
        i += strlen(processName);
        buf[i++] = '\0';

        _argv[1] = buf + i;
        strncpy(buf + i, nodeOptions, strlen(nodeOptions));
        i += strlen(nodeOptions);
        buf[i++] = '\0';

        _argv[2] = buf + i;
        strncpy(buf + i, evalScript, strlen(evalScript));
        i += strlen(evalScript);
        buf[i++] = '\0';

        {
            using namespace std;
            using namespace node;
            using namespace v8;
            using namespace near;

            static Platform *platform_;
            platform_ = platform::CreateDefaultPlatform();  //v8_default_thread_pool_size = 4;
            V8::InitializePlatform(platform_);
            V8::Initialize();

            Isolate::CreateParams params;
            params.array_buffer_allocator = new ArrayBufferAllocator();

            isolate_ = Isolate::New(params);
            // // FIXME : check isolate is null or not
            // if (isolate == nullptr)
            //   return 12;  // Signal internal error.

            Locker locker(isolate_);
            Isolate::Scope isolate_scope(isolate_);
            HandleScope handle_scope(isolate_);
            //static Local<Context> context = Context::New(isolate_);
            context = Context::New(isolate_);
            Context::Scope context_scope(context);

            int exec_argc;
            const char **exec_argv;
            Init(&_argc, const_cast<const char **>(_argv), &exec_argc, &exec_argv);

            static Environment *env = CreateEnvironment(
                    isolate_, loop, context, _argc, _argv, exec_argc, exec_argv);

            LoadEnvironment(env);

            /* inject the near script / user script here */
            {
                auto global = context->Global();
                Local<Object> nearObject = Object::New(isolate_);
                global->DefineOwnProperty(
                        context,
                        String::NewFromUtf8(isolate_, "near"),
                        nearObject,
                        static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete)
                ).FromJust();

                init(nearObject);
            }
            {
                // 'node/lib/internal/bootstrap_node.js' is lazily loaded.
                std::string source = "";
                source += "process.nextTick(function() {";
                source += userScript;
                source += "});";

                Isolate *isolate = static_cast<Isolate *>(isolate_);
                Local<Value> result;

                HandleScope handle_scope(isolate);
                TryCatch try_catch(isolate);

                Local<Context> context(isolate->GetCurrentContext());

                Local<String> script = String::NewFromUtf8(isolate, source.c_str());
                Local<Script> compiled_script;
                if (!Script::Compile(context, script).ToLocal(&compiled_script)) {
                    String::Utf8Value error(try_catch.Exception());
                    fprintf(stderr, "Fatal Error in userScript\n%s", *error);
                    fflush(stderr);
                    return;
                }

                if (!compiled_script->Run(context).ToLocal(&result)) {
                    String::Utf8Value error(try_catch.Exception());
                    fprintf(stderr, "Fatal Error in userScript\n%s", *error);
                    fflush(stderr);
                    return;
                }
            }
            /* */

            bool more;
            do {
                platform::PumpMessageLoop(platform_, isolate_);
                more = uv_run(loop, UV_RUN_ONCE);
                if (more == false) {
                    platform::PumpMessageLoop(platform_, isolate_);
                    EmitBeforeExit(env);

                    more = uv_loop_alive(loop);
                    if (uv_run(loop, UV_RUN_NOWAIT) != 0)
                        more = true;
                }
            } while (more == true);

            const int exit_code = EmitExit(env);
            RunAtExit(env);
            //return exit_code;

            if (nearOnUnload)
                nearOnUnload(isolate_, exit_code);
        }
    }

    void nearInit(const char *processName,
                  const char *userScript,
                  NearOnloadCallback _nearOnLoad,
                  NearOnunloadCallback _nearOnUnload,
                  NearHostCallCallback _nearHostCall) {
        // initialized the eventListeners map
        eventListeners = new Callback;
        hostEventListeners = new HostCallback;

        // set the default event loop.
        loop = uv_default_loop();

        near::nearOnLoad = _nearOnLoad;
        near::nearOnUnload = _nearOnUnload;
        near::nearHostCall = _nearHostCall;

        // start the node.js in a thread
        std::thread n(_node,
                      (!processName) ? "near" : processName,
                      userScript);
        n.detach();
    }

}  // namespace near