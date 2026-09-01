# ChessGrid Move Detection

## Overview

ChessGrid detects chess moves by observing changes in the state of the 64 board sensors.

The movement detection system is built around a simple principle:

> **Translate physical sensor-state changes into a UCI move candidate, then let `chess.hpp` determine whether the move is legal.**

The firmware does not attempt to understand chess rules during movement detection.

---

## Design Principle

The movement detection system is not a chess engine.

Its primary responsibility is to translate physical changes in the 64 sensor states into a **UCI move candidate**.

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

The movement detector is concerned with **physical board behavior**, not chess legality.

For example, if the physical board indicates that a piece moved from `e5` to `d6`, the movement detector produces:

```text
e5d6
```

It does not need to determine whether the move is:

* a normal move
* a capture
* an en passant capture

That decision is delegated to `chess.hpp`.

Likewise, the movement detector does not need dedicated chess-rule logic for:

* castling
* en passant
* promotion legality
* check
* pinned pieces
* blocked pieces
* castling rights

It only needs to correctly translate the physical sensor changes into the appropriate UCI candidate.

This separation keeps the sensor layer general while allowing `chess.hpp` to remain responsible for chess rules and move legality.

---

# Baseline State

After every valid move, the firmware stores the current sensor state as the new **baseline**.

The baseline represents the last known valid chess position.

Each sensor therefore has two relevant states:

```cpp
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

The detector does not need to reconstruct the entire chess position from scratch.

It only needs to determine how the current physical changes relate to the previous valid position.

---

# Core Movement Tracking

Three variables form the core of the movement detection system:

```cpp
movementSource
firstDestination
lastDisturbed
```

These variables describe the physical movement pattern from the baseline.

---

## `movementSource`

The source is identified when a square that was occupied in the baseline changes:

```text
ON → OFF
```

Because a chess piece is expected to be lifted before being placed elsewhere, this transition identifies the square from which the piece was removed.

The first valid source transition **locks the movement source**.

```text
Baseline: ON
Current:  OFF

        ↓

movementSource = square
        ↓
     SOURCE LOCK
```

Once locked, subsequent `ON → OFF` transitions are no longer considered possible movement sources.

This is important because some chess moves temporarily produce more than one occupied square becoming empty.

---

## Source Lock

Source lock establishes which piece is being moved before interpreting subsequent sensor changes.

Consider an en passant capture:

```text
e5xd6 e.p.
```

The physical board may produce:

```text
e5: ON → OFF
d6: OFF → ON
d5: ON → OFF
```

Without source lock, both `e5` and `d5` could appear to be candidate sources because both were occupied and both become empty.

With source lock:

```text
e5: ON → OFF
        ↓
movementSource = e5
        ↓
SOURCE LOCK
```

When `d5` later changes:

```text
d5: ON → OFF
```

it cannot become another source.

It is instead interpreted as a **disturbance** and stored as:

```text
lastDisturbed = d5
```

This distinction is fundamental to the movement detection system.

The first valid source transition establishes the moving piece. All subsequent occupied-square removals are interpreted relative to that locked source.

---

## `firstDestination`

The destination is identified from the first previously-empty square that becomes occupied:

```text
OFF → ON
```

This square is immediately stored as:

```cpp
firstDestination
```

and is **locked as the movement destination**.

Subsequent `OFF → ON` transitions do not replace `firstDestination`.

```text
Baseline: OFF
Current:  ON

        ↓

firstDestination = square
```

This is important because some chess moves produce more than one `OFF → ON` transition.

### Castling Example

During kingside castling:

```text
e1: ON → OFF
g1: OFF → ON
h1: ON → OFF
f1: OFF → ON
```

The first empty square that becomes occupied is `g1`.

Therefore:

```text
movementSource   = e1
firstDestination = g1
```

The later `f1: OFF → ON` transition does not change the destination.

The resulting UCI candidate is:

```text
e1g1
```

This allows castling to be handled by the same movement detection mechanism without the sensor layer needing to understand the concept of castling.

`chess.hpp` receives `e1g1` and handles the actual chess rule, including the rook movement.

---

## `lastDisturbed`

After the source has been identified, other previously-occupied squares may become empty:

```text
ON → OFF
```

These transitions are tracked as disturbances.

The latest such square, excluding the locked movement source itself, is stored as:

```cpp
lastDisturbed
```

This is particularly useful when a piece is captured on an already-occupied destination square.

For example:

```text
Source:       ON → OFF
Destination:  ON
Captured:     ON → OFF
```

The destination itself does not produce an `OFF → ON` transition because it was already occupied.

`lastDisturbed` provides the additional physical information needed to construct the UCI candidate.

---

# Translating Sensor States into UCI

The movement detector ultimately produces only a UCI move candidate.

The general process is:

```text
Sensor State Changes
        ↓
Identify Source
        ↓
Lock Source
        ↓
