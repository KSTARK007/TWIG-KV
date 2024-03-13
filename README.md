# LDC

### Regen capn proto

```
Debug
Change in machnet

set(CMAKE_C_FLAGS "-Wall -msse4.2")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -DNDEBUG -g")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG -fno-omit-frame-pointer")

set(CMAKE_CXX_FLAGS "-Wall -fno-rtti -fno-exceptions -msse4.2")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wno-unused-value")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -DNDEBUG -g -Wno-unused-value")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer -DDEBUG")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer")
```

uftrace

```
put in toplevel cmake
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finstrument-functions")
```

```
capnp compile packet.capnp -o c++
```

```
git submodule update --remote --merge
```

```
#server
<!-- ./bin/ldc -client 0 -config /mnt/sda4/LDC/third_party/blkcache/config.json -machine_index 1 -dataset_config /mnt/sda4/LDC/src/ops_config.json index 1 -->
make ldc -j && ./bin/ldc -config ../config.json -dataset_config ../src/ops_config.json -machine_index 1 -threads 2
make ldc -j && ./bin/ldc -config ../config.json -dataset_config ../src/ops_config.json -machine_index 2 -threads 2
make ldc -j && ./bin/ldc -config ../config.json -dataset_config ../src/ops_config.json -machine_index 3 -threads 2

#client

<!-- ./bin/ldc -client 1 -config /mnt/sda4/LDC/third_party/blkcache/config.json -machine_index 0 -dataset_config /mnt/sda4/LDC/src/ops_config.json  -->
make ldc -j && ./bin/ldc -config ../config.json -dataset_config ../src/ops_config.json -machine_index 0 -threads 2

```