# Source code of Bloomfwd and MIHT

## Algorithms

  - `baseline`: Baseline BFs algorithm (uses C `rand()` function for hashing).
  - `bloomfwd-v4`: Optimized BFs algorithm for IPv4.
  - `bloomfwd-v4-coop`: Cooperative version of `bloomfwd` for IPv4.
  - `bloomfwd-v6`: Optimized BFs algorithm for IPv6.
  - `miht-v4`: (16,16)-MIHT.
  - `miht-v6`: (32,32)-MIHT.

## Building

First, make sure `$(CC)` is set to the Intel(R) C Compiler:

```
export CC=icc
```

Then, run:

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

This will generate a **Release** build by default, with all the optimization
flags on. In order to generate a **Debug** build, run `cmake
-DCMAKE_BUILD_TYPE=Debug ..` instead. The compilation process can be followed in
detail by running `make VERBOSE=1`. The binaries will be put in the `bin/`
directory under the project root.

The BFs algorithms for IPv4 (`baseline`, `bloomfwd-v4` and `bloomfwd-v4-coop`)
also accept setting the false positive ratio and the hash function to be used
for searching the Bloom filters and the hash tables (check
`src/CMakeLists.txt` file).  Example:

```
# Sets 60% of FPR, Murmur3 for BFs and H2 for HTs.
cmake -DCMAKE_BUILD_TYPE=Release -DFALSE_POSITIVE_RATIO=0.6 -DBLOOM_HASH_FUNCTION=BLOOM_MURMUR_HASH -DHASHTBL_HASH_FUNCTION=HASHTBL_H2_HASH ..
```

## Running

It is required to the OpenMP library (`libiomp5.so`) to be in the search path.
It can be done with the following command:

```
export LD_LIBRARY_PATH=<path to the library>
```

Then, you can simply run the binary that was generated in the `bin/` folder.

The BFs algorithms require at least three parameters:

  - `-d`: path to the prefixes distribution file.
  - `-p`: path to the file containing the prefixes.
  - `-r`: path to the file containing the input IP addresses.

The MIHT algorithms require only `-p` and `-r`.

The structure of those files is described in the next section.

## Input Files

In the experiments, we have collected real prefix datasets from
[Routeviews](http://www.routeviews.org/), [RIPE](https://www.ripe.net/) and
[Potaroo](http://bgp.potaroo.net/). Those files, in MRT format, were parsed
using tools such as
[zebra-dump-parser](https://github.com/rfc1036/zebra-dump-parser),
[bgpdump](https://bitbucket.org/ripencc/bgpdump), standard Linux tools
(`cut`, `paste`, `uniq`, `sort`, etc) and helper scripts (under `ip-helpers`
directory) to input files in the formats below. 

We have placed sample data files for both IPv4 and IPv6 in the directory
`data-samples`.

### Prefixes Distribution File

This is a text file with two columns representing the total number of prefixes
in each possible length for a particular prefixes file (specified after `-p`).
For IPv4, the first column values are 0, 1, 2, ..., 32; while for IPv6, the
first column values are 0, 1, 2, ..., 64. The second column values are the
amount of prefixes in each length.

### Prefixes File

This is a text file with two columns representing the prefixes and their
associated next hop addresses. For IPv4, the first column is the prefix in CIDR
notation, and the second column is the next hop, also in CIDR notation. For
IPv6, both the prefixes and the next hops must be represented in the canonical
form.

### Input IP Addresses

This is a text file with just one column containing a list of input addresses
for lookup. The first row in this file must be an integer with the amount of
addresses to perform the lookup for. Again, for IPv4, the addresses must be
represented in CIDR notation and, for IPv6, in canonical form. 
