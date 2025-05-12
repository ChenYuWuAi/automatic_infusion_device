cmake_minimum_required(VERSION 3.10)
project(MQTTExample)

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

# ✅【修改1】根据平台决定是否引入 libgpiod
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

# 添加 include 目录
include_directories(
    include
    ${OpenCV_INCLUDE_DIRS}
    ${GSL_INCLUDE_DIR}
)

# 添加第三方目录
add_subdirectory(thirdparty/json)

# 搜索源文件
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

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
    nlohmann_json::nlohmann_json
)
target_link_libraries(auto-infusion
    PahoMqttCpp::paho-mqttpp3
    nlohmann_json::nlohmann_json
    ${GSL_LIBRARY}
    ${GSL_CBLAS_LIBRARY}
    ${OpenCV_LIBS}
    ${LIBGPIOD_LIBRARIES}
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
