#!/bin/bash

# Create output directory
mkdir -p bin

# Common Include Flags (Looks in root and src)
CFLAGS="-I. -Isrc"

build_linux() {
    echo "Compiling Linux Client..."
    # Compiles Client.c with the Linux-specific Window implementation
    if [ -f "src/include/Linux/Window.c" ]; then
        gcc src/Client.c src/include/Linux/*.c src/include/UI.c src/include/Time.c src/include/Util.c src/include/PC.c -o bin/Client -lX11 $CFLAGS
    else
        # Fallback if files are in include/Linux instead of src/include/Linux
        gcc src/Client.c include/Linux/*.c src/include/UI.c src/include/Time.c src/include/Util.c src/include/PC.c -o bin/Client -lX11 $CFLAGS
    fi
}

build_win32() {
    echo "Compiling Windows Client..."
    # Compiles WinClient.c with the Windows-specific Window implementation
    # Uses static linking to avoid missing DLLs on Windows XP
    if command -v i686-w64-mingw32-gcc &> /dev/null; then
        i686-w64-mingw32-gcc src/WinClient.c src/include/Windows/*.c src/include/Time.c -o bin/WinClient.exe -lws2_32 -lgdi32 -ladvapi32 -luser32 -lshell32 -static $CFLAGS
    else
        echo "Error: i686-w64-mingw32-gcc not found. Skipping Windows build."
    fi
}

build_server() {
    echo "Compiling Server..."
    # Compiles Server.c (Assuming it's in src/)
    if [ -f "src/Server.c" ]; then
        gcc src/Server.c src/include/*.c -o bin/Server $CFLAGS
    else
        echo "Error: src/Server.c not found."
    fi
}

case "$1" in
    Linux)
        build_linux
        ;;
        
    Win32)
        build_win32
        ;;
        
    Server)
        build_server
        ;;

    release)
        echo "Creating Pre-Release binaries..."
        mkdir -p release
        rm -f release/*

        build_server
        [ -f bin/Server ] && cp bin/Server release/

        build_linux
        [ -f bin/Client ] && cp bin/Client release/

        build_win32
        [ -f bin/WinClient.exe ] && cp bin/WinClient.exe release/

        echo "Done. Binaries are in 'release/' folder."
        ls -lh release/
        ;;
        
    *)
        echo "Usage: ./build.sh [Linux|Server|Win32|release]"
        exit 1
        ;;
esac