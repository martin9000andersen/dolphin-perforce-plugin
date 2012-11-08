cmake_minimum_required(VERSION 2.6)
project(fileviewperforceplugin)

find_package(KDE4 REQUIRED)
find_package(LibKonq REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})
include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${LIBKONQ_INCLUDE_DIR} )

set(fileviewperforceplugin_SRCS fileviewperforceplugin.cpp)
kde4_add_plugin(fileviewperforceplugin  ${fileviewperforceplugin_SRCS})
target_link_libraries(fileviewperforceplugin ${KDE4_KIO_LIBS} ${LIBKONQ_LIBRARY})

install(FILES fileviewperforceplugin.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(TARGETS fileviewperforceplugin DESTINATION ${PLUGIN_INSTALL_DIR})
