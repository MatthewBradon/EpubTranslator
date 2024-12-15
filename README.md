# Epub Translator
A Japanese to English Epub Translator application using local AI models

It is important to build using Release. I do not know why but when installing the packages using debug the python will not have core modules like _socket which is needed during runtime for libraries such as transformers
```
cmake --build build --config Release
```

If using VSCode make sure to include in your settings
```
    "cmake.configureArgs": ["-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"],
```