Identify First Destination
        ↓
Track Additional Disturbances
        ↓
Construct UCI
        ↓
chess.hpp
```

The detector does not classify the chess move.

It simply translates the physical movement pattern into UCI.

---

# Normal Move

For a normal move:

```text
Source:      ON → OFF
Destination: OFF → ON
```

The movement detector produces:

```text
movementSource + firstDestination
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

# Normal Capture

For a normal capture, the destination square was already occupied.

Therefore, there is no `OFF → ON` transition on the destination.

Instead:

```text
Source:       ON → OFF
Destination:  ON
Captured:     ON → OFF
```

The movement detector uses `lastDisturbed` to identify the occupied square that was disturbed after the source was locked.

Conceptually:

```text
movementSource → lastDisturbed
```

Example:

```text
e4 → d5
```

produces:

```text
e4d5
```

The movement detector does not need to label this as a capture.

`chess.hpp` determines that from the current chess position.

---

# En Passant

En passant demonstrates the importance of **source lock** and `firstDestination`.

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

The physical changes are:

```text
e5: ON → OFF
d6: OFF → ON
d5: ON → OFF
```

The detector processes these transitions as:

```text
e5: ON → OFF
        ↓
movementSource = e5
        ↓
SOURCE LOCK

d6: OFF → ON
        ↓
firstDestination = d6

d5: ON → OFF
        ↓
lastDisturbed = d5
```

The resulting movement candidate is:

```text
e5d6
```

The detector does not need to determine that the pawn on `d5` is being captured en passant.

`chess.hpp` receives:

```text
e5d6
```

and, based on the current chess position, determines whether the move is a legal en passant capture.

The important point is that source lock prevents `d5` from being interpreted as a second source.

---

# Castling

Castling produces two pieces moving and therefore multiple sensor transitions.

For kingside castling:

```text
e1 → g1
h1 → f1
```

The sensor changes may be:

```text
e1: ON → OFF
g1: OFF → ON
h1: ON → OFF
f1: OFF → ON
```

The detector records:

```text
movementSource   = e1
firstDestination = g1
lastDisturbed    = h1
```

The second `OFF → ON` transition at `f1` is ignored for destination selection because `firstDestination` has already been locked.

The resulting UCI candidate is:

```text
e1g1
```

`chess.hpp` then determines that `e1g1` represents legal castling and applies the corresponding king and rook movement.

Queenside castling follows the same principle.

The movement detector does not need a special `castle` state.

---

# Promotion

Promotion uses the same source and destination detection mechanism.

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

The movement detector does not need to determine whether promotion is legal. It only supplies the requested UCI move.

---

# Movement Cancellation

A movement can be cancelled if the board returns to the baseline state before a move is committed.

The firmware checks:

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

This allows a piece to be temporarily lifted and returned to its original square without generating a move.

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
   /    \
 YES     NO
 ↓       ↓
Commit  Recovery
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
3. The physical board must return to the baseline position.
4. Once the baseline is restored, the firmware returns to `READY`.

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

The physical board does not represent the last committed chess position.

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

Examples:

```text
e2 + e4
    ↓
e2e4
```

and:

```text
e7 + e8 + q
    ↓
e7e8q
```

The resulting UCI candidate is passed to `chess.hpp`.

If the move is legal, the internal chess board is updated and the current sensor state becomes the new baseline.

---

# Why This Approach Is General

The key property of this design is that the movement detector does not contain separate algorithms for every chess move type.

Instead, it operates on a small number of physical transition patterns:

```text
ON → OFF
OFF → ON
```

combined with the order in which those transitions occur.

The detector identifies:

```text
movementSource
      ↓
  SOURCE LOCK
      ↓
firstDestination
      ↓
lastDisturbed
```

and converts that physical information into a UCI candidate.

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
          ┌──────────┴──────────┐
          │                     │
          ▼                     ▼
 movementSource        firstDestination
          │                     │
          │                FIRST ON→OFF
          │                DESTINATION LOCK
          │                     │
          └──────────┬──────────┘
                     │
              Additional
              Disturbances
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
3. Identify the first occupied square that becomes empty as `movementSource`.
4. **Lock the source immediately** so subsequent occupied-square removals cannot be interpreted as another source.
5. Identify the first previously-empty square that becomes occupied as `firstDestination` and lock it.
6. Track subsequent occupied squares that become empty as `lastDisturbed`.
7. Translate the physical movement into a UCI move candidate.
8. Pass that candidate to `chess.hpp`.

`chess.hpp` is responsible for determining whether the resulting UCI move is legal and for applying the corresponding chess rules.

The resulting separation of responsibilities is:

```text
ChessGrid Board
    = Physical State → UCI

chess.hpp
    = UCI → Chess Legality / State
```

This separation allows the physical sensor layer to remain simple and general while the chess library handles the complexity of chess itself.
