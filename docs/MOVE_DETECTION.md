# ChessGrid General Move Detection

## Overview

ChessGrid Board detects chess moves by monitoring changes across 64 board sensors.

The movement detection system is designed around a **general sensor-delta logic** rather than separate detection algorithms for each chess move type.

The firmware compares the current sensor state against the previously validated board position, called the **baseline**.

From these physical changes, the firmware determines:

* Which square was left first
* Which previously empty square became occupied
* Which previously occupied square was disturbed
* Whether the movement represents a candidate chess move

The resulting move is then validated by `chess.hpp`.

This separation allows the same detection system to support normal moves, captures, en passant, castling, and promotion.

---

## Baseline

After a valid chess move has been completed, the current 64-square sensor state becomes the new baseline.

The baseline represents the last known valid physical board position.

```text
Valid Chess Position
        │
        ▼
  Sensor State
        │
        ▼
     Baseline
```

Every subsequent sensor change is interpreted relative to this baseline.

---

## Movement State Machine

Movement detection uses four states:

```text
WAITING_FOR_POSITION
        │
        ▼
      READY
        │
        │ first piece lifted
        ▼
    MOVEMENT
        │
        ├── valid move ──→ READY
        │
        ├── board restored → READY
        │
        └── illegal move ─→ RECOVERY
                              │
                              │ board restored
                              ▼
                            READY
```

### `WAITING_FOR_POSITION`

The board is not currently in a valid starting position.

No movement is processed until the expected position is detected.

### `READY`

The board is stable and ready for the next move.

### `MOVEMENT`

A piece has been detected leaving its source square.

The firmware records movement information while waiting for the player to complete the move and press the clock.

### `RECOVERY`

The detected movement was invalid.

The player must return the board to the previous valid position.

Once the board matches the baseline again, the system returns to `READY`.

---

# Movement Tracking

The firmware maintains three important movement variables:

```cpp
int movementSource = -1;
int firstDestination = -1;
int lastDisturbed = -1;
```

Each variable represents a different physical event.

---

## 1. Movement Source

The first square that changes from:

```text
ON → OFF
```

while the board is in `READY` is considered the movement source.

The source is immediately locked.

```text
ON → OFF
    │
    ▼
movementSource
    │
    ▼
SOURCE LOCKED
```

Once locked, subsequent disturbances cannot replace the source.

This is important because a single chess move can cause several sensor transitions, especially during captures and castling.

---

# 2. First Destination

A square that was **empty in the baseline** and changes from:

```text
OFF → ON
```

is considered a destination candidate.

The first such square detected is stored as:

```cpp
firstDestination
```

The firmware does not replace it with later OFF → ON transitions.

Conceptually:

```text
Baseline: OFF
Current:  ON
          │
          ▼
firstDestination
```

This simple rule is particularly useful for castling and en passant.

---

# 3. Last Disturbed

The firmware also tracks a different type of event:

```text
Baseline: ON
Current:  OFF
```

This represents a square that was occupied in the previous valid position but has subsequently been disturbed.

Such a square is stored as:

```cpp
lastDisturbed
```

The source square itself is excluded from this tracking.

Conceptually:

```text
Baseline: ON
Current:  OFF
          │
          ▼
lastDisturbed
```

The distinction between `firstDestination` and `lastDisturbed` is fundamental to the movement detection system.

```text
firstDestination
    = previously empty → now occupied

lastDisturbed
    = previously occupied → now empty
```

---

# Normal Move

Consider:

```text
e2 → e4
```

Initial baseline:

```text
e2 = ON
e4 = OFF
```

The physical changes are:

```text
e2: ON → OFF
e4: OFF → ON
```

The firmware records:

```text
movementSource  = e2
firstDestination = e4
```

The resulting candidate move is:

```text
e2e4
```

It is then passed to `chess.hpp` for legality validation.

---

# Capture

Consider:

```text
e4 × d5
```

Initial baseline:

```text
e4 = ON
d5 = ON
```

Because the destination was already occupied, it does not produce the normal:

```text
OFF → ON
```

destination transition.

Instead, the captured piece produces:

```text
d5: ON → OFF
```

This is recorded as a disturbance.

The resulting movement information is:

```text
movementSource = e4
lastDisturbed  = d5
```

When the clock is clicked, the firmware uses `lastDisturbed` as the destination if no `firstDestination` exists.

```text
source → lastDisturbed
```

Result:

```text
e4d5
```

