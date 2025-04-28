cmake_minimum_required(VERSION 3.10)
project(MQTTExample)

# These will only be built if SSL selected

find_package(GSL REQUIRED)
find_package(OpenCV REQUIRED)

if(PAHO_WITH_SSL)
    set(SSL_EXECUTABLES ssl_publish)
endif()

find_package(PahoMqttCpp REQUIRED)
pkg_check_modules(LIBGPIOD REQUIRED libgpiod)

set (EXECUTABLES
    auto-infusion
)

include_directories(include ${OpenCV_INCLUDE_DIRS})
add_subdirectory(thirdparty)
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

# 从SOURCES中排除掉不需要的文件
foreach (file IN LISTS SOURCES)
    if (file MATCHES ".*pump_calibration.cpp")
        list(REMOVE_ITEM SOURCES ${file})
    endif()
endforeach()

## Build the example apps
add_executable(auto-infusion ${SOURCES})
target_link_libraries(auto-infusion PahoMqttCpp::paho-mqttpp3 gpiod nlohmann_json::nlohmann_json ${GSL_LIBRARIES} ${GSL_CBLAS_LIBRARIES})

add_executable(pump_calibration "src/pump_calibration.cpp" "src/pump_database.cpp")
target_link_libraries(pump_calibration nlohmann_json::nlohmann_json ${GSL_LIBRARIES} ${GSL_CBLAS_LIBRARIES})

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