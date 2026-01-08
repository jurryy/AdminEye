#!/bin/bash

# Create output directory
mkdir -p bin

# Common Include Flags (Looks in root and src)
CFLAGS="-I. -Isrc"

case "$1" in
    Linux)
        echo "Compiling Linux Client..."
        # Compiles Client.c with the Linux-specific Window implementation
        if [ -f "src/include/Linux/Window.c" ]; then
            gcc src/Client.c src/include/Linux/*.c -o bin/Client -lX11 $CFLAGS
        else
            # Fallback if files are in include/Linux instead of src/include/Linux
            gcc src/Client.c include/Linux/*.c -o bin/Client -lX11 $CFLAGS
        fi
        ;;
        
    Win32)
        echo "Compiling Windows Client..."
        # Compiles WinClient.c with the Windows-specific Window implementation
        # Uses static linking to avoid missing DLLs on Windows XP
        i686-w64-mingw32-gcc src/WinClient.c src/include/Windows/*.c -o bin/WinClient.exe -lws2_32 -lgdi32 -lshell32 -static $CFLAGS
        ;;
        
    Server)
        echo "Compiling Server..."
        # Compiles Server.c (Assuming it's in src/)
        if [ -f "src/Server.c" ]; then
            gcc src/Server.c src/include/*.c -o bin/Server $CFLAGS
        else
            echo "Error: src/Server.c not found."
        fi
        ;;
    *)
        
        echo "Usage: ./build.sh [Linux|Server|Win32]"
        exit 1
        ;;
esac