---

# En Passant

En passant is one of the most important reasons for maintaining both `firstDestination` and `lastDisturbed`.

Consider:

```text
White pawn: e5
Black pawn: d5

White plays: exd6
```

Baseline:

```text
e5 = ON
d5 = ON
d6 = OFF
```

After the move:

```text
e5: ON → OFF
d6: OFF → ON
d5: ON → OFF
```

The firmware therefore records:

```text
movementSource  = e5
firstDestination = d6
lastDisturbed    = d5
```

The important point is that **`d5` is not the destination**.

It is the captured pawn's square.

The actual destination is `d6`, which is identified by the OFF → ON transition.

Because `firstDestination` exists, it takes priority over `lastDisturbed`.

Therefore:

```text
e5 → d6
```

is generated rather than:

```text
e5 → d5
```

`chess.hpp` then determines whether `e5d6` is a legal en passant move.

No dedicated en passant sensor algorithm is required.

---

# Castling

Castling moves two pieces and therefore creates multiple physical changes.

For example, king-side castling produces two previously empty squares becoming occupied.

The sensor layer may therefore detect:

```text
OFF → ON
OFF → ON
```

Rather than creating a special sensor rule for castling, ChessGrid uses the same general destination logic.

The **first detected OFF → ON square** is retained as:

```text
firstDestination
```

This provides the king's destination.

The resulting move is then validated by `chess.hpp`, which understands the castling rule and updates the chess position accordingly.

The sensor layer therefore does not need to explicitly determine:

```text
"This is castling."
```

It only needs to identify the physical movement.

---

# Promotion

Promotion uses the same source and destination detection system as a normal pawn move.

The only additional information required is the desired promotion piece.

The firmware accepts four promotion modifiers:

```text
N = Knight
B = Bishop
R = Rook
Q = Queen
```

The modifier is appended to the generated UCI move.

Examples:

```text
e7e8q
e7e8r
e7e8b
e7e8n
```

This allows the player to select the promotion piece while keeping the underlying movement detection completely general.

---

# Move Candidate Resolution

When the player presses the clock, the firmware resolves the movement information.

The basic priority is:

```text
1. movementSource
2. firstDestination
3. lastDisturbed
```

In simplified form:

```text
source = movementSource

if firstDestination exists:
    destination = firstDestination

else if lastDisturbed exists:
    destination = lastDisturbed

else:
    movement is invalid
```

This produces a source/destination candidate that can be converted into UCI notation.

---

# Movement Cancellation

A movement can be cancelled simply by returning the board to its previous state.

The firmware continuously compares the current sensor state against the baseline.

If:

```text
current sensor state == baseline
```

the movement is considered cancelled.

The movement tracking variables are cleared and the board returns to:

```text
READY
```

This allows a player to lift a piece and put it back without generating a chess move.

---

# Chess Validation

The sensor system intentionally does **not** determine chess legality.

After the movement candidate is resolved, the firmware creates a UCI move:

```text
source + destination + optional promotion modifier
```

For example:

```text
e2e4
e7e8q
e1g1
e5d6
```

The move is then passed to `chess.hpp`.

```text
Physical Board
      │
      ▼
Sensor Changes
      │
      ▼
Movement Candidate
      │
      ▼
UCI Move
      │
      ▼
   chess.hpp
      │
   ┌──┴──┐
   ▼     ▼
 LEGAL  ILLEGAL
   │       │
   ▼       ▼
Update   Recovery
Board
```

For a legal move:

1. The chess position is updated.
2. The current sensor state becomes the new baseline.
3. Movement tracking is cleared.
4. The promotion modifier is cleared.
5. The UCI move is sent to ChessGrid Live.
6. The board returns to `READY`.

For an illegal move:

1. The move is rejected.
2. Movement tracking is cleared.
3. The board enters `RECOVERY`.
4. The player must return the physical board to the previous valid position.

---

# Design Principle

The core design principle of ChessGrid Board is the separation between **physical movement detection** and **chess interpretation**.

The sensor layer answers:

> **What physically changed on the board?**

The chess library answers:

> **What chess move does this represent, and is that move legal?**

This separation allows the same movement detection engine to handle:

* Normal moves
* Captures
* En passant
* Castling
* Promotion
* Movement cancellation
* Illegal moves and recovery

without requiring a separate sensor-detection algorithm for each chess rule.

The result is a general-purpose movement detection layer that converts physical board activity into validated chess moves.
