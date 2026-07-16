#==============================================================================
#
#  OpenFresco - Open Framework for Experimental Setup and Control
#
#  CMake build for Linux/macOS.
#
#  Builds the loadable Python package OpenFrescoPy_pkg.so, which is
#  loaded from an OpenSeesPy model with:
#
#      ops.loadPackage('/path/to/OpenFrescoPy_pkg', 'OpenFrescoPy')
#
#  The package is self-contained: it compiles its own copy of the
#  OpenSees core (SRC/openseesCore + SRC/openseesExtra, kept in sync
#  with upstream OpenSees) because the host opensees.so module is
#  dlopen'ed with RTLD_LOCAL and its symbols are not visible here.
#  This mirrors the WIN64/proj/OpenFrescoPy_dll Visual Studio project.
#
#  Usage:
#      cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#      cmake --build build --parallel
#  Output:
#      build/lib/packages/OpenFrescoPy_pkg.so
#
#==============================================================================

cmake_minimum_required(VERSION 3.16)
project(OpenFresco LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Python3 REQUIRED COMPONENTS Development.Module)
find_package(OpenSSL REQUIRED)
find_package(LAPACK REQUIRED)

set(OPF_SRC ${PROJECT_SOURCE_DIR}/SRC)

#------------------------------------------------------------------------------
# Bundled OpenSees sources: only the classes the host opensees.so does NOT
# export (everything else resolves from the host at dlopen time, see below)
#------------------------------------------------------------------------------
set(OPF_OPENSEES_MISSING_SRCS
    ${OPF_SRC}/openseesExtra/Actor.cpp
    ${OPF_SRC}/openseesExtra/Shadow.cpp
    ${OPF_SRC}/openseesExtra/TCP_SocketSSL.cpp)

#------------------------------------------------------------------------------
# OpenFresco framework sources
#------------------------------------------------------------------------------
# ESFErrorSimTimeDelay.cpp (references an undeclared 'undershoot' member) and
# ESFForceBroyden.cpp (duplicate copy of the ESFErrorSimRandomGauss
# implementation) are broken upstream sources: the Windows build excludes
# them too and neither is registered in ExpSignalFilterCommands.cpp.
file(GLOB OPF_FRAMEWORK_SRCS
    ${OPF_SRC}/experimentalControlPoint/*.cpp
    ${OPF_SRC}/experimentalElement/*.cpp
    ${OPF_SRC}/experimentalRecorder/*.cpp
    ${OPF_SRC}/experimentalRecorder/response/*.cpp
    ${OPF_SRC}/experimentalSetup/*.cpp
    ${OPF_SRC}/experimentalSignalFilter/*.cpp
    ${OPF_SRC}/experimentalSite/*.cpp
    ${OPF_SRC}/experimentalTangentStiff/*.cpp)
list(REMOVE_ITEM OPF_FRAMEWORK_SRCS
    ${OPF_SRC}/experimentalSignalFilter/ESFErrorSimTimeDelay.cpp
    ${OPF_SRC}/experimentalSignalFilter/ESFForceBroyden.cpp)

# Experimental controls: only the portable (socket/simulation based) ones.
# The hardware controllers (xPCtarget, SCRAMNet(GT), dSpace, MTSCsi,
# NIEseries) need proprietary Windows-only libraries and are excluded.
set(OPF_CONTROL_SRCS
    ${OPF_SRC}/experimentalControl/ExperimentalControl.cpp
    ${OPF_SRC}/experimentalControl/ECGenericTCP.cpp
    ${OPF_SRC}/experimentalControl/ECLabVIEW.cpp
    ${OPF_SRC}/experimentalControl/ECSimulation.cpp
    ${OPF_SRC}/experimentalControl/ECSimDomain.cpp
    ${OPF_SRC}/experimentalControl/ECSimFEAdapter.cpp
    ${OPF_SRC}/experimentalControl/ECSimSimulink.cpp
    ${OPF_SRC}/experimentalControl/ECSimUniaxialMaterials.cpp
    ${OPF_SRC}/experimentalControl/ECSpeedgoat.cpp)

#------------------------------------------------------------------------------
# Interpreter commands + Python package entry point
#------------------------------------------------------------------------------
set(OPF_INTERPRETER_SRCS
    ${OPF_SRC}/interpreter/ExpControlCommands.cpp
    ${OPF_SRC}/interpreter/ExpElementCommands.cpp
    ${OPF_SRC}/interpreter/ExpRecorderCommands.cpp
    ${OPF_SRC}/interpreter/ExpServerCommands.cpp
    ${OPF_SRC}/interpreter/ExpSetupCommands.cpp
    ${OPF_SRC}/interpreter/ExpSignalFilterCommands.cpp
    ${OPF_SRC}/interpreter/ExpSiteCommands.cpp
    ${OPF_SRC}/interpreter/ExpTangentStiffCommands.cpp
    ${OPF_SRC}/interpreter/python/PythonPackage.cpp)

#------------------------------------------------------------------------------
# Target
#------------------------------------------------------------------------------
# The bundled OpenSees core (openseesCore/openseesExtra) is NOT compiled in:
# on Linux the host opensees.so must be imported with RTLD_GLOBAL
# (sys.setdlopenflags) and every OpenSees symbol resolves from the host at
# dlopen time, exactly like the classic OpenFresco.so Tcl package. Bundling
# a second copy of the core classes alongside the host's leads to symbol
# interposition mixing the two copies (observed as heap corruption).
add_library(OpenFrescoPy_pkg SHARED
    ${OPF_OPENSEES_MISSING_SRCS}
    ${OPF_FRAMEWORK_SRCS}
    ${OPF_CONTROL_SRCS}
    ${OPF_INTERPRETER_SRCS})

set_target_properties(OpenFrescoPy_pkg PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/packages)

target_compile_definitions(OpenFrescoPy_pkg PRIVATE _UNIX _LINUX)

target_include_directories(OpenFrescoPy_pkg PRIVATE
    ${OPF_SRC}
    ${OPF_SRC}/openseesCore
    ${OPF_SRC}/openseesExtra
    ${OPF_SRC}/experimentalControl
    ${OPF_SRC}/experimentalControlPoint
    ${OPF_SRC}/experimentalElement
    ${OPF_SRC}/experimentalRecorder
    ${OPF_SRC}/experimentalRecorder/response
    ${OPF_SRC}/experimentalSetup
    ${OPF_SRC}/experimentalSignalFilter
    ${OPF_SRC}/experimentalSite
    ${OPF_SRC}/experimentalTangentStiff)

target_link_libraries(OpenFrescoPy_pkg PRIVATE
    Python3::Module
    OpenSSL::SSL
    OpenSSL::Crypto
    LAPACK::LAPACK
    ${CMAKE_DL_LIBS}
    pthread
    m)
