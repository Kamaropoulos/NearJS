# Embed Node.js into C++

build node.js
```
./configure --shared
make

```
#### copy node/out/Release/obj.target/libnode.so.51

## example
linux
```
clang++ toby.cpp -c -o toby.o \
--std=c++11 -fPIC \
-I../node/deps/v8/include/ \
-I../node/src/ -g \
&& clang++ example.cpp -o example \
--std=c++11 \
-I../node/deps/v8/include/ \
-I../node/src/ -g ./libnode.so.51 toby.o \
-Wl,-rpath=. -ldl -lpthread \
&& ./example
```

mac
```
clang++ toby.cpp -c -o toby.o \
--std=c++11 -fPIC \
-I../node/deps/v8/include/ \
-I../node/src/ -g \
&& clang++ example.cpp -o example \
--std=c++11 \
-I../node/deps/v8/include/ \
-I../node/src/ -g ./libnode.51.dylib toby.o \
-ldl -lpthread \
&& DYLD_LIBRARY_PATH=. ./a.out
```