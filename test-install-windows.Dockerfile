# escape=`

# Start with Windows Server Core as the base image
FROM mcr.microsoft.com/windows/servercore:ltsc2019 AS build

# Restore the default Windows shell for correct batch processing
SHELL ["cmd", "/S", "/C"]

# Install .NET Framework 4.8 (required for Chocolatey)
RUN curl -SL --output dotnet-framework-installer.exe https://download.visualstudio.microsoft.com/download/pr/2d6bb6b2-226a-4baa-bdec-798822606ff1/8494001c276a4b96804cde7829c04d7f/ndp48-x86-x64-allos-enu.exe `
    && start /w dotnet-framework-installer.exe /q /norestart `
    && del dotnet-framework-installer.exe

# Install Chocolatey package manager
RUN powershell -Command `
    Set-ExecutionPolicy Bypass -Scope Process -Force; `
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; `
    iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

# Install Git, 7zip, and Ninja build system
RUN choco install -y git 7zip ninja cmake neovim

# todo: extend the Windows build matrix with MSVC, clang-cl, MSYS2 MinGW GCC,
# and MSYS2 MinGW Clang. clang-cl currently requires adding the VS/LLVM toolset
# component here; MSYS2 requires installing the MinGW compiler packages.

# Install Visual Studio Build Tools with minimal C++ workload
RUN `
    # Download the Build Tools bootstrapper
    curl -SL --output vs_buildtools.exe https://aka.ms/vs/17/release/vs_buildtools.exe `
    `
    # Install Build Tools with minimal C++ workload - just enough for standard C++ development
    && (start /w vs_buildtools.exe --quiet --wait --norestart --nocache `
        --installPath "C:\BuildTools" `
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        --add Microsoft.VisualStudio.Component.VC.Redist.14.Latest `
        --add Microsoft.VisualStudio.Component.Windows10SDK `
        --add Microsoft.VisualStudio.Component.Windows10SDK.19041 `
        || IF "%ERRORLEVEL%"=="3010" EXIT 0) `
    `
    # Cleanup
    && del /q vs_buildtools.exe

# Setup Developer Command Prompt for VS - use environment variable
ENTRYPOINT ["C:\\BuildTools\\Common7\\Tools\\VsDevCmd.bat", "-arch=x64", "&&", "powershell.exe", "-NoLogo", "-ExecutionPolicy", "Bypass"]
