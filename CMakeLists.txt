CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)

IF(POLICY CMP0043)
        # Don't ignore COMPILE_DEFINITIONS_{CONFIG} directives, as these are used
        # by Qt.
        CMAKE_POLICY(SET CMP0043 OLD)
ENDIF()

IF (POLICY CMP0071)
        CMAKE_POLICY(SET CMP0071 OLD)
ENDIF()

if(${CMAKE_VERSION} VERSION_EQUAL "3.11.0" OR ${CMAKE_VERSION} VERSION_GREATER "3.11.0")
IF (POLICY CMP0072)
        CMAKE_POLICY(SET CMP0072 OLD)
ENDIF()
endif()

SET(
        CMAKE_USER_MAKE_RULES_OVERRIDE
        "${CMAKE_SOURCE_DIR}/cmake/default_cflags.cmake"
)
SET(
        CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
        "${CMAKE_SOURCE_DIR}/cmake/default_cxxflags.cmake"
)

PROJECT("Scan Tailor Universal")

IF(MSVC)
        # Disable checked iterators for extra performance.
        ADD_DEFINITIONS(-D_SECURE_SCL=0)
ENDIF()

IF(DEBUG_CLI)
        ADD_DEFINITIONS(-DDEBUG_CLI)
ENDIF(DEBUG_CLI)

ENABLE_TESTING()

# An undocumented side-effect of CONFIGURE_FILE() is that it makes
# the whole project depend on the file we are parsing / copying.
CONFIGURE_FILE(
        "${PROJECT_SOURCE_DIR}/version.h"
        "${PROJECT_BINARY_DIR}/.version.h" COPYONLY
)

# Prevent this leftover from old builds to be used in favour
# of the one in ${PROJECT_SOURCE_DIR}
IF(NOT "${PROJECT_BINARY_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
        FILE(REMOVE "${PROJECT_BINARY_DIR}/version.h")
ENDIF()

# Extract VERSION and VERSION_QUAD from version.h
FILE(READ "${PROJECT_SOURCE_DIR}/version.h" version_h_contents)
STRING(
        REGEX REPLACE
        ".*#define[ \\t]+VERSION[ \\t]+\"([^\"]*)\".*"
        "\\1" VERSION "${version_h_contents}"
)
IF("${VERSION}" STREQUAL "${version_h_contents}")
        MESSAGE(FATAL_ERROR "Failed to extract VERSION from version.h")
ENDIF()

# VERSION_QUAD must be either empty or be in the form of X.Y.Z.Y
STRING(
        REGEX REPLACE
        ".*#define[ \\t]+VERSION_QUAD[ \\t]+\"(([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)?)\".*"
        "\\1" VERSION_QUAD "${version_h_contents}"
)
IF("${VERSION_QUAD}" STREQUAL "${version_h_contents}")
        MESSAGE(FATAL_ERROR "Failed to extract VERSION_QUAD from version.h")
ENDIF()

# For config.h
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}")

INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(TestCXXAcceptsFlag)
INCLUDE(cmake/FindPthreads.cmake)
IF(UNIX AND NOT APPLE)
INCLUDE(cmake/FindCanberra.cmake)
ENDIF(UNIX AND NOT APPLE)
INCLUDE(cmake/SetDefaultBuildType.cmake)
INCLUDE(cmake/SetDefaultGccFlags.cmake)
INCLUDE(cmake/ToNativePath.cmake)
INCLUDE(cmake/UpdateTranslations.cmake)
INCLUDE(cmake/CopyToBuildDir.cmake)
INCLUDE(cmake/LibToDLL.cmake)

ST_SET_DEFAULT_BUILD_TYPE(Release)
IF(CMAKE_COMPILER_IS_GNUCC)
        ST_SET_DEFAULT_GCC_FLAGS()
ENDIF(CMAKE_COMPILER_IS_GNUCC)

