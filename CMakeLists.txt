cmake_minimum_required(VERSION 3.10)
project(MQTTExample)

# These will only be built if SSL selected
if(PAHO_WITH_SSL)
    set(SSL_EXECUTABLES ssl_publish)
endif()

find_package(PahoMqttCpp REQUIRED)
pkg_check_modules(LIBGPIOD REQUIRED libgpiod)

set (EXECUTABLES
    auto-infusion
)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")


## Build the example apps
add_executable(auto-infusion ${SOURCES})
target_link_libraries(auto-infusion PahoMqttCpp::paho-mqttpp3 gpiod)

set_target_properties(auto-infusion PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(PAHO_BUILD_SHARED)
    target_compile_definitions(auto-infusion PRIVATE PAHO_MQTTPP_IMPORTS)
endif()

## Extra configuration for the SSL/TLS examples, if selected
foreach(EXECUTABLE ${SSL_EXECUTABLES})
    target_compile_definitions(EXECUTABLE PUBLIC OPENSSL)
endforeach()

## install binaries
# include(GNUInstallDirs)