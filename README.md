# ChessGrid Board

ESP32 firmware for the ChessGrid electronic chessboard.

ChessGrid Board is responsible for detecting physical chess piece movement, maintaining the board state, validating chess moves, and communicating with ChessGrid Live.

## Status

**Current version: `v0.1.0-alpha`**

ChessGrid Board is currently in the **Alpha / Proof-of-Concept** stage.

The core chess logic, movement detection, and communication pipeline are functional using a simulated sensor input layer. Physical reed switch and MUX integration are still under development.

---

## Architecture

```text
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
          │     └── Chess Logic & Validation
          │
          └── WebSocket
                  │
                  ▼
          ChessGrid Live
```

---

## Features

* ESP32-based chessboard controller
* 64-square sensor input simulation
* Chess position tracking
* General movement detection
* Source square locking
* Destination detection
* Capture detection
* En passant support
* Castling support
* Promotion support
* Movement cancellation
* Illegal move recovery
* Legal move validation using `chess.hpp`
* UCI move generation
* Promotion modifiers (`N`, `B`, `R`, `Q`)
* WiFi configuration and connection
* mDNS support
* WebSocket communication with ChessGrid Live
* Device identification
* Server authorization flow
* Live move transmission

---

## General Move Detection

ChessGrid Board uses a **general sensor-delta movement detection system**.

Instead of implementing separate sensor algorithms for normal moves, captures, en passant, castling, and promotion, the firmware observes changes from the previous valid board position (**baseline**) and produces a movement candidate.

The core movement tracking uses:

```text
movementSource
firstDestination
lastDisturbed
```

The first **ON → OFF** transition identifies and locks the source.

The first previously-empty square that changes **OFF → ON** becomes `firstDestination`.

Previously-occupied squares that subsequently change **ON → OFF** are tracked as `lastDisturbed`.

This allows the same detection system to handle different types of chess moves.

For example:

```text
Normal move:
source → firstDestination

Capture:
source → lastDisturbed

En passant:
source → firstDestination
              +
       lastDisturbed = captured pawn

Castling:
source → firstDestination
```

The resulting movement candidate is converted into UCI notation and validated by `chess.hpp`.

### Detailed Documentation

For the complete movement detection design, state machine, baseline handling, capture logic, en passant handling, castling, promotion, and recovery behavior:

**[General Move Detection](docs/MOVE_DETECTION.md)**

---

## Chess Logic

ChessGrid Board uses `chess.hpp` for chess game logic and legal move validation.

The firmware converts detected physical movements into UCI moves such as:

```text
e2e4
e7e5
g1f3
```

Special moves including captures, en passant, castling, and promotion are validated by the chess library rather than being implemented as separate physical sensor rules.

---

## Promotion

Promotion moves support the standard UCI promotion modifiers:

```text
N = Knight
B = Bishop
R = Rook
Q = Queen
```

Examples:

```text
e7e8q
e7e8r
e7e8b
e7e8n
```

---

## Communication

ChessGrid Board communicates with ChessGrid Live through WebSocket.

### ESP32 → Server

```text
HELLO <device_id>
<uci_move>
RESET
```

Example:

```text
HELLO CG-1
e2e4
RESET
```

### Server → ESP32

```text
PENDING
ACCEPTED
REJECTED
ERROR <message>
```

The server authorization system prevents unapproved ChessGrid boards from interacting with the live server.

---

## WiFi

The ESP32 supports WiFi configuration through the board's configuration interface.

The board can start an access point for initial network configuration before connecting to the configured WiFi network.

mDNS support is also included for local network discovery.

---

## Hardware

The physical sensor system is still under development.

The planned hardware architecture is:

```text
64 × Reed Switches
        │
        ▼
       MUX
        │
        ▼
      ESP32
```

The current firmware uses a **simulated sensor input layer** for development and testing.

Planned hardware work includes:

* Physical reed switch integration
* MUX integration
* PCB design
* Magnet positioning
* Sensor calibration
* Power management
* Mechanical board construction

---

## Requirements

* ESP32
* Arduino framework
* `chess.hpp`
* WiFi network
* ChessGrid Live server

---

## Project Structure

```text
chessgrid-board/
├── chessgrid.ino
├── chess.hpp
├── docs/
│   └── MOVE_DETECTION.md
└── README.md
```

---

## Related Project

The server and browser interface are maintained separately:

**ChessGrid Live**

`chessgrid-live`

---

## Version

```text
v0.1.0-alpha
```

This release represents the first functional software prototype of ChessGrid Board, establishing the core chess logic, movement detection, and communication pipeline before physical sensor integration.

---

## License

To be defined.
