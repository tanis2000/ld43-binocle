project(ld43-binocle)

cmake_minimum_required(VERSION 2.8.12)

# Policies
if("${CMAKE_VERSION}" VERSION_GREATER 3.1)
	cmake_policy(SET CMP0054 NEW)
	cmake_policy(SET CMP0025 NEW)
endif()

# cmake modules directory
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/binocle-c/build/cmake")

# Enables CMake database for YCM
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# Enables CMake debugging with VSCode
include(CMakeToolsHelpers OPTIONAL)

# Skip the platform compiler checks for cross compiling
set (CMAKE_CXX_COMPILER_WORKS TRUE)
set (CMAKE_C_COMPILER_WORKS TRUE)

# Enables CMake debugging with VSCode
include(CMakeToolsHelpers OPTIONAL)

# Add our own DEBUG define for Debug build types
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

include(BinocleUtils)

SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "1")
SET(VERSION_PATCH "0")
SET(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(BINOCLE_LINK_LIBRARIES binocle freetype kazmath parson)

if (NOT EMSCRIPTEN)
	set(BINOCLE_LINK_LIBRARIES ${BINOCLE_LINK_LIBRARIES} sdl ogg vorbis zlib)
endif()

if (MSVC)
	message("Including Windows defines")
	include(BinocleWindows)
elseif(APPLE)
	if(IOS)
		message("Including iOS defines")
		include(BinocleIOS)
	else()
		message("Including Mac defines")
		include(BinocleMac)
	endif()
elseif(ANDROID)
	message("Including Android defines")
	include(BinocleAndroid)
elseif(EMSCRIPTEN)
	message("Including Emscripten defines")
	include(BinocleWeb)
endif()

message("Project source dir: ${PROJECT_SOURCE_DIR}")

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE HEADERS ${PROJECT_SOURCE_DIR}/src/*.h)

message("Linking with the following libraries: ${BINOCLE_LINK_LIBRARIES}")

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/binocle-c/src/deps
    ${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include
    ${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl_mixer
    ${CMAKE_SOURCE_DIR}/binocle-c/src/deps/stb_image
    ${CMAKE_SOURCE_DIR}/binocle-c/src/binocle/core
    ${CMAKE_SOURCE_DIR}/src/gameplay
)

if (APPLE)
    if (IOS)
        include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include/configs/default)
    else ()
        include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include/configs/mac)
        include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/glew/include)
    endif ()
endif ()

if (ANDROID)
    include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include/configs/default)
endif ()

if (WIN32)
    include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include/configs/windows)
    include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/glew/include)
endif ()

if (EMSCRIPTEN)
    include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include/configs/default)
endif ()

file(GLOB SOURCE_FILES
${CMAKE_SOURCE_DIR}/src/*.c
${CMAKE_SOURCE_DIR}/src/*.h
        )

message("List of source files: ${SOURCE_FILES}")
message("List of include directories follows")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "dir='${dir}'")
endforeach()

if (NOT DEFINED BINOCLE_DATA_DIR)
    if (APPLE)
        if (IOS)
            set(BINOCLE_DATA_DIR "")
        else ()
            set(BINOCLE_DATA_DIR "../Resources/")
        endif ()
    elseif (WIN32)
        set(BINOCLE_DATA_DIR "../assets/")
    elseif (EMSCRIPTEN)
        # TODO: make this relative but also make sure that it
        # becomes relative in BinocleWeb.cmake as they have to be the same
        set(BINOCLE_DATA_DIR "/Users/tanis/Documents/ld43-binocle/assets/")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --preload-file /Users/tanis/Documents/ld43-binocle/assets")
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --preload-file /Users/tanis/Documents/ld43-binocle/assets")
    endif ()
endif ()
message("Data dir is ${BINOCLE_DATA_DIR}")


CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/src/sys_config.h.cmake ${CMAKE_SOURCE_DIR}/src/sys_config.h)
SET_SOURCE_FILES_PROPERTIES(${CMAKE_SOURCE_DIR}/src/sys_config.h PROPERTIES GENERATED TRUE)

if (APPLE)
    #if(IOS)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    file(GLOB RESOURCES_LIST ${CMAKE_SOURCE_DIR}/assets/*)
    set(RESOURCES
            ${RESOURCES_LIST}
            )
    message("Resources list: ${RESOURCES}")
    #endif()
  elseif (WIN32)
    file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR}/src)
endif ()


if (ANDROID)
    include_directories(${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include)
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/src/main/android/SDL_android_main.c)
else ()
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${EXE_TYPE} ${SOURCE_FILES} ${BINOCLE_ICON} ${RESOURCES})
endif ()

target_link_libraries(${PROJECT_NAME} ${BINOCLE_LINK_LIBRARIES})

# strtok_r is not supported by c99, so we obey gnu99 standard when compiling for emscripten
if (NOT EMSCRIPTEN)
    set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif()

if (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_PRODUCT_NAME "${PROJECT_NAME}")
endif ()

# Build the shared library next to sdl-endless-runner
set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/src
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/src
        XCODE_ATTRIBUTE_PRODUCT_NAME "${PROJECT_NAME}"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        )

if (APPLE)
    set(CMAKE_MACOSX_RPATH 1)

    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_RPATH 1
            BUILD_WITH_INSTALL_RPATH 1
            INSTALL_RPATH "@loader_path/../Frameworks"
            MACOSX_BUNDLE_GUI_IDENTIFIER "it.altralogica.${PROJECT_NAME}"
            )
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    #set_target_properties(${PROJECT_NAME} PROPERTIES RESOURCE "${RESOURCES}")
    # file(GLOB_RECURSE RESOURCES
    # 			${CMAKE_SOURCE_DIR}/data/*
    # )
    # set_source_files_properties(
    # 			${RESOURCES}
    # 			PROPERTIES
    # 			MACOSX_PACKAGE_LOCATION Resources
    # )
    set_source_files_properties(
            ${RESOURCES}
            PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources)
    if (IOS)
        set_target_properties(
                ${PROJECT_NAME}
                PROPERTIES
                MACOSX_BUNDLE YES
                MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/binocle-c/build/ios/Info.plist"
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"
                XCODE_ATTRIBUTE_INFOPLIST_PREPROCESS YES
                XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 8.0
                RESOURCE "${RESOURCES}"
        )

        # Copy resources
        #add_custom_command(
        #    TARGET ${NAME}
        #    POST_BUILD
        #    COMMAND /Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp -exclude .DS_Store -exclude CVS -exclude .svn -resolve-src-symlinks ${CMAKE_SOURCE_DIR}/data ${APP_NAME}
        #)
    endif ()
endif ()


if (MSVC)
    set(EXE_TYPE WIN32)
    #    set (SOURCE_FILES ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/build/cmake/Binocle.rc)
elseif (APPLE)
    #ICNS
    set(MACOSX_BUNDLE_ICON_FILE ${PROJECT_NAME}.icns)
    if (NOT IOS)
        set(BINOCLE_ICON ${CMAKE_SOURCE_DIR}/build/cmake/Binocle.icns)
    endif ()
    set_source_files_properties(${BINOCLE_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set(EXE_TYPE MACOSX_BUNDLE)
endif ()

if (ANDROID)
    message("Post build will copy to ${CMAKE_CURRENT_BINARY_DIR}/../../../android-project/libs/${ANDROID_ABI}/libmain.so")
    add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/../../../android-project/libs/${ANDROID_ABI}/libmain.so")
    add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/../../../android-project/libs/${ANDROID_ABI}/libmain.so")
    #	file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/../android-project/libs/${ANDROID_ABI}/lib${PROJECT_NAME}.so ${CMAKE_CURRENT_BINARY_DIR}/../android-project/libs/${ANDROID_ABI}/libmain.so)
endif ()

# Enable Undefined Behavior Sanitizer for macOS debug builds
if (APPLE)
    if (DEBUG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=integer -fsanitize=undefined -fsanitize=nullability -fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=integer -fsanitize=undefined -fsanitize=nullability -fsanitize=address -fno-omit-frame-pointer")
        add_definitions("-fsanitize=integer -fsanitize=undefined -fsanitize=nullability -fsanitize=address -fno-omit-frame-pointer")
    endif()
endif ()

add_subdirectory(binocle-c/src)
add_subdirectory(src/gameplay)