GET_FILENAME_COMPONENT(source_outer_dir "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
GET_FILENAME_COMPONENT(build_outer_dir "${PROJECT_BINARY_DIR}/.." ABSOLUTE)

SET(STAGING_LIBS_DIR "")


INCLUDE(FindOpenGL)
SET(use_opengl OFF)
IF(OPENGL_FOUND AND QT_QTOPENGL_FOUND)
        # Disabled for now due to various problems associated with it.
        # SET(use_opengl ON)
ENDIF()
OPTION(ENABLE_OPENGL "OpenGL may be used for UI acceleration" ${use_opengl})

IF(WIN32)
        FIND_PATH(
                DEPS_BUILD_DIR build-qt.bat
                HINTS "${build_outer_dir}/scantailor-universal-deps-build"
                DOC "Build directory for Scan Tailor dependencies."
        )
        IF(NOT DEPS_BUILD_DIR)
                MESSAGE(
                        FATAL_ERROR "The build directory for Scan Tailor dependencies could not be found! "
                        "You can specify it manually in DEPS_BUILD_DIR variable. Make sure you build the dependencies first!"
                )
        ENDIF()
        SET(STAGING_LIBS_DIR "${DEPS_BUILD_DIR}/staging/libs")
        INCLUDE("${DEPS_BUILD_DIR}/export-vars.cmake")

        IF (NOT QT_SRC_DIR OR NOT EXISTS "${QT_SRC_DIR}")
                FILE(GLOB qt_dirs1 "${build_outer_dir}/qt-win-*-5.[0-9]*")
                FILE(GLOB qt_dirs2 "${build_outer_dir}/qt-everywhere-*-5.[0-9]*")
                FILE(GLOB qt_dirs3 "${source_outer_dir}/qt-win-*-5.[0-9]*")
                FILE(GLOB qt_dirs4 "${source_outer_dir}/qt-everywhere-*-5.[0-9]*")
                FILE(GLOB qt_dirs5 "C:/Qt/Qt5.*/*/Src")
                FIND_PATH(
                        QT_SRC_DIR qtbase/qtbase.pro
                        HINTS ${qt_dirs1} ${qt_dirs2} ${qt_dirs3}
                        ${qt_dirs4} ${qt_dirs5} dummy_dir
                        DOC "Path to top-level Qt5 source directory. If you installed a binary version, it will be something like C:\\Qt\\Qt5.0.2\\5.0.2\\Src"
                )
        ENDIF()

        IF(NOT QT_SRC_DIR OR NOT EXISTS "${QT_SRC_DIR}")
                MESSAGE(
                        FATAL_ERROR "Qt5 could not be found. "
                        "If it's installed in a non-standard location, specify it QT_SRC_DIR variable."
                )
        ENDIF()

        FILE(GLOB qt_possible_prebuilt_dirs "${QT_SRC_DIR}/../*")
        FIND_PATH(
                QT_PREBUILT_DIR bin/qmake.exe HINTS ${qt_possible_prebuilt_dirs}
                DOC "[optional] Installation path of a pre-built version of Qt5. If you installed a binary version, it will be something like C:\\Qt\\Qt5.0.2\\5.0.2\\msvc2012"
        )


        SET(Qt5Core_DIR "${QT_SRC_DIR}/qtbase/lib/cmake/Qt5Core")
        SET(Qt5Gui_DIR "${QT_SRC_DIR}/qtbase/lib/cmake/Qt5Gui")
        SET(Qt5Widgets_DIR "${QT_SRC_DIR}/qtbase/lib/cmake/Qt5Widgets")
        SET(Qt5Xml_DIR "${QT_SRC_DIR}/qtbase/lib/cmake/Qt5Xml")
        SET(Qt5Network_DIR "${QT_SRC_DIR}/qtbase/lib/cmake/Qt5Network")
        SET(Qt5OpenGL_DIR "${QT_SRC_DIR}/qtbase/lib/cmake/Qt5OpenGL")
        IF(QT_PREBUILT_DIR)
                SET(Qt5LinguistTools_DIR "${QT_PREBUILT_DIR}/lib/cmake/Qt5LinguistTools")
        ELSE()
                SET(Qt5LinguistTools_DIR "${QT_SRC_DIR}/qtbase/lib/cmake/Qt5LinguistTools")
        ENDIF()
ENDIF()

SET(CMAKE_AUTOMOC ON)
SET(Qt_MIN_VERSION 5.7.0)
IF(APPLE)
   # hardcoded path to Qt5CoreConfig.cmake etc. [truf]
   SET(CMAKE_PREFIX_PATH "/Users/robert/build/Qt/5.8/clang_64/")
ENDIF(APPLE)
FIND_PACKAGE(Qt5Core ${Qt_MIN_VERSION} REQUIRED)
FIND_PACKAGE(Qt5Gui ${Qt_MIN_VERSION} REQUIRED)
FIND_PACKAGE(Qt5Widgets ${Qt_MIN_VERSION} REQUIRED)
FIND_PACKAGE(Qt5Xml ${Qt_MIN_VERSION} REQUIRED)
FIND_PACKAGE(Qt5Network ${Qt_MIN_VERSION} REQUIRED)
FIND_PACKAGE(Qt5Svg ${Qt_MIN_VERSION} REQUIRED)
FIND_PACKAGE(Qt5LinguistTools ${Qt_MIN_VERSION} REQUIRED)
IF(ENABLE_OPENGL)
        FIND_PACKAGE(Qt5OpenGL ${Qt_MIN_VERSION} REQUIRED)
ENDIF()

FIND_PATH(
        JPEG_INCLUDE_DIR jpeglib.h
	PATHS /usr/local/include /usr/include
	HINTS ${JPEG_DIR} # JPEG_DIR may come from export-vars.cmake
	DOC "Path to libjpeg headers."
)
IF(NOT JPEG_INCLUDE_DIR)
        MESSAGE(
                FATAL_ERROR
                "Could not find jpeg headers.\n"
		"You may need to install a package named libjpeg62-dev or similarly."
        )
ENDIF()

INCLUDE_DIRECTORIES("${JPEG_INCLUDE_DIR}")

FIND_LIBRARY(
        JPEG_LIBRARY NAMES jpeg libjpeg.lib
	PATHS /usr/local/lib /usr/lib
	HINTS ${STAGING_LIBS_DIR}
	DOC "Path to jpeg library."
)
IF(NOT JPEG_LIBRARY)
        MESSAGE(
                FATAL_ERROR
                "Could not find jpeg library.\n"
                "You may need to install a package named libjpeg62-dev or similarly."
        )
ENDIF()

SET (use_openjpeg ON)
OPTION(ENABLE_OPENJPEG "OpenJPEG library may be used to suppoet JPEG 2000 (.jp2) images." ${use_openjpeg})

IF (ENABLE_OPENJPEG)

IF(NOT WIN32)
FIND_PACKAGE(OpenJPEG QUIET)
ELSE(NOT WIN32)

FIND_PATH(
        OPENJPEG_INCLUDE_DIRS openjpeg.h
        PATHS /usr/local/include /usr/include
        HINTS ${JPEG2000_DIR} # JPEG2000_DIR may come from export-vars.cmake
        PATH_SUFFIXES src/lib/openjp2
        DOC "Path to openjpeg headers."
)
IF(NOT OPENJPEG_INCLUDE_DIRS)
        MESSAGE(
                "Could not find openjpeg headers.\n"
                "You may need to install a package openjpeg."
        )
ENDIF()

INCLUDE_DIRECTORIES("${OPENJPEG_INCLUDE_DIRS}")

FIND_LIBRARY(
        OPENJPEG_LIBRARIES openjp2 openjp2.lib
        PATHS /usr/local/lib /usr/lib
        HINTS ${STAGING_LIBS_DIR}
        PATH_SUFFIXES openjp2
        DOC "Path to openjpeg library."
)

IF(OPENJPEG_LIBRARIES AND OPENJPEG_INCLUDE_DIRS)
SET (OpenJPEG_FOUND YES)
SET (OPENJPEG_MAJOR_VERSION 2)
ENDIF(OPENJPEG_LIBRARIES AND OPENJPEG_INCLUDE_DIRS)

ENDIF(NOT WIN32)

IF(NOT OpenJPEG_FOUND OR OPENJPEG_MAJOR_VERSION VERSION_LESS 2)
    message("warning: OpenJPEG library isn't found.\n"
        "The JPEG 2000 (*.jp2) images support will be disabled.\n"
        "You may install libopenjp2-7-dev to fix this.\n"
        "Or disable this massege with -DENABLE_OPENJPEG=OFF")
    SET(ENABLE_OPENJPEG OFF)
ENDIF()
ENDIF(ENABLE_OPENJPEG)

FIND_PATH(
        ZLIB_INCLUDE_DIR zlib.h
	PATHS /usr/local/include /usr/include
	HINTS ${ZLIB_DIR} # ZLIB_DIR may come from export-vars.cmake
	DOC "Path to zlib headers."
)
IF(NOT ZLIB_INCLUDE_DIR)
        MESSAGE(
                FATAL_ERROR
                "Could not find zlib headers.\n"
                "You may need to install a package named zlib1g-dev or similarly."
        )
ENDIF()

INCLUDE_DIRECTORIES("${ZLIB_INCLUDE_DIR}")

FIND_LIBRARY(
        ZLIB_LIBRARY NAMES z zdll.lib
	PATHS /usr/local/lib /usr/lib
	HINTS ${STAGING_LIBS_DIR}
	DOC "Path to zlib library."
)
IF(NOT ZLIB_LIBRARY)
        MESSAGE(
                FATAL_ERROR
                "Could not find zlib library.\n"
                "You may need to install a package named zlib1g-dev or similarly."
        )
ENDIF()

FIND_PATH(
        PNG_INCLUDE_DIR png.h
	PATHS /usr/local/include /usr/include
	HINTS ${PNG_DIR} # PNG_DIR may come from export-vars.cmake
	DOC "Path to libpng headers."
)
IF(NOT PNG_INCLUDE_DIR)
        MESSAGE(
                FATAL_ERROR
                "Could not find libpng headers.\n"
                "You may need to install a package named libpng12-dev or similarly."
        )
ENDIF()

INCLUDE_DIRECTORIES("${PNG_INCLUDE_DIR}")

FIND_LIBRARY(
        PNG_LIBRARY NAMES png libpng.lib
	PATHS /usr/local/lib /usr/lib
	HINTS ${STAGING_LIBS_DIR}
	DOC "Path to png library."
)
IF(NOT PNG_LIBRARY)
        MESSAGE(
                FATAL_ERROR
                "Could not find libpng library.\n"
                "You may need to install a package named libpng12-dev or similarly."
        )
ENDIF()


FIND_PATH(
        TIFF_INCLUDE_DIR tiff.h
	PATHS /usr/local/include /usr/include
	HINTS ${TIFF_DIR} # TIFF_DIR may come from export-vars.cmake
	PATH_SUFFIXES libtiff
	DOC "Path to libtiff headers."
)
IF(NOT TIFF_INCLUDE_DIR)
        MESSAGE(
                FATAL_ERROR
                "Could not find libtiff headers.\n"
                "You may need to install a package named libtiff4-dev or similarly."
        )
ENDIF()

INCLUDE_DIRECTORIES("${TIFF_INCLUDE_DIR}")

FIND_LIBRARY(
        TIFF_LIBRARY tiff libtiff.lib
	PATHS /usr/local/lib /usr/lib
	HINTS ${STAGING_LIBS_DIR}
	PATH_SUFFIXES libtiff
	DOC "Path to tiff library."
)
IF(NOT TIFF_LIBRARY)
        MESSAGE(
                FATAL_ERROR
                "Could not find libtiff library.\n"
                "You may need to install a package named libtiff4-dev or similarly."
        )
ENDIF()

FIND_LIBRARY(
        DJVULIBRE_LIBRARY djvulibre libdjvulibre.lib
	PATHS /usr/local/lib /usr/lib
	HINTS ${STAGING_LIBS_DIR}
	PATH_SUFFIXES libdjvulibre
	DOC "Path to djvulibre library."
)
IF(NOT DJVULIBRE_LIBRARY)
        MESSAGE(
	        FATAL_ERROR
		"Could not find libdjvulibre library.\n"
		"You may need to install a package named libdjvulibre-dev or similarly."
		)
ENDIF()

IF(WIN32)
        ADD_DEFINITIONS(-DUSE_LIBTIFF_DLL)
ENDIF()


IF(WIN32)
        SET(Boost_USE_STATIC_LIBS ON)
ELSE(WIN32)
        ADD_DEFINITIONS(-DBOOST_TEST_DYN_LINK)
ENDIF(WIN32)

SET(Boost_USE_MULTITHREADED ON)
FIND_PACKAGE(Boost 1.35.0 COMPONENTS unit_test_framework prg_exec_monitor)
IF(NOT Boost_FOUND)
        MESSAGE(
                FATAL_ERROR
                "Could not find boost headers or libraries. "
                "You may need to install a package named libboost1.35-dev or similarly. "
                "Hint: create a Boost_DEBUG variable in cmake and set it to YES."
        )
ENDIF(NOT Boost_FOUND)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

SET(EXTRA_LIBS "")

IF(UNIX)
        FindPthreads()
	IF(PTHREADS_FOUND)
	        ADD_DEFINITIONS(${PTHREADS_CFLAGS})
		LINK_LIBRARIES(${PTHREADS_LIBS})
		ELSE(PTHREADS_FOUND)
		MESSAGE(
		        FATAL_ERROR
			"Could not detect threading flags.\n"
			"Try specifying them manually in PTHREADS_CFLAGS and PTHREADS_LIBS."
			)
	ENDIF(PTHREADS_FOUND)
ELSEIF(WIN32)
        ADD_DEFINITIONS(-DNOMINMAX)
ENDIF(UNIX)

IF(CANBERRA_FOUND)
        ADD_DEFINITIONS(-DHAVE_CANBERRA)
        INCLUDE_DIRECTORIES("${CANBERRA_INCLUDE_DIRS}")
ENDIF(CANBERRA_FOUND)


CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
IF(NOT HAVE_STDINT_H)
        CONFIGURE_FILE(compat/pstdint.h "${CMAKE_CURRENT_BINARY_DIR}/stdint.h" COPYONLY)
        INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}")
