# LDC

### Regen capn proto

```
capnp compile packet.capnp -o c++
```

```
#server
make ldc -j && ./bin/ldc -client 0 -config config.json -machine_index 1 index 1
#client
make ldc -j && ./bin/ldc -client 1 -config config.json -machine_index 0 
```