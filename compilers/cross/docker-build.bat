@REM Copy the sysroot's headers into the build context (make toolchain-headers makes them).
@REM Headers only: GCC needs nothing else, and a changed program would rebuild GCC.
@REM /MIR also deletes what is no longer there.
robocopy ../../build/x86_64-pc/sysroot/usr/include ./sysroot/usr/include /MIR

@REM Build docker image from the repository's root: the image builds mlibc for libstdc++
@REM (Dockerfile.dockerignore keeps the rest out)
docker build -f Dockerfile -t mihos-compiler ../..