ELSE(NOT HAVE_STDINT_H)
        FILE(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/stdint.h")
ENDIF(NOT HAVE_STDINT_H)

ADD_DEFINITIONS(-DBOOST_MULTI_INDEX_DISABLE_SERIALIZATION)

LIST(APPEND EXTRA_LIBS ${TIFF_LIBRARY} ${PNG_LIBRARY} ${ZLIB_LIBRARY} ${JPEG_LIBRARY} ${DJVULIBRE_LIBRARY} ${CANBERRA_LIBRARIES})

IF(ENABLE_OPENJPEG AND OpenJPEG_FOUND)
   INCLUDE_DIRECTORIES(${OPENJPEG_INCLUDE_DIRS})
   LIST(APPEND EXTRA_LIBS ${OPENJPEG_LIBRARIES})
ENDIF()

IF(WIN32)
   LIST(APPEND EXTRA_LIBS "ws2_32.lib")
ENDIF(WIN32)


SET(MAYBE_QT_OPENGL_MODULE "")
IF(ENABLE_OPENGL)
        LIST(APPEND EXTRA_LIBS ${OPENGL_LIBRARIES})
        SET(MAYBE_QT_OPENGL_MODULE OpenGL)
ENDIF()

# Prepare config.h
IF(WIN32)
        SET(TRANSLATIONS_DIR_REL "translations")
        SET(STYLESHEETS_DIR_REL "stylesheets")
ELSEIF(APPLE)
        SET(TRANSLATIONS_DIR_REL "../Resources/translations")
        SET(STYLESHEETS_DIR_REL "../Resources/stylesheets")
ELSE()
        SET(TRANSLATIONS_DIR_REL "share/scantailor-universal/translations")
        SET(STYLESHEETS_DIR_REL "share/scantailor-universal/stylesheets")
        SET(PIXMAPS_DIR_ABS "${CMAKE_INSTALL_PREFIX}/share/pixmaps/scantailor-universal/stylesheets")
ENDIF()
SET(TRANSLATIONS_DIR_ABS "${CMAKE_INSTALL_PREFIX}/${TRANSLATIONS_DIR_REL}")
SET(STYLESHEETS_DIR_ABS "${CMAKE_INSTALL_PREFIX}/${STYLESHEETS_DIR_REL}")


CONFIGURE_FILE(config.h.in ${CMAKE_BINARY_DIR}/config.h @ONLY)

ADD_SUBDIRECTORY(src)


IF(NOT WIN32 AND NOT APPLE)
        INSTALL(FILES debian/scantailor-universal.svg DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pixmaps/")
        INSTALL(FILES debian/scantailor-universal.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications/")
ENDIF(NOT WIN32 AND NOT APPLE)

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Source code packaging
SET(CPACK_CMAKE_GENERATOR "")
SET(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "scantailor-universal-${VERSION}")
SET(
        CPACK_SOURCE_IGNORE_FILES
        "/\\\\.svn/"
        "/\\\\.git/"
        "~$"
        "\\\\.pcs$"
        "TODO.txt"
        "CMakeLists.txt.user"
        "/doxygen/"
        "${CMAKE_BINARY_DIR}"
)
INCLUDE(CPack)

IF(WIN32)
        # Copy some DLLs to the staging dir.

        SET(
                qt5_libs
                ${Qt5Widgets_LIBRARIES} ${Qt5Gui_LIBRARIES}
                ${Qt5Core_LIBRARIES} ${Qt5Xml_LIBRARIES}  ${Qt5Svg_LIBRARIES}
        )
        IF(ENABLE_OPENGL)
                LIST(APPEND qt5_libs ${Qt5OpenGL_LIBRARIES})
        ENDIF()
        FOREACH(target ${qt5_libs})
                GET_TARGET_PROPERTY(debug_loc "${target}" LOCATION_DEBUG)
                GET_TARGET_PROPERTY(release_loc "${target}" LOCATION_RELEASE)
                COPY_TO_BUILD_DIR("${debug_loc}" CONFIGURATIONS Debug)
                COPY_TO_BUILD_DIR("${release_loc}" CONFIGURATIONS Release MinSizeRel RelWithDebInfo)
        ENDFOREACH()

        SET(DLL_DIR "${QT_SRC_DIR}/qtbase/bin")
        COPY_TO_BUILD_DIR(
                "${DLL_DIR}/libpng.dll" "${DLL_DIR}/libjpeg.dll"
                "${DLL_DIR}/libtiff.dll" "${DLL_DIR}/zdll.dll"
        )

IF(ENABLE_OPENJPEG AND OpenJPEG_FOUND)
        COPY_TO_BUILD_DIR( "${DLL_DIR}/openjp2.dll" )
ENDIF(ENABLE_OPENJPEG AND OpenJPEG_FOUND)

        # Qt's plugins.
        SET(PLUGINS_DIR "${QT_SRC_DIR}/qtbase/plugins")
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/platforms/qwindows.dll" SUBDIR platforms
                CONFIGURATIONS Release MinSizeRel RelWithDebInfo
        )
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/platforms/qwindowsd.dll" SUBDIR platforms
                CONFIGURATIONS Debug
        )
        # required to display images in stylesheets
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/imageformats/qico.dll" SUBDIR imageformats
                CONFIGURATIONS Release MinSizeRel RelWithDebInfo
        )
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/imageformats/qicod.dll" SUBDIR imageformats
                CONFIGURATIONS Debug
        )
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/imageformats/qsvg.dll" SUBDIR imageformats
                CONFIGURATIONS Release MinSizeRel RelWithDebInfo
        )
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/imageformats/qsvgd.dll" SUBDIR imageformats
                CONFIGURATIONS Debug
        )
#        COPY_TO_BUILD_DIR(
#                "${PLUGINS_DIR}/iconengines/qsvgicon.dll" SUBDIR iconengines
#                CONFIGURATIONS Release MinSizeRel RelWithDebInfo
#        )
#        COPY_TO_BUILD_DIR(
#                "${PLUGINS_DIR}/iconengines/qsvgicond.dll" SUBDIR iconengines
#                CONFIGURATIONS Debug
#        )

    if (Qt5Widgets_VERSION VERSION_LESS 5.4.0)
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/accessible/qtaccessiblewidgets.dll" SUBDIR accessible
                CONFIGURATIONS Release MinSizeRel RelWithDebInfo
        )
        COPY_TO_BUILD_DIR(
                "${PLUGINS_DIR}/accessible/qtaccessiblewidgetsd.dll" SUBDIR accessible
                CONFIGURATIONS Debug
        )
    endif()

        # Generate the target that will actually do the copying.
        GENERATE_COPY_TO_BUILD_DIR_TARGET(copy_to_build_dir)

        # Installer for Windows.  Must go last.
        ADD_SUBDIRECTORY(src/packaging/windows)
ENDIF(WIN32)
