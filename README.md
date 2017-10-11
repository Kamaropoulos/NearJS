# NodeJS Embeded Application Runtime for C++

## Example - Simple
```c++
#include <unistd.h>

#include <near.hpp>

int main(int argc, char *argv[]) {
  const char* userScript = "console.log(process.version)";
  nearInit(nullptr, userScript, nullptr, nullptr, nullptr);

  usleep(1000*1000);
  return 0;
}
```

## Example - Full
```c++
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


using namespace std;

void nearOnLoad(void* isolate) {
  cout << "\e[32m" << "** nearOnLoad : " << isolate << endl;

  // custom source
  const char* source = "function _f(x) {"
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

void nearOnUnload(void* isolate, int exitCode) {
  cout << "\e[31m" << "** nearOnUnload : " << isolate;
  cout << " exitCode : " << exitCode << endl;
  cout << "\e[0m" << endl << flush;
}

char* nearHostCall(const char* name, const char* value) {
  cout << "\e[93m" << "** from javascript. name = " << name;
  cout << " , value = " << value << "\e[0m";
  cout << endl << flush;

  char* data = new char[10];
  strcpy(data, "hi there");
  return data;
}


int main(int argc, char *argv[]) {
  const char* userScript = "require('./app.js');";

  nearInit(argv[0],
           userScript,
           nearOnLoad,
           nearOnUnload,
           nearHostCall);

  // dummy loop in host
  static int i = 0;
  while(true) {
    SLEEP_ONE_SECOND;
    nearJSEmit("test", to_string(i++).c_str());
  }

  return 0;
}
```
#### app.js
```javascript
'use strict'

// print near.version
console.log(`node :: near.version = ${near.version}`);

// assgined from example.cpp
console.log(`node :: _v = ${_v}`);

var num = 42;
var foo = 'foo';

near.on('test', function(x){
  console.log(`node :: near.on(test) = ${x}`);
});

var result = near.hostCall('dory', {num, foo});
console.log(`node :: near.hostCall() = ${result}`);

// exit after 2 secs
(function(){setTimeout(function(){
	process.exitCode = 42;
},2000)})();
```
#### Output
```
** nearOnLoad : 0x7f9fe0001f10
** nearJSCompile : undefined
** nearJSCall : ":)"
node :: near.version = 0.1.7
node :: _v = 43
** from javascript. name = dory , value = {"num":42,"foo":"foo"}
node :: near.hostCall() = hi there
node :: near.on(test) = 0
node :: near.on(test) = 1
** nearOnUnload : 0x7f9fe0001f10 exitCode : 42
```



## BUILD
### Build node.js v6.11.4 LTS
```
$ git clone https://github.com/nodejs/node.git
$ cd node && git checkout tags/v6.11.4
$ ./configure --shared
$ make

# or ./configure --shared --debug
# for gdb/lldb debugging

$ cp out/Release/obj.target/libnode.so.48 /usr/lib/
$ ln -s /usr/lib/libnode.so.48 /usr/lib/libnode.so

```
