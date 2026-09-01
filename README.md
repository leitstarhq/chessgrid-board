# ChessGrid Board

ESP32 firmware for the ChessGrid electronic chessboard.

ChessGrid Board is responsible for detecting chess piece movement, maintaining the current board state, validating chess moves, and communicating with ChessGrid Live.

## Status

**Current version: `v0.1.0-alpha`**

ChessGrid Board is currently in the alpha / proof-of-concept stage.

The current firmware establishes the core chess logic, movement detection, and communication pipeline using a simulated reed switch input layer. Physical sensor and MUX integration are still under development.

## Architecture

```text id="7y3x8q"
Reed Switch Input
    (Simulation)
          │
          ▼
        ESP32
          │
          ├── Board State
          │
          ├── Movement Detection
          │
          ├── chess.hpp
          │     └── Chess logic & legal moves
          │
          └── WebSocket
                  │
                  ▼
          ChessGrid Live
```

## Features

* ESP32-based chessboard controller
* 64-square reed switch input simulation
* Chess position tracking
* Movement detection using a state machine
* Legal move validation using `chess.hpp`
* UCI move generation
* Capture detection
* Castling detection
* Promotion handling
* Board reset handling
* WiFi configuration and connection
* mDNS support
* WebSocket communication with ChessGrid Live
* Device identification and authorization
* Live move transmission to ChessGrid Live

## Chess Logic

ChessGrid Board uses [`chess.hpp`](https://github.com/Disservin/chess-library) for chess game logic and legal move validation.

The firmware converts detected board movements into UCI moves such as:

```text id="2h2kqk"
e2e4
e7e5
g1f3
```

Special moves such as castling, captures, and promotion are handled by the board state and movement detection logic before the resulting UCI move is sent to ChessGrid Live.

## Movement Detection

The firmware uses a state machine to interpret changes in the 64-square board state.

The current implementation handles:

* Normal moves
* Captures
* Castling
* Promotion
* Movement cancellation
* Board state recovery
* Baseline position tracking

The current reed switch layer is simulated for software development and testing.

## Communication

ChessGrid Board communicates with ChessGrid Live through WebSocket.

### ESP32 → Server

```text id="q4by6g"
HELLO <device_id>
<uci_move>
RESET
```

Example:

```text id="s2rj8e"
HELLO CG-1
e2e4
RESET
```

### Server → ESP32

The board currently handles messages including:

```text id="x1h8l4"
PENDING
ACCEPTED
REJECTED
ERROR <message>
```

## WiFi

The ESP32 supports WiFi configuration and connection to a local router.

The board can start an access point for initial configuration before connecting to the configured WiFi network.

mDNS is also used when available to provide local network discovery.

## Hardware

The physical ChessGrid sensor system is still under development.

Planned hardware architecture:

```text id="6d5b3q"
64 × Reed Switches
        │
        ▼
       MUX
        │
        ▼
      ESP32
```

The current firmware does not yet represent the final physical implementation.

Planned hardware work includes:

* Physical reed switch integration
* MUX integration
* PCB design
* Magnet and reed switch positioning
* Sensor calibration
* Power system
* Mechanical board construction

## Requirements

* ESP32
* Arduino framework
* `chess.hpp`
* WiFi-capable local network
* ChessGrid Live server

## Project Structure

```text id="b4r1qy"
chessgrid-board/
├── chessgrid.ino
├── chess.hpp
└── README.md
```

Additional source files may be added as the firmware architecture evolves.

## Versioning

ChessGrid Board follows semantic versioning.

Current release:

```text id="x5k2s9"
v0.1.0-alpha
```

Alpha releases may introduce changes to the firmware architecture, sensor interface, and communication protocol.

## Related Project

The server and browser interface are maintained separately:

**ChessGrid Live**

`chessgrid-live`

## License

To be defined.
