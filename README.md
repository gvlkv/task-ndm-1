# task-ndm-1

## Build

``` sh
meson setup build
cd build
meson compile
```

## Test

``` sh
docker build -t task-ndm-1 .
docker run -it --privileged task-ndm-1 ./run_test
```
