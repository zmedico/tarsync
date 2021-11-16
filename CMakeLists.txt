cmake_minimum_required(VERSION 3.18)

project(tarsync VERSION 0.2.3)

find_library(CFILE_LIBRARY NAMES cfile REQUIRED)
find_path(CFILE_INCLUDE_DIR cfile.h REQUIRED)

set(TARSYNC_SRC
	excludes.c
	fs.c
	main.c
	names.c
	options.c
	tar.c
)

configure_file(version.h.cmake.in version.h)

add_executable(tarsync ${TARSYNC_SRC})
target_link_libraries(tarsync PRIVATE ${CFILE_LIBRARY})
target_include_directories(tarsync PRIVATE ${CFILE_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(tarsync PRIVATE _POSIX_C_SOURCE=200809L)

include(GNUInstallDirs)
install(TARGETS tarsync	RUNTIME DESTINATION bin)
install(FILES README.md TYPE DOC)
