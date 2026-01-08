# AdminEye: PC Management System

AdminEye is a lightweight, cross-platform system designed for managing access to multiple computers in environments like school computer labs,
libraries, or personal PC rental setups. It provides automated session management, real-time monitoring, and remote control from a central server
application.

## Key Features

### Server-Side (Admin Dashboard)

-  **Text User Interface (TUI):** A responsive, terminal-based dashboard to monitor all PCs in real-time.
-  **Session Management:** Add time, assign users/notes, and remotely stop sessions.
-  **Network Discovery:** Automatically scan and find clients on the network.
-  **Encrypted Data Persistence:** All PC configurations and session data are saved to an encrypted `AdminEye.dat` file, ensuring data integrity and
   recovery after a restart.
-  **PC Status Control**

### Client-Side (User Terminal)

-  **Cross-Platform:** Native clients for both **Windows** (XP and newer) and **Linux**.
-  **Secure Lock Screen:** A full-screen overlay blocks access when a session is not active.
-  **Process Hardening (Windows):** The client runs as a background process, hides its console, and resists termination.
-  **Floating Timer:** A transparent, draggable window displays the remaining time without being intrusive.
-  **System Tray Icon:** Provides an easy way to toggle the visibility of the console and timer display.
-  **Offline Resilience:** The client continues its countdown locally even if the connection to the server is lost, preventing unfair lockouts.

---

## Project Structure

The project is organized into a `src` directory containing the core logic and platform-specific implementations.

```
AdminEye/
├── src/
│   ├── Client.c              # Main logic for the Linux client
│   ├── WinClient.c           # Main logic for the Windows client
│   ├── Server.c              # Main logic for the Server application
│   ├── include/
│   │   ├── PC.h/.c           # Shared PC data structures and logic
│   │   ├── Seam.h/.c         # Custom UDP networking protocol
│   │   ├── UI.h/.c           # Server TUI drawing functions
│   │   ├── Time.h/.c         # Time formatting utilities
│   │   ├── Util.h/.c         # Keyboard input utilities
│   │   ├── Linux/
│   │   │   ├── Window.h/.c   # Linux (X11) GUI/Console implementation
│   │   └── Windows/
│   │       ├── WinWindow.h/.c# Windows (Win32) GUI/Console implementation
│   └── ...
├── build.sh                  # Build script for all components
└── bin/                        # Output directory for compiled binaries
```

---

## Compilation

### 1. Compiling the Server (Linux)

This will create a `Server` executable in the `bin/` directory.

```bash
./build.sh Server
```

### 2. Compiling the Linux Client

This requires the `libx11-dev` package to be installed.

```bash
# For Debian/Ubuntu
sudo apt-get install libx11-dev

# Compile
./build.sh Linux
```

### 3. Cross-Compiling the Windows Client (from Linux)

This requires the `mingw-w64` cross-compiler. The output will be a single, statically-linked `.exe` file.

```bash
# For Debian/Ubuntu
sudo apt-get install mingw-w64

# Compile
./build.sh Win32
```

---

## Usage

1. **Run the Server:** Start the server on your main administration machine.

   ```bash
   ./bin/Server
   ```

2. **Add PCs on the Server:**
   -  On the server dashboard, press `P` to open the "Add PC" menu.
   -  Choose `S` to scan the network for running clients.
   -  Select a discovered client and press `Enter` to add it to a slot.
   -  You can now manage the PC from the main grid.

---

## License

This project is open-source. Please feel free to use, modify, and distribute it.
