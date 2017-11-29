@echo off

cd %APPVEYOR_BUILD_FOLDER%

echo Compiler: %COMPILER%
echo Architecture: %MSYS2_ARCH%
echo Platform: %PLATFORM%
echo MSYS2 directory: %MSYS2_DIR%
echo MSYS2 system: %MSYSTEM%
echo Configuration: %CONFIGURATION%
echo Bits: %BIT%

IF %COMPILER%==msys2 (
  @echo on
  SET "PATH=C:\%MSYS2_DIR%\%MSYSTEM%\bin;C:\%MSYS2_DIR%\usr\bin;%PATH%"

  bash -lc "cd $APPVEYOR_BUILD_FOLDER && cp -rf windows/* ./"
  bash -lc "cd $APPVEYOR_BUILD_FOLDER && ./bootstrap.sh"
  bash -lc "cd $APPVEYOR_BUILD_FOLDER && ./configure --datadir=/c"
  bash -lc "cd $APPVEYOR_BUILD_FOLDER && make"
  bash -lc "cd $APPVEYOR_BUILD_FOLDER && make install"
  bash -lc "cd $APPVEYOR_BUILD_FOLDER && cp src/.libs/libpostal-*.dll libpostal.dll"
  "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\lib.exe" /def:libpostal.def /out:libpostal.lib /machine:x64
)
