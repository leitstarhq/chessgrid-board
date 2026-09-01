# ChessGrid Move Detection

## Overview

ChessGrid detects chess moves by observing changes in the state of the 64 board sensors.

The movement detection system is designed around a simple principle:

> **Translate physical sensor-state changes into a UCI move candidate, then let the chess library determine whether the move is legal.**

The firmware does not attempt to understand chess rules during movement detection.

---

## Design Principle

The movement detection system does not attempt to understand chess rules.

Its primary responsibility is to translate changes in the 64 sensor states into a **UCI move candidate**.

```text
Sensor States
      ↓
Movement Detection
      ↓
UCI Move Candidate
      ↓
chess.hpp
      ↓
Legal / Illegal
```

The movement detector is therefore concerned with **physical board behavior**, not chess legality.

For example, if the physical board indicates that a piece moved from `e5` to `d6`, the movement detector produces:

```text
e5d6
```

It does not need to determine whether the move is:

* a normal pawn move
* a capture
* an en passant capture

That decision is delegated to `chess.hpp`.

Likewise, the movement detector does not need separate chess-rule logic for:

* castling
* promotion legality
* check
* pinned pieces
* blocked pieces
* castling rights
* en passant rules

The movement detector only needs to correctly translate the physical board changes into the appropriate UCI candidate.

This separation keeps the sensor layer general while allowing `chess.hpp` to remain responsible for chess rules and move legality.

---

## Baseline State

After every valid move, the firmware stores the current sensor state as the new **baseline**.

The baseline represents the last known valid chess position.

Each sensor therefore has two relevant states:

```text
baselineState[]
sensorState[]
```

The movement detector compares changes in `sensorState` against the baseline.

Conceptually:

```text
Previous Valid Position
        ↓
      Baseline
        ↓
Sensor State Changes
        ↓
Movement Candidate
```

The detector does not need to continuously reconstruct the entire chess position from scratch.

It only needs to understand how the current physical changes relate to the previous valid position.

---

## Movement Tracking

Three variables form the core of the movement detection system:

```cpp
movementSource
firstDestination
lastDisturbed
```

### `movementSource`

The source is identified when a square that was occupied in the baseline changes:

```text
ON → OFF
```

Because chess pieces are assumed to be lifted before being placed elsewhere, this transition identifies the square from which the piece was removed.

Once detected, the source is locked.

```text
baseline: ON
current:  OFF

        ↓

movementSource = square
```

---

### `firstDestination`

The first previously-empty square that becomes occupied is detected as:

```text
OFF → ON
```

This square becomes `firstDestination`.

```text
baseline: OFF
current:  ON

        ↓

firstDestination = square
```

The first such transition is preserved because, for several chess moves, it represents the square where the moving piece is intended to arrive.

---

### `lastDisturbed`

After the source has been identified, additional previously-occupied squares may become empty:

```text
ON → OFF
```

These are tracked as disturbances.

The latest such square, excluding the source itself, is stored as:

```cpp
lastDisturbed
```

This is particularly useful for captures where the destination square was already occupied.

---

# Translating Sensor States into UCI

The movement detector ultimately needs to produce only a UCI move candidate.

The general process is:

```text
Sensor State Changes
        ↓
Identify Source
        ↓
Identify Destination / Disturbed Square
        ↓
Construct UCI
        ↓
chess.hpp
```

The detector does not classify the chess move itself.

---

## Normal Move

For a normal move:

```text
Source:      ON → OFF
Destination: OFF → ON
```

The resulting UCI candidate is:

```text
source + destination
```

Example:

```text
e2 → e4
```

produces:

```text
e2e4
```

---

## Normal Capture

For a normal capture, the destination square was already occupied.

Therefore, the destination does not produce an `OFF → ON` transition.

Instead, the captured piece is removed:

```text
Source:        ON → OFF
Destination:   ON
Captured piece: ON → OFF
```

The detector therefore uses `lastDisturbed` to identify the occupied square that was disturbed after the source was removed.

Conceptually:

```text
movementSource → lastDisturbed
```

The resulting UCI candidate is then passed to `chess.hpp`.

Example:

```text
e4xd5
```

produces:

```text
e4d5
```

The movement detector does not need to label this as a capture. `chess.hpp` determines that from the current chess position.

---

## En Passant

En passant is an important example of why the movement detector does not need dedicated chess-rule logic.

Consider:

```text
e5xd6 e.p.
```

Before the move:

```text
e5 = ON
d5 = ON
d6 = OFF
```

Physical changes occur approximately as:

```text
e5: ON → OFF
d6: OFF → ON
d5: ON → OFF
```

The detector therefore records:

```text
movementSource = e5
firstDestination = d6
lastDisturbed = d5
```

The UCI candidate becomes:

```text
e5d6
```

The detector does not need an `if (enPassant)` condition.

`chess.hpp` receives:

```text
e5d6
```

and, given the current board position, determines that this is a legal en passant capture.

The physical movement pattern therefore naturally produces the correct UCI move.

