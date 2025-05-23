cmake_minimum_required(VERSION 3.10)
project(AutoInfusionPump)

# C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ccache
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
set(CMAKE_C_COMPILER_LAUNCHER ccache)

# ninja
set(CMAKE_GENERATOR "Ninja")


# 查找 GSL（GNU Scientific Library）
find_path(GSL_INCLUDE_DIR gsl/gsl_math.h)
find_library(GSL_LIBRARY NAMES gsl)
find_library(GSL_CBLAS_LIBRARY NAMES gslcblas)

if(NOT GSL_INCLUDE_DIR OR NOT GSL_LIBRARY OR NOT GSL_CBLAS_LIBRARY)
    message(FATAL_ERROR "GSL not found")
endif()

# OpenCV 必需
find_package(OpenCV REQUIRED)

# SSL 开关示例
if(PAHO_WITH_SSL)
    set(SSL_EXECUTABLES ssl_publish)
endif()

if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBGPIOD REQUIRED libgpiod)
    set(HAVE_GPIOD TRUE)
else()
    message(WARNING "Skipping libgpiod: only available on Linux.")
    set(HAVE_GPIOD FALSE)
endif()

# MQTT
find_package(PahoMqttCpp CONFIG REQUIRED)

# 寻找 spdlog 包
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

pkg_check_modules(LIBCAMERA REQUIRED libcamera)

# 添加 include 目录
include_directories(
    include
    ${OpenCV_INCLUDE_DIRS}
    ${GSL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# 添加第三方目录
add_subdirectory(thirdparty/json)

# 搜索源文件
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c" "thirdparty/LCCV/src/*.cpp"
    "thirdparty/pn532/pn532_rpi.c"
    "thirdparty/pn532/pn532.c"
    "thirdparty/pn532/serial.c"
)


# 从源文件中排除特定文件
foreach(file IN LISTS SOURCES)
    if(file MATCHES ".*pump_calibration.cpp")
        list(REMOVE_ITEM SOURCES ${file})
    endif()
endforeach()

# 主程序构建
add_executable(auto-infusion ${SOURCES})

target_include_directories(auto-infusion PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${GSL_INCLUDE_DIR}
    ${GSL_CBLAS_INCLUDE_DIR}
    ${LIBCAMERA_INCLUDE_DIRS}
    nlohmann_json::nlohmann_json
    ${CMAKE_CURRENT_LIST_DIR}/thirdparty/LCCV/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/pn532
)

target_link_libraries(auto-infusion
    PahoMqttCpp::paho-mqttpp3
    nlohmann_json::nlohmann_json
    ${GSL_LIBRARY}
    ${GSL_CBLAS_LIBRARY}
    ${OpenCV_LIBS}
    ${LIBGPIOD_LIBRARIES}
    ${LIBCAMERA_LINK_LIBRARIES}
    spdlog::spdlog
)

# 单独编译 pump_calibration 程序
add_executable(pump_calibration
    "src/pump_calibration.cpp"
    "src/pump_database.cpp"
)
target_link_libraries(pump_calibration
    nlohmann_json::nlohmann_json
    ${GSL_LIBRARY}
    ${GSL_CBLAS_LIBRARY}
)

# 设置编译选项
set_target_properties(auto-infusion PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 定义 SSL 宏（可选）
if(PAHO_BUILD_SHARED)
    target_compile_definitions(auto-infusion PRIVATE PAHO_MQTTPP_IMPORTS)
endif()

# 针对 SSL 额外定义
foreach(EXECUTABLE ${SSL_EXECUTABLES})
    target_compile_definitions(${EXECUTABLE} PUBLIC OPENSSL)
endforeach()
