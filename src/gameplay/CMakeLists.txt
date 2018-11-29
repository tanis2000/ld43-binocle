include_directories(${CMAKE_CURRENT_SOURCE_DIR}
)

include_directories(
        ${CMAKE_SOURCE_DIR}/binocle-c/src
        ${CMAKE_SOURCE_DIR}/binocle-c/src/deps
        ${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl/include
        ${CMAKE_SOURCE_DIR}/binocle-c/src/deps/sdl_mixer
)

        
file(GLOB SOURCE_FILES
        *.c
        *.h
        )

GroupSources("gameplay")

# TODO: make it shared only on the desktop and in debug mode
add_library(gameplay SHARED ${SOURCE_FILES})

#target_link_libraries(gameplay ${BINOCLE_LINK_LIBRARIES})
target_link_libraries(gameplay binocle)

