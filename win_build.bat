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
)