---

## Castling

Castling produces multiple sensor changes because both the king and rook move.

For example, kingside castling:

```text
e1 → g1
h1 → f1
```

The detector may observe:

```text
e1: ON → OFF
g1: OFF → ON
h1: ON → OFF
f1: OFF → ON
```

The first destination detected is:

```text
g1
```

Therefore the movement candidate is:

```text
e1g1
```

The detector does not need to recognize that two pieces moved or explicitly implement castling logic.

`chess.hpp` interprets `e1g1` according to the current chess position and performs the appropriate legal castling move, including the rook movement.

---

## Promotion

Promotion uses the same source/destination detection mechanism.

The only additional information required is the promotion piece.

The firmware supports:

```text
n = Knight
b = Bishop
r = Rook
q = Queen
```

For example:

```text
e7 → e8
```

with queen promotion produces:

```text
e7e8q
```

The promotion modifier is appended to the UCI candidate before it is passed to `chess.hpp`.

Again, the movement detector does not need to determine whether promotion is legal. It only supplies the requested UCI move.

---

# Movement Cancellation

A movement can be cancelled if the board returns to the baseline state before a move is committed.

The firmware continuously checks:

```cpp
boardMatchesBaseline()
```

If every sensor matches the baseline again:

```text
Current State == Baseline State
```

the movement is cleared and the board returns to:

```text
READY
```

This allows a user to temporarily lift a piece and return it to its original square without generating a move.

---

# Illegal Move and Recovery

After constructing a UCI candidate, the firmware passes it to `chess.hpp`.

Conceptually:

```text
UCI Candidate
      ↓
chess.hpp
      ↓
Legal?
   /     \
 YES      NO
 ↓        ↓
Commit   Recovery
```

If the move is legal:

1. The chess board state is updated.
2. The UCI move is sent to ChessGrid Live.
3. The current sensor state becomes the new baseline.
4. Movement tracking is cleared.
5. The state returns to `READY`.

If the move is illegal:

1. The movement is rejected.
2. The firmware enters `RECOVERY`.
3. The board must return to the baseline position before normal movement detection resumes.

This prevents an illegal physical arrangement from silently becoming the new chess state.

---

# State Machine

The movement detector uses four primary states:

```text
WAITING_FOR_POSITION
        ↓
      READY
        ↓
    MOVEMENT
        ↓
      READY
```

An illegal move enters:

```text
MOVEMENT
    ↓
RECOVERY
    ↓
READY
```

### `WAITING_FOR_POSITION`

Initial state.

The firmware waits until the physical board represents a valid starting position.

---

### `READY`

The board is synchronized with the current chess state and is ready for a new movement.

---

### `MOVEMENT`

A physical movement has been detected.

The firmware tracks:

```text
movementSource
firstDestination
lastDisturbed
```

until the movement is committed, cancelled, or rejected.

---

### `RECOVERY`

The physical board does not represent a valid committed chess position.

The firmware waits until the sensors once again match the baseline.

Only then does it return to `READY`.

---

# Move Processing

Once a movement has been identified, the firmware constructs a UCI string.

Conceptually:

```text
Source
  +
Destination
  +
Optional Promotion Modifier
        ↓
      UCI
```

For example:

```text
e2 + e4
    ↓
e2e4
```

or:

```text
e7 + e8 + q
    ↓
e7e8q
```

The resulting UCI candidate is passed to `chess.hpp`.

A legal move is then committed to the internal chess board.

---

# Why This Approach Is General

The important property of this design is that the movement detector does not contain separate algorithms for every chess move type.

Instead, it operates on a small number of physical transition patterns:

```text
ON → OFF
OFF → ON
```

and tracks their relationship to the baseline.

The chess library then provides the chess-specific interpretation.

This means the same movement detection mechanism can support:

```text
Normal moves
Captures
En passant
Castling
Promotion
```

without requiring dedicated sensor logic for each chess rule.

The architecture can therefore be summarized as:

```text
             PHYSICAL BOARD
                    │
                    ▼
             Sensor States
                    │
                    ▼
          Movement Detection
                    │
                    ▼
             UCI Candidate
                    │
                    ▼
               chess.hpp
                    │
              ┌─────┴─────┐
              ▼           ▼
           Legal        Illegal
              │           │
              ▼           ▼
        Commit Move    Recovery
              │
              ▼
       Save New Baseline
```

---

# Summary

ChessGrid's movement detection is intentionally **not a chess engine**.

Its responsibility is limited to:

1. Observe changes in the 64 sensor states.
2. Compare those changes against the previous valid baseline.
3. Identify the movement source.
4. Identify the destination or relevant disturbance.
5. Translate the physical movement into a UCI move candidate.
6. Pass that candidate to `chess.hpp`.

`chess.hpp` is responsible for determining whether the resulting UCI move is legal and for applying the corresponding chess rules.

This separation allows the physical sensor layer to remain simple and general while the chess library handles the complexity of chess itself.
