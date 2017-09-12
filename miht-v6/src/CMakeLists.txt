# NOTE: With the 'REQUIRED' option, 'find_package' issues an error if the
#       package can't be found.
find_package(OpenMP REQUIRED)

# Passes the required OpenMP/pthreads flags to the compiler.  NOTE: Using
# 'target_compile_options()' works for gcc, but doesn't work for icc, because
# it needs the flag '-openmp' also in the linking stage.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c11 ${OpenMP_C_FLAGS}")

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -qopt-report=5 -qopt-report-phase=ipo,loop,vec")
endif ()

option(BENCHMARK "BENCHMARK" OFF)
if (BENCHMARK)
    message(STATUS "BENCHMARK: ON")
    add_definitions(-DBENCHMARK)
else()
    message(STATUS "BENCHMARK: OFF")
endif()

# Serial (CPU)
add_executable(miht-v6
    main.c
    ip.c
    ip.h
    miht.c
    prettyprint.c
)

target_link_libraries(miht-v6 m)

# Parallel (CPU)
add_executable(miht-v6_par
    main.c
    ip.c
    ip.h
    miht.c
    prettyprint.c
)

target_compile_definitions(miht-v6_par PRIVATE -DLOOKUP_PARALLEL)

target_link_libraries(miht-v6_par m)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    message(STATUS "MIC: ON")
    # Serial (MIC)
    add_executable(miht-v6_mic
        main.c
        ip.c
        ip.h
        miht.c
        prettyprint.c
    )

    target_compile_options(miht-v6_mic PRIVATE -mmic)
    target_link_libraries(miht-v6_mic m -mmic)

    # Parallel (MIC)
    add_executable(miht-v6_mic_par
        main.c
        ip.c
        ip.h
        miht.c
        prettyprint.c
    )

    target_compile_options(miht-v6_mic_par PRIVATE -mmic)
    target_compile_definitions(miht-v6_mic_par PRIVATE -DLOOKUP_PARALLEL)
    target_link_libraries(miht-v6_mic_par m -mmic)
endif()

