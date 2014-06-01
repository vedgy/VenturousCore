cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# Depends on TemplateUtilities, Qt, QtCoreUtilities.
# NOTE: projects that use ItemTree-inl.hpp must
# include_directories(${TemplateUtilities_PublicHeaders_Path})
set(Target_Name VenturousCore)
set(CAP_Target_Name VENTUROUS_CORE)
message(<${Target_Name}>)

set(PATH_TO_MODULES modules)
set(PATH_TO_VEDG_TOOLS ${PATH_TO_MODULES}/vedgTools)

## vedgTools/CMakeModules path boilerplate.
include(vedgTools/CMakeLicense OPTIONAL RESULT_VARIABLE CMakeModulesWasIncluded)
if(CMakeModulesWasIncluded STREQUAL NOTFOUND)
    set(PATH_TO_CMAKE_MODULES
            ${CMAKE_CURRENT_SOURCE_DIR}/${PATH_TO_VEDG_TOOLS}/CMakeModules)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PATH_TO_CMAKE_MODULES})
    message("Including CMakeModules.")
    include(vedgTools/CMakeLicense)
endif()

# NOTE: QtCore is the only module, used by this library.
include(vedgTools/LibraryQtStart)

include(vedgTools/LibraryTemplateUtilitiesStart)
include(vedgTools/LibraryQtCoreUtilitiesStart)

option(DEBUG_VENTUROUS "Print details of internal workflow to stdout." OFF)
message("DEBUG_VENTUROUS = " ${DEBUG_VENTUROUS})

include(vedgTools/LibraryWithQtInit)


include(vedgTools/LibraryFindQt)
libraryFindQt(Qt5Core . QTCORE)

set(PATH_TO_TEMPLATE_UTILITIES ${PATH_TO_VEDG_TOOLS}/TemplateUtilities)
include(vedgTools/LibraryGetTemplateUtilitiesPublicHeaders)
set(PATH_TO_QT_CORE_UTILITIES ${PATH_TO_VEDG_TOOLS}/QtCoreUtilities)
include(vedgTools/LibraryGetQtCoreUtilitiesPublicHeaders)

set(CMAKE_AUTOMOC ON)


if(DEBUG_VENTUROUS)
    add_definitions(
        -DDEBUG_VENTUROUS_HISTORY -DDEBUG_VENTUROUS_MEDIA_PLAYER
        -DDEBUG_VENTUROUS_ADDING_ITEMS
    )
endif()

include(vedgTools/AddErrorPrefixDefinition)


set(Headers_Path ${${Target_Name}_PublicHeaders_Path}/${Target_Name})
set(Sources_Path src)
set(MediaPlayer_Path ${Sources_Path}/MediaPlayer)
set(Audacious_Path ${MediaPlayer_Path}/Audacious)

include_directories(
    ${PATH_TO_CMAKE_MODULES}/include
    ${TemplateUtilities_PublicHeaders_Path}
    ${QtCoreUtilities_PublicHeaders_Path}
    ${${Target_Name}_PublicHeaders_Path}
    ${Headers_Path}
    ${MediaPlayer_Path} ${Audacious_Path}
)

set(Sources
    ${Sources_Path}/ItemTree.cpp ${Sources_Path}/History.cpp
    ${Sources_Path}/AddingItems.cpp ${MediaPlayer_Path}/MediaPlayer.cpp
    ${Audacious_Path}/Audacious.cpp ${Audacious_Path}/DetachedAudacious.cpp
    ${Audacious_Path}/ManagedAudacious.cpp
)

qt_wrap_cpp(${Target_Name} Sources ${Headers_Path}/MediaPlayer.hpp)


include(vedgTools/LibraryAddTarget)

include(vedgTools/LinkQt)
linkQt(${Target_Name} Core . ${QT_QTCORE_LIBRARY})

include(vedgTools/LibraryLinkQtCoreUtilitiesToTarget)


set_target_properties(${Target_Name} PROPERTIES
                        PUBLIC_HEADER
    "ItemTree.hpp;ItemTree-inl.hpp;History.hpp;AddingItems.hpp;MediaPlayer.hpp")

message(</${Target_Name}>)
