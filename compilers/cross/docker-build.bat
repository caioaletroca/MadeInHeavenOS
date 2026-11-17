@REM Copy the sysroot's headers into the build context (make toolchain-headers makes them).
@REM Headers only: GCC needs nothing else, and a changed program would rebuild GCC.
@REM /MIR also deletes what is no longer there.
robocopy ../../build/x86_64-pc/sysroot/usr/include ./sysroot/usr/include /MIR

@REM Build docker image
docker build . -t mihos-compiler
