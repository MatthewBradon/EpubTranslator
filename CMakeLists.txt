cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
project(main)

# Add the executable
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/GUI.cpp 
    src/PDFTranslator.cpp 
    src/EpubTranslator.cpp 
)

# Include directories for headers
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Find the required packages
find_package(libzip CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS process filesystem system)
find_package(Stb REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)


# Cairo configuration
pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(cairo-script-interpreter REQUIRED IMPORTED_TARGET cairo-script-interpreter)




if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
# Include the MuPDF headers
set(MUPDF_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/macBuild/include")
set(MUPDF_LIB_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/macBuild/lib")
file(GLOB MUPDF_LIBS "${MUPDF_LIB_DIR}/*.a")




elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
# MuPDF configuration
set(MUPDF_LIB_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/platform/win32/x64/Release")
set(MUPDF_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/include")
include_directories("${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows/include")
file(GLOB MUPDF_LIBS "${MUPDF_LIB_DIR}/*.lib")

endif()

# Include the MuPDF headers
include_directories(${MUPDF_INCLUDE_DIR})



# Add Llama.cpp
add_subdirectory(${CMAKE_SOURCE_DIR}/external/llama.cpp)

set(LLAMACPP_LIB_DIR "${CMAKE_SOURCE_DIR}/external/llama.cpp/build/src/Release")

# Include directories for Llama.cpp
include_directories(${CMAKE_SOURCE_DIR}/external/llama.cpp/include)
include_directories(${CMAKE_SOURCE_DIR}/external/llama.cpp/ggml/include/)


# Link libraries
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
target_link_libraries(${PROJECT_NAME} PRIVATE
        CURL::libcurl nlohmann_json::nlohmann_json
        PkgConfig::cairo PkgConfig::cairo-script-interpreter
        Boost::process Boost::filesystem Boost::system 
        nfd::nfd glad::glad glfw imgui::imgui 
        libzip::zip LibXml2::LibXml2
        ${MUPDF_LIBS} # Include all MuPDF libraries
        ${LLAMACPP_LIB_DIR}
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")

    file(GLOB LLAMACPP_LIBS "${LLAMACPP_LIB_DIR}/*.lib")

    target_link_libraries(${PROJECT_NAME} PRIVATE
        CURL::libcurl 
        PkgConfig::cairo PkgConfig::cairo-script-interpreter
        Boost::process Boost::filesystem Boost::system 
        nfd::nfd glad::glad glfw imgui::imgui 
        libzip::zip LibXml2::LibXml2
        ${MUPDF_LIBS} # Include all MuPDF libraries
        ${LLAMACPP_LIBS}
        )
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows/include)
endif()

# Set output directory for the executable
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)


# Test configuration
enable_testing()

# Add the test executable
add_executable(BookTranslatorTest
    tests/BookTranslatorTests.cpp
    src/EpubTranslator.cpp
    src/GUI.cpp
    src/PDFTranslator.cpp
)

# Include directories for headers
target_include_directories(BookTranslatorTest PRIVATE src)

# Link test executable with required libraries
target_link_libraries(BookTranslatorTest PRIVATE
    CURL::libcurl nlohmann_json::nlohmann_json
    Catch2::Catch2 Catch2::Catch2WithMain
    PkgConfig::cairo PkgConfig::cairo-script-interpreter
    Boost::process Boost::filesystem Boost::system 
    nfd::nfd glad::glad glfw imgui::imgui 
    libzip::zip LibXml2::LibXml2
    ${MUPDF_LIBS} # Include all MuPDF libraries
)

target_include_directories(BookTranslatorTest PRIVATE ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows/include)


target_compile_definitions(BookTranslatorTest PRIVATE TESTING)

set_target_properties(BookTranslatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)

add_test(NAME BookTranslatorTests COMMAND BookTranslatorTest)