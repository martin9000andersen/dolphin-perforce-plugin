project(fileviewperforceplugin)
# project(fileviewperforceplugin)

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${LIBKONQ_INCLUDE_DIR} )

set(fileviewperforceplugin_SRCS fileviewperforceplugin.cpp)
kde4_add_plugin(fileviewperforceplugin  ${fileviewperforceplugin_SRCS})
target_link_libraries(fileviewperforceplugin ${KDE4_KIO_LIBS} ${LIBKONQ_LIBRARY})
install(FILES fileviewperforceplugin.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(TARGETS fileviewperforceplugin DESTINATION ${PLUGIN_INSTALL_DIR})
