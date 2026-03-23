cmake_minimum_required(VERSION 3.19)
project(SSS)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-w)

if (MSVC)
    set(OS "win")
    set(BOOST_INCLUDEDIR libOTe/cryptoTools/thirdparty/${OS}/include/boost-1_76)
    find_package(Boost)
else ()
    set(OS "unix")
    set(BOOST_INCLUDEDIR libOTe/cryptoTools/thirdparty/${OS}/include/)
    set(BOOST_COMPOENTS filesystem system thread atomic regex)
    add_compile_options(-maes -msse2 -msse3 -mssse3 -msse4.1 -mpclmul)
    find_package(Boost COMPONENTS REQUIRED ${BOOST_COMPOENTS})
endif ()


set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GMP REQUIRED)
find_package(pybind11 REQUIRED)
set(MY_LIBS libOTe cryptoTools relic gmp gmpxx)

include_directories(libOTe/out/install/linux/include)
include_directories(libOTe/cryptoTools/thirdparty/${OS}/include/)
include_directories(${BOOST_INCLUDEDIR})

link_directories(libOTe/cryptoTools/thirdparty/${OS}/lib)
link_directories(libOTe/out/install/linux/lib64)
link_directories(libOTe/out/install/linux/lib)

include_directories(osn)
include_directories(common)

add_library(osn SHARED 
        osn/benes.cpp
        osn/benes.h
        osn/OSNReceiver.cpp
        osn/OSNReceiver.h
        osn/OSNSender.cpp
        osn/OSNSender.h
        osn/op.cpp
        osn/op.h
        )

# 添加性能分析选项
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
endif()

# 创建Python扩展模块
pybind11_add_module(SSS python/libSSS_wrapper.cpp osn)

target_link_libraries(SSS PRIVATE  ${MY_LIBS} ${Boost_LIBRARIES} osn)

add_executable(test_osn
        test/test_osn.cpp)
target_link_libraries(test_osn osn ${MY_LIBS} ${Boost_LIBRARIES})