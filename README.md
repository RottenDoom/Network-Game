# Multiplayer Coin Collector Game

A real-time multiplayer game demonstrating client-server architecture with authoritative server, network latency simulation, and client-side interpolation.

## Features

- **Authoritative Server**: All game logic runs on the server
- **Network Latency Simulation**: 200ms artificial delay on all messages
- **Client-Side Interpolation**: Smooth player movement despite latency
- **Real-time Coin Collection**: Competitive coin gathering with score tracking
- **Cross-Platform**: Works on Windows, Linux, and macOS

## Technology Stack

- **Networking**: Boost.ASIO (TCP sockets)
- **Graphics**: SDL2 and SDL2_ttf
- **Build System**: CMake with FetchContent
- **Language**: C++17

## Architecture

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│  Client 1   │◄───────►│    Server    │◄───────►│  Client 2   │
│   (SDL2)    │  200ms  │  (Authority) │  200ms  │   (SDL2)    │
└─────────────┘ latency └──────────────┘ latency └─────────────┘
```

### Server Responsibilities

- Player position validation
- Collision detection (player-coin)
- Score management
- Game state broadcasting (50ms tick rate)
- Coin spawning (every 3 seconds)

### Client Responsibilities

- Rendering
- Input capture
- Entity interpolation for smooth visuals
- Local prediction (minimal)

## Build Instructions

### Prerequisites

- CMake 3.20+
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- Internet connection (for fetching dependencies)
- SDL2 and SDL2_ttf libraries (or use bundled third-party libraries)

### Quick Build (Windows)

Use the provided batch script:

```bash
build-all.bat
```

This will:

1. Create a `build/build-gcc` directory
2. Configure CMake
3. Build both server and client executables

### Manual Build

#### Windows (MinGW/GCC)

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --config Release
```

#### Windows (Visual Studio)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

#### Linux/macOS

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### Build Output

After building, executables will be located in:

- **Windows**: `build/build-gcc/client.exe` and `build/build-gcc/server.exe`
- **Linux/macOS**: `build/client` and `build/server`

### Building Documentation

To generate Doxygen documentation:

```bash
# Windows
build_docs.bat

# Linux/macOS (requires Doxygen installed)
doxygen Doxyfile
```

Documentation will be generated in the `docs/html` directory. Open `docs/html/index.html` in your browser to view it.

## Running the Game

### Start Server

```bash
# Default port 12345
./server

# Custom port
./server 8080
```

### Start Clients (in separate terminals)

```bash
# Client 1
./client

# Client 2 (on same machine)
./client

# Remote client
./client 192.168.1.100 12345
```

### Controls

- **WASD** or **Arrow Keys**: Move player
- **ESC**: Quit

## Network Protocol

### Message Types

1. `CLIENT_CONNECT` - Initial connection handshake
2. `CLIENT_INPUT` - Movement input (dx, dy, timestamp)
3. `SERVER_GAME_STATE` - Full game state update
4. `SERVER_START_GAME` - Game session begins
5. `CLIENT_DISCONNECT` - Player leaves

### Message Format

```
[Header: 5 bytes]
  - MessageType: 1 byte
  - Length: 4 bytes

[Body: Variable]
  - Depends on message type
```

### Game State Message Structure

```
[timestamp: 4 bytes]
[player_count: 1 byte]
[coin_count: 1 byte]
[PlayerState * player_count]
  - id: 4 bytes
  - position: 8 bytes (2 floats)
  - score: 4 bytes
[CoinState * coin_count]
  - id: 4 bytes
  - position: 8 bytes (2 floats)
```

## Interpolation System

The client implements linear interpolation to smooth out movement despite 200ms latency:

```cpp
// Interpolation buffer: 100ms
render_pos = current_pos + (target_pos - current_pos) * t
where t = elapsed_time / interpolation_time
```

This creates smooth visuals even with delayed network updates.

## Security Features

- **Server Authority**: Clients cannot spoof scores or positions
- **Input Validation**: Server validates all client inputs
- **Proximity Checking**: Server verifies player is close enough to collect coins
- **State Ownership**: Only server modifies canonical game state

## Project Structure

```
.
├── CMakeLists.txt
├── README.md
├── common/
│   └── protocol.hpp         # Shared network protocol
├── server/
│   ├── main.cpp             # Server entry point
│   ├── game_server.hpp/cpp  # Network server
│   └── game_session.hpp/cpp # Game logic
└── client/
    ├── main.cpp             # Client entry point
    ├── game_client.hpp/cpp  # Network client
    └── renderer.hpp/cpp     # SDL2 rendering
```

## Testing Checklist

- [x] Two clients can connect
- [x] Players can move with WASD/arrows
- [x] Coins spawn randomly every 3 seconds
- [x] Collision detection works correctly
- [x] Scores increment on collection
- [x] Interpolation provides smooth movement
- [x] 200ms latency is simulated
- [x] Server authority is maintained
- [x] Clients cannot cheat

## Known Limitations

1. **Font Loading**: May need to adjust font paths for your system
2. **No Reconnection**: Clients cannot reconnect after disconnect
3. **No Lobby System**: Game auto-starts with 2 players
4. **Fixed Map Size**: 800x600 hardcoded

## Future Development Plan

See [FUTURE_DEVELOPMENT.md](FUTURE_DEVELOPMENT.md) for roadmap.

## Video Demonstration

Record your gameplay showing:

1. Two client windows side-by-side
2. Smooth player movement with latency
3. Coin collection and score updates
4. Server console showing events

## Troubleshooting

### Build Issues

- Ensure CMake 3.20+ is installed
- Check internet connection for FetchContent
- Try clearing build directory: `rm -rf build`

### Connection Issues

- Check firewall settings
- Verify server is running before starting clients
- Ensure correct IP/port in client args

### Font Not Loading

Edit `client/renderer.cpp` line with `TTF_OpenFont` to point to a valid TTF font on your system.

## License

MIT License - Feel free to use for educational purposes.

## Author

Created for Associate Game Developer Technical Test
