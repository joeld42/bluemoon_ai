cmake_minimum_required(VERSION 3.1...3.12)
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(BlueMoon VERSION 0.0.1
                  DESCRIPTION "BlueMoon AI"
                  LANGUAGES C CXX)

find_package (Intl REQUIRED)

# TODO: Make a CMAKE module for this
set ( RAYLIB_HOME /Users/joeld/Toolkits/raylib/ )

set (INTL_INCLUDE_DIRS /usr/local/opt/gettext/include/ )
set (INTL_LIBRARY_DIRS /usr/local/opt/gettext/lib/ )

message( STATUS "INTL FOUND: " ${Intl_FOUND} )
message( STATUS "Inc" ${INTL_INCLUDE_DIRS} )
message( STATUS "Lib" ${INTL_LIBRARY_DIRS} )

include_directories(
	${RAYLIB_HOME}/src
	/usr/local/include
	${INTL_INCLUDE_DIRS}
)

link_directories(
	${RAYLIB_HOME}/build/src/
	/usr/local/Cellar/glfw3/3.2.1/lib 	
	${INTL_LIBRARY_DIRS}
)

set ( LIBS ${LIBS}
		"-framework OpenGL"
		"-framework OpenAl"
		"-framework Cocoa"
		"-framework AudioToolbox"
		"-framework IOKit"
		"-framework CoreVideo"
		raylib
		intl
		#glfw3
      )
#[[
add_custom_command( 
	OUTPUT ${CMAKE_SOURCE_DIR}/shores_game_GEN.cpp
	COMMAND python /Users/joeld/stuff/pycgen_test/cmunger.py ${CMAKE_SOURCE_DIR}/shores_game.cpp ${CMAKE_SOURCE_DIR}/shores_game_GEN.cpp
	DEPENDS ${CMAKE_SOURCE_DIR}/shores_game.cpp
	VERBATIM
	)
]]

add_executable (bluemoon

				par_easings.h		
				bluemoon.h	
				net.h
			
				ai.c
				engine.c
				init.c
				net.c				
				bluemoon_main.cpp 
				)

target_compile_options( bluemoon PUBLIC -fno-rtti)
target_link_libraries( bluemoon ${LIBS} )
set_target_properties( bluemoon PROPERTIES 
	 					RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/ )
