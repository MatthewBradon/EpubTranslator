cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
project(main)

# Add the executable
add_executable(${PROJECT_NAME} main.cpp main.h GUI.cpp GUI.h)

# Find the required packages
find_package(libzip CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS process filesystem system)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${libzip_INCLUDE_DIRS}
    ${imgui_INCLUDE_DIRS}
)

# Print Python3 details
message(STATUS "Python3_FOUND: ${Python3_FOUND}")
message(STATUS "Python3_VERSION: ${Python3_VERSION}")
message(STATUS "Python3_EXECUTABLE: ${Python3_EXECUTABLE}")
message(STATUS "Python3_INCLUDE_DIR: ${Python3_INCLUDE_DIR}")
message(STATUS "Python3_LIBRARY: ${Python3_LIBRARY}")


if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # Install Python dependencies using requirements.txt for macOS
    add_custom_target(InstallPythonPackages ALL
        COMMAND ${Python3_EXECUTABLE} -m venv .venv                    # Create virtual environment
        COMMAND ${CMAKE_SOURCE_DIR}/build/.venv/bin/python3 -m pip install --upgrade pip   # Upgrade pip in the virtual environment
        COMMAND ${CMAKE_SOURCE_DIR}/build/.venv/bin/python3 -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt  # Install dependencies
        COMMENT "Installing required Python packages from requirements.txt on macOS"
    )

elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    # Install Python dependencies using requirements.txt for Windows
    add_custom_target(InstallPythonPackages ALL
        COMMAND ${Python3_EXECUTABLE} -m venv .venv                    # Create virtual environment
        COMMAND ${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe -m pip install --upgrade pip   # Upgrade pip in the virtual environment
        COMMAND ${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt  # Install dependencies
        COMMENT "Installing required Python packages from requirements.txt on Windows"
    )
endif()

# Ensure the Python packages are installed before building the executable
add_dependencies(${PROJECT_NAME} InstallPythonPackages)



# Link libraries
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(${PROJECT_NAME} PRIVATE Boost::process Boost::filesystem Boost::system nfd::nfd glad::glad glfw imgui::imgui libzip::zip LibXml2::LibXml2 Python3::Python pybind11::embed)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE Boost::process Boost::filesystem Boost::system nfd::nfd glad::glad glfw imgui::imgui libzip::zip LibXml2::LibXml2 Python3::Python pybind11::embed)
endif()

# Set output directory for the executable
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)

# Ensure the Python packages are installed before building the executable
add_dependencies(${PROJECT_NAME} InstallPythonPackages)
