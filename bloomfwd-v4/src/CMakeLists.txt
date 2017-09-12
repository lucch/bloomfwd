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

if(BLOOM_HASH_FUNCTION)
    if("${BLOOM_HASH_FUNCTION}" STREQUAL "BLOOM_KNUTH_HASH")
        message(STATUS "BLOOM_HASH_FUNCTION: BLOOM_KNUTH_HASH")
        add_definitions(-DBLOOM_KNUTH_HASH)
    elseif("${BLOOM_HASH_FUNCTION}" STREQUAL "BLOOM_H2_HASH")
        message(STATUS "BLOOM_HASH_FUNCTION: BLOOM_H2_HASH")
        add_definitions(-DBLOOM_H2_HASH)
    elseif("${BLOOM_HASH_FUNCTION}" STREQUAL "BLOOM_MURMUR_HASH")
        message(STATUS "BLOOM_HASH_FUNCTION: BLOOM_MURMUR_HASH")
    else()
        message(FATAL_ERROR "Unsupported hash function!")
    endif()
else()
    message(STATUS "BLOOM_HASH_FUNCTION: BLOOM_MURMUR_HASH")
endif()

if(HASHTBL_HASH_FUNCTION)
    if("${HASHTBL_HASH_FUNCTION}" STREQUAL "HASHTBL_KNUTH_HASH")
        message(STATUS "HASHTBL_HASH_FUNCTION: HASHTBL_KNUTH_HASH")
        add_definitions(-DHASHTBL_KNUTH_HASH)
    elseif("${HASHTBL_HASH_FUNCTION}" STREQUAL "HASHTBL_H2_HASH")
        message(STATUS "HASHTBL_HASH_FUNCTION: HASHTBL_H2_HASH")
        add_definitions(-DHASHTBL_H2_HASH)
    elseif("${HASHTBL_HASH_FUNCTION}" STREQUAL "HASHTBL_MURMUR_HASH")
        message(STATUS "HASHTBL_HASH_FUNCTION: HASHTBL_MURMUR_HASH")
    else()
        message(FATAL_ERROR "Unsupported hash function!")
    endif()
else()
    message(STATUS "HASHTBL_HASH_FUNCTION: HASHTBL_MURMUR_HASH")
endif()

if(FALSE_POSITIVE_RATIO)
    message(STATUS "FALSE_POSITIVE_RATIO: ${FALSE_POSITIVE_RATIO}")
    add_definitions(-DFALSE_POSITIVE_RATIO=${FALSE_POSITIVE_RATIO})
else()
    message(STATUS "FALSE_POSITIVE_RATIO: 0.01")
endif()

############### CPU
###### Serial
add_executable(bloomfwd_opt main_opt.c
    prettyprint.c
    bloomfwd_opt.c
)
target_compile_definitions(bloomfwd_opt PRIVATE)
target_link_libraries(bloomfwd_opt m)

###### Parallel
add_executable(bloomfwd_opt_par main_opt.c
    prettyprint.c
    bloomfwd_opt.c
)
target_compile_definitions(bloomfwd_opt_par PRIVATE -DLOOKUP_PARALLEL)
target_link_libraries(bloomfwd_opt_par m)

############### MIC
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    message(STATUS "MIC: ON")
    ###### Serial
    add_executable(bloomfwd_opt_mic main_opt.c
        prettyprint.c
        bloomfwd_opt.c
    )
    target_compile_options(bloomfwd_opt_mic PRIVATE -mmic)
    target_compile_definitions(bloomfwd_opt_mic PRIVATE)
    target_link_libraries(bloomfwd_opt_mic m -mmic)

    # Intrin
    add_executable(bloomfwd_opt_mic_intrin main_opt.c
        prettyprint.c
        bloomfwd_opt.c
    )
    target_compile_options(bloomfwd_opt_mic_intrin PRIVATE -mmic)
    target_compile_definitions(bloomfwd_opt_mic_intrin PRIVATE -DLOOKUP_VEC_INTRIN)
    target_link_libraries(bloomfwd_opt_mic_intrin m -mmic)
    
    ###### Parallel
    add_executable(bloomfwd_opt_mic_par main_opt.c
        prettyprint.c
        bloomfwd_opt.c
    )
    target_compile_options(bloomfwd_opt_mic_par PRIVATE -mmic)
    target_compile_definitions(bloomfwd_opt_mic_par PRIVATE -DLOOKUP_PARALLEL)
    target_link_libraries(bloomfwd_opt_mic_par m -mmic)
    
    # Intrin
    add_executable(bloomfwd_opt_mic_par_intrin main_opt.c
        prettyprint.c
        bloomfwd_opt.c
    )
    target_compile_options(bloomfwd_opt_mic_par_intrin PRIVATE -mmic)
    target_compile_definitions(bloomfwd_opt_mic_par_intrin PRIVATE -DLOOKUP_PARALLEL -DLOOKUP_VEC_INTRIN)
    target_link_libraries(bloomfwd_opt_mic_par_intrin m -mmic)
    
endif()

