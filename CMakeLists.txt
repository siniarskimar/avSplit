cmake_minimum_required(VERSION 3.9)

project(avSplit)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-FindLibAv/cmake")

find_package(AVformat REQUIRED)
find_package(AVutil REQUIRED)

add_executable(${PROJECT_NAME}
	"main.cpp"
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	AVformat::AVformat
	AVutil::AVutil
)