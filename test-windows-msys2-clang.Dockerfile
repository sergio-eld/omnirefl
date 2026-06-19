# escape=`

FROM ghcr.io/sergio-eld/test-windows-msys2-mingw

SHELL ["cmd", "/S", "/C"]

# Keep MSYS2 clang64 out of the MinGW GCC image. The clang row is non-required
# until omnirefl's bundled libc++ headers work with the clang64 MinGW CRT setup.
RUN C:\tools\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja mingw-w64-clang-x86_64-clang && pacman -Scc --noconfirm"
