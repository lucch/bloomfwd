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
if(BENCHMARK)
    message(STATUS "BENCHMARK: ON")
    add_definitions(-DBENCHMARK)
else()
    message(STATUS "BENCHMARK: OFF")
endif()

if(FALSE_POSITIVE_RATIO)
    message(STATUS "FALSE_POSITIVE_RATIO: ${FALSE_POSITIVE_RATIO}")
    add_definitions(-DFALSE_POSITIVE_RATIO=${FALSE_POSITIVE_RATIO})
else()
    message(STATUS "FALSE_POSITIVE_RATIO: 0.01")
endif()

############### CPU
###### Serial
add_executable(bloomfwd_opt_rand main_opt.c
    prettyprint.c
    bloomfwd_opt.c
)
target_compile_definitions(bloomfwd_opt_rand PRIVATE)
target_link_libraries(bloomfwd_opt_rand m)

###### Parallel
add_executable(bloomfwd_opt_par_rand main_opt.c
    prettyprint.c
    bloomfwd_opt.c
)
target_compile_definitions(bloomfwd_opt_par_rand PRIVATE -DLOOKUP_PARALLEL)
target_link_libraries(bloomfwd_opt_par_rand m)

############### MIC
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    message(STATUS "MIC: ON")
    ###### Serial
    add_executable(bloomfwd_opt_mic_rand main_opt.c
        prettyprint.c
        bloomfwd_opt.c
    )
    target_compile_options(bloomfwd_opt_mic_rand PRIVATE -mmic)
    target_compile_definitions(bloomfwd_opt_mic_rand PRIVATE)
    target_link_libraries(bloomfwd_opt_mic_rand m -mmic)

    ###### Parallel
    add_executable(bloomfwd_opt_mic_par_rand main_opt.c
        prettyprint.c
        bloomfwd_opt.c
    )
    target_compile_options(bloomfwd_opt_mic_par_rand PRIVATE -mmic)
    target_compile_definitions(bloomfwd_opt_mic_par_rand PRIVATE -DLOOKUP_PARALLEL)
    target_link_libraries(bloomfwd_opt_mic_par_rand m -mmic)
endif()

