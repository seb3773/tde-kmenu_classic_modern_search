#################################################
#
#  (C) 2010-2011 Serghei Amelian
#  serghei (DOT) amelian (AT) gmail.com
#
#  Improvements and feedback are welcome
#
#  This file is released under GPL >= 2
#
#################################################

project( kicker )


##### subdirectories ############################

add_subdirectory( libkicker )
add_subdirectory( taskmanager )
add_subdirectory( taskbar )
add_subdirectory( kicker )
add_subdirectory( extensions )
add_subdirectory( applets )
add_subdirectory( menuext )
add_subdirectory( proxy )
add_subdirectory( data )

install( FILES
    menu-docs.png
    menu-images.png
    menu-logout.png
    menu-restart.png
    menu-sleep.png
    menu-hibernate.png
    menu-hybrid.png
    kickermenu-logout.png
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/32x32/apps )

##### install import cmake modules ###############

tde_install_export( )
