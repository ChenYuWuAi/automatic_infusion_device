cmake_minimum_required(VERSION 3.5)

project(pn532linux LANGUAGES C)

add_executable(pn532linux
    main.c pn532.c pn532_rpi.c serial.c
    pn532.h pn532.h pn532_rpi.h serial.h
)

include(GNUInstallDirs)
install(TARGETS pn532linux
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
