#include <Arduino.h>

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ESPmDNS.h>

#undef sq

#include "chess.hpp"
#include "webserver.h"

using namespace chess;


// ============================================================
// CHESS
// ============================================================

Board board;


// ============================================================
// GAME STATE
// ============================================================

enum GameState {

  WAITING_FOR_POSITION,
  READY,
  MOVEMENT,
  RECOVERY
};

GameState gameState =
  WAITING_FOR_POSITION;


// ============================================================
// WEBSOCKET
// ============================================================

WebSocketsClient webSocket;

bool webSocketStarted = false;

const char* chessGridID = "CG-1";

bool serverApproved = false;

// ============================================================
// SENSOR
// ============================================================

const int SENSOR_COUNT = 64;

const char* squares[SENSOR_COUNT] = {

  "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",

  "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",

  "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",

  "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",

  "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",

  "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",

  "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",

  "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};


// ============================================================
// SENSOR STATE
// ============================================================

bool sensorState[SENSOR_COUNT];

bool baselineState[SENSOR_COUNT];


// ============================================================
// MOVEMENT TRACKING
// ============================================================

bool movementActive = false;

int movementSource = -1;

int firstDestination = -1;

int lastDisturbed = -1;


// ============================================================
// MOVE MODIFIER
// ============================================================

char promotionModifier = '\0';


// ============================================================
// READ SENSOR
// ============================================================

bool readSensor(int index) {

  return sensorState[index];
}


// ============================================================
// FIND SQUARE INDEX
// ============================================================

int findSquareIndex(String square) {

  for (int i = 0; i < SENSOR_COUNT; i++) {

    if (
      square.equalsIgnoreCase(
        squares[i]
      )
    ) {

      return i;
    }
  }

  return -1;
}


// ============================================================
// SCAN SENSORS
// ============================================================

void scanSensors() {

  Serial.println();
  Serial.println(
    "Scanning 64 sensors..."
  );

  Serial.println();


  for (int i = 0; i < SENSOR_COUNT; i++) {

    Serial.print(
      squares[i]
    );

    Serial.print(
      " = "
    );

    Serial.println(
      sensorState[i]
        ? "ON"
        : "OFF"
    );
  }


  Serial.println();

  Serial.println(
    "BOARD SENSORS READY"
  );
}


// ============================================================
// VALIDATE STARTING POSITION
// ============================================================

bool validateStartingPosition() {

  for (int i = 0; i < SENSOR_COUNT; i++) {

    bool shouldBeOccupied = false;

    int rank =
      i / 8;


    if (rank == 0) {

      shouldBeOccupied = true;

    }

    else if (rank == 1) {

      shouldBeOccupied = true;

    }

    else if (rank == 6) {

      shouldBeOccupied = true;

    }

    else if (rank == 7) {

      shouldBeOccupied = true;
    }


    if (
      sensorState[i] !=
      shouldBeOccupied
    ) {

      return false;
    }
  }


  return true;
}


// ============================================================
// CHECK STARTING POSITION
// ============================================================

bool checkStartingPosition() {

  Serial.println();

  Serial.println(
    "Checking starting position..."
  );


  if (
    validateStartingPosition()
  ) {

    gameState =
      READY;


    movementActive =
      false;

    movementSource =
      -1;

    firstDestination =
      -1;

    lastDisturbed =
      -1;


    Serial.println();

    Serial.println(
      "STARTING POSITION VALID"
    );

    Serial.println(
      "CHESSGRID READY"
    );


    return true;

  } else {

    gameState =
      WAITING_FOR_POSITION;


    Serial.println();

    Serial.println(
      "PIECES INVALID"
    );

    Serial.println(
      "Waiting for starting position..."
    );


    return false;
  }
}


// ============================================================
// CLEAR MOVEMENT
// ============================================================

void clearMovement() {

  movementActive =
    false;

  movementSource =
    -1;

  firstDestination =
    -1;

  lastDisturbed =
    -1;
}


// ============================================================
// CLEAR MODIFIER
// ============================================================

void clearModifier() {

  promotionModifier =
    '\0';
}


// ============================================================
// SAVE BASELINE
// ============================================================

void saveBaseline() {

  for (int i = 0; i < SENSOR_COUNT; i++) {

    baselineState[i] =
      sensorState[i];
  }
}


// ============================================================
// CHECK BOARD AGAINST BASELINE
// ============================================================

bool boardMatchesBaseline() {

  for (int i = 0; i < SENSOR_COUNT; i++) {

    if (
      sensorState[i] !=
      baselineState[i]
    ) {

      return false;
    }
  }

  return true;
}


// ============================================================
// PRINT TURN
// ============================================================

void printTurn() {

  if (
    board.sideToMove() ==
    Color::WHITE
  ) {

    Serial.println(
      "White>"
    );

  } else {

    Serial.println(
      "Black>"
    );
  }
}


// ============================================================
// WEBSOCKET EVENT
// ============================================================

void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
) {

  switch (type) {

    // ========================================================
    // CONNECTED
    // ========================================================

    case WStype_CONNECTED:

      Serial.println();
      Serial.println(
        "WebSocket connected!"
      );

      Serial.print(
        "Requesting connection as "
      );

      Serial.println(
        chessGridID
      );

      serverApproved = false;

      webSocket.sendTXT(
        String("HELLO ") + chessGridID
      );

      break;


    // ========================================================
    // DISCONNECTED
    // ========================================================

    case WStype_DISCONNECTED:

      serverApproved = false;

      Serial.println();
      Serial.println(
        "WebSocket disconnected"
      );

      break;


    // ========================================================
    // ERROR
    // ========================================================

    case WStype_ERROR:

      Serial.println();
      Serial.println(
        "WebSocket error"
      );

      break;


    // ========================================================
    // TEXT MESSAGE
    // ========================================================

    case WStype_TEXT: {

      String message =
        String((char*)payload);

      message.trim();


      Serial.print(
        "WebSocket message: "
      );

      Serial.println(
        message
      );


      // ======================================================
      // ACCEPTED
      // ======================================================

      if (
        message == "ACCEPTED"
      ) {

        serverApproved = true;

        Serial.println();
        Serial.println(
          "CHESSGRID CONNECTION APPROVED!"
        );

        Serial.println(
          "Live server access granted."
        );

        break;
      }


      // ======================================================
      // PENDING
      // ======================================================

      if (
        message == "PENDING"
      ) {

        serverApproved = false;

        Serial.println();
        Serial.println(
          "CONNECTION PENDING"
        );

        Serial.println(
          "Waiting for server approval..."
        );

        break;
      }


      // ======================================================
      // REJECTED
      // ======================================================

      if (
        message == "REJECTED"
      ) {

        serverApproved = false;

        Serial.println();
        Serial.println(
          "CONNECTION REJECTED"
        );

        break;
      }


      // ======================================================
      // OTHER SERVER MESSAGE
      // ======================================================

      Serial.println(
        "Unknown WebSocket message."
      );

      break;
    }


    // ========================================================
    // DEFAULT
    // ========================================================

    default:

      break;
  }
}


// ============================================================
// START WEBSOCKET
// ============================================================

void connectWebSocket() {

  if (
    websocketHost.length() == 0
  ) {

    Serial.println();
    Serial.println(
      "WebSocket host not configured."
    );

    return;
  }


  Serial.println();
  Serial.println(
    "=============================="
  );

  Serial.println(
    "STARTING WEBSOCKET"
  );

  Serial.println(
    "=============================="
  );


  Serial.print(
    "Server: "
  );

  Serial.println(
    websocketHost
  );


  Serial.println(
    "Protocol: WSS"
  );


  Serial.println(
    "Port: 443"
  );


  webSocket.beginSSL(
    websocketHost.c_str(),
    443,
    "/ws"
  );


  webSocket.onEvent(
    webSocketEvent
  );


  webSocket.setReconnectInterval(
    5000
  );


  webSocketStarted =
    true;


  Serial.println();

  Serial.println(
    "WebSocket client started."
  );
}


// ============================================================
// PROCESS MOVE
// ============================================================

void processMove(
  int from,
  int to,
  char modifier
) {

  if (
    from < 0 ||
    to < 0
  ) {

    return;
  }


  String uciMove =
    String(squares[from]) +
    String(squares[to]);


  if (
    modifier != '\0'
  ) {

    uciMove +=
      modifier;
  }


  Serial.println();

  Serial.print(
    "Detected move: "
  );

  Serial.println(
    uciMove
  );


  std::string moveText =
    uciMove.c_str();


  Move move =
    uci::uciToMove(
      board,
      moveText
    );


  // ==========================================================
  // LEGALITY CHECK
  // ==========================================================

  if (
    movegen::isLegal(
      board,
      move
    )
  ) {

    board.makeMove(
      move
    );


    Serial.print(
      "LEGAL: "
    );

    Serial.println(
      uciMove
    );


    // ========================================================
    // SEND MOVE TO WEBSOCKET
    // ========================================================

    if (
  webSocketStarted &&
  webSocket.isConnected() &&
  serverApproved
) {

      webSocket.sendTXT(
        uciMove.c_str()
      );


      Serial.println(
        "Move sent via WebSocket."
      );

    } else {

      Serial.println(
        "WebSocket not connected."
      );

      Serial.println(
        "Move was NOT sent."
      );
    }


    saveBaseline();

    clearMovement();

    clearModifier();


    gameState =
      READY;


    Serial.println();

    printTurn();


  } else {

    Serial.print(
      "ILLEGAL: "
    );

    Serial.println(
      uciMove
    );


    Serial.println();

    Serial.println(
      "MOVE REJECTED"
    );

    Serial.println(
      "RETURN PIECE TO ORIGINAL POSITION"
    );


    clearMovement();

    clearModifier();


    gameState =
      RECOVERY;
  }
}


// ============================================================
// CLOCK CLICK
// ============================================================

void clockClick() {

  Serial.println();

  Serial.println(
    "CLOCK CLICK"
  );


  if (
    gameState != MOVEMENT
  ) {

    Serial.println(
      "CLOCK CLICK IGNORED"
    );

    return;
  }


  // ==========================================================
  // NO MOVE
  // ==========================================================

  if (
    boardMatchesBaseline()
  ) {

    Serial.println(
      "NO MOVE"
    );

    Serial.println(
      "BOARD RETURNED TO BASELINE"
    );


    clearMovement();

    clearModifier();


    gameState =
      READY;


    Serial.println();

    printTurn();

    return;
  }


  // ==========================================================
  // SOURCE
  // ==========================================================

  int source =
    movementSource;


  if (
    source == -1
  ) {

    Serial.println(
      "INVALID SOURCE"
    );

    Serial.println(
      "MOVE REJECTED"
    );


    gameState =
      RECOVERY;

    clearMovement();
    clearModifier();

    return;
  }


  // ==========================================================
  // DESTINATION
  // ==========================================================

  int destination =
    firstDestination;


  if (
    destination == -1
  ) {

    destination =
      lastDisturbed;


    if (
      destination == -1
    ) {

      Serial.println(
        "NO VALID DESTINATION"
      );

      Serial.println(
        "MOVE REJECTED"
      );


      gameState =
        RECOVERY;

      clearMovement();
      clearModifier();

      return;
    }


    Serial.println(
      "CAPTURE"
    );
  }


  // ==========================================================
  // SAFETY
  // ==========================================================

  if (
    destination ==
    source
  ) {

    Serial.println(
      "INVALID MOVE"
    );

    Serial.println(
      "SOURCE == DESTINATION"
    );


    gameState =
      RECOVERY;

    clearMovement();
    clearModifier();

    return;
  }


  // ==========================================================
  // PROCESS
  // ==========================================================

  processMove(
    source,
    destination,
    promotionModifier
  );
}


// ============================================================
// SET MODIFIER
// ============================================================

void setModifier(
  char modifier
) {

  promotionModifier =
    modifier;


  Serial.println();

  Serial.print(
    "MOVE MODIFIER: "
  );

  Serial.println(
    modifier
  );
}


// ============================================================
// UPDATE SENSOR STATE
// ============================================================

void updateSensorState(
  int index,
  bool newState
) {

  bool oldState =
    sensorState[index];


  if (
    oldState ==
    newState
  ) {

    return;
  }


  sensorState[index] =
    newState;


  Serial.print(
    "Sensor: "
  );

  Serial.print(
    squares[index]
  );

  Serial.print(
    " = "
  );

  Serial.println(
    newState
      ? "ON"
      : "OFF"
  );


  // ==========================================================
  // RECOVERY
  // ==========================================================

  if (
    gameState ==
    RECOVERY
  ) {

    if (
      boardMatchesBaseline()
    ) {

      Serial.println();

      Serial.println(
        "BOARD POSITION RESTORED"
      );

      Serial.println(
        "CHESSGRID READY"
      );


      clearMovement();
      clearModifier();


      gameState =
        READY;


      Serial.println();

      printTurn();
    }


    return;
  }


  // ==========================================================
  // WAITING
  // ==========================================================

  if (
    gameState ==
    WAITING_FOR_POSITION
  ) {

    return;
  }


  // ==========================================================
  // MOVEMENT START
  // ==========================================================

  if (
    !movementActive &&
    baselineState[index] == true &&
    oldState == true &&
    newState == false
  ) {

    movementActive =
      true;

    movementSource =
      index;

    firstDestination =
      -1;

    lastDisturbed =
      -1;

    gameState =
      MOVEMENT;


    Serial.println();

    Serial.println(
      "MOVEMENT STARTED"
    );

    Serial.print(
      "SOURCE LOCKED: "
    );

    Serial.println(
      squares[index]
    );
  }


  // ==========================================================
  // FIRST DESTINATION
  // ==========================================================

  if (
    movementActive &&
    firstDestination == -1 &&
    baselineState[index] == false &&
    oldState == false &&
    newState == true
  ) {

    firstDestination =
      index;


    Serial.print(
      "FIRST DESTINATION: "
    );

    Serial.println(
      squares[index]
    );
  }


  // ==========================================================
  // ADDITIONAL DISTURBANCE
  // ==========================================================

  if (
    movementActive &&
    baselineState[index] == true &&
    oldState == true &&
    newState == false
  ) {

    if (
      index != movementSource
    ) {

      lastDisturbed =
        index;


      Serial.print(
        "DISTURBANCE: "
      );

      Serial.println(
        squares[index]
      );
    }
  }


  // ==========================================================
  // MOVEMENT CANCELLED
  // ==========================================================

  if (
    movementActive &&
    boardMatchesBaseline()
  ) {

    Serial.println();

    Serial.println(
      "MOVEMENT CANCELLED"
    );

    Serial.println(
      "BOARD RETURNED TO BASELINE"
    );


    clearMovement();
    clearModifier();


    gameState =
      READY;


    Serial.println();

    printTurn();

    return;
  }


  // ==========================================================
  // MOVEMENT STATUS
  // ==========================================================

  if (
    movementActive
  ) {

    Serial.println(
      "MOVEMENT ACTIVE"
    );

    Serial.println(
      "Waiting for clock click..."
    );
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(1000);


  Serial.println();

  Serial.println(
    "=== CHESSGRID ==="
  );


  // ==========================================================
  // INITIAL SENSOR STATE
  // ==========================================================

  for (int i = 0; i < SENSOR_COUNT; i++) {

    sensorState[i] =
      false;

    baselineState[i] =
      false;
  }


  clearMovement();

  clearModifier();


  // ==========================================================
  // INITIAL SCAN
  // ==========================================================

  scanSensors();


  // ==========================================================
  // STARTING POSITION
  // ==========================================================

  checkStartingPosition();


  // ==========================================================
  // SAVE BASELINE
  // ==========================================================

  saveBaseline();

  clearMovement();
  clearModifier();


  // ==========================================================
  // WEBSERVER / AP
  // ==========================================================

  setupWebServer();


  // ==========================================================
  // STATUS
  // ==========================================================

  Serial.println();

  if (
    gameState ==
    READY
  ) {

    Serial.println(
      "CHESSGRID READY"
    );

    printTurn();

  } else {

    Serial.println(
      "CHESSGRID NOT READY"
    );

    Serial.println(
      "Place pieces in starting position."
    );
  }
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // ==========================================================
  // WEB SERVER
  // ==========================================================

  handleWebServer();


  // ==========================================================
  // START WEBSOCKET AFTER CONFIGURATION
  // ==========================================================

  if (
  WiFi.status() == WL_CONNECTED &&
  websocketHost.length() > 0 &&
  !webSocketStarted
  ) {

    Serial.println();

    Serial.println(
      "WiFi connected and WebSocket configured."
    );


    connectWebSocket();
  }


  // ==========================================================
  // WEBSOCKET
  // ==========================================================

  if (
    webSocketStarted
  ) {

    webSocket.loop();
  }


  // ==========================================================
  // SERIAL INPUT
  // ==========================================================

  if (Serial.available()) {

    String input =
      Serial.readStringUntil('\n');

    input.trim();


    if (
      input.length() == 0
    ) {

      return;
    }


    // ========================================================
    // PLACING PIECES
    // ========================================================

    if (
      input.equalsIgnoreCase(
        "placing pieces"
      )
    ) {

      if (
        gameState ==
        RECOVERY
      ) {

        Serial.println(
          "CANNOT PLACE PIECES DURING RECOVERY"
        );

        Serial.println(
          "Return board to previous position."
        );

        return;
      }


      Serial.println();

      Serial.println(
        "PLACING PIECES..."
      );


      for (int i = 0; i < SENSOR_COUNT; i++) {

        int rank =
          i / 8;


        if (
          rank == 0 ||
          rank == 1 ||
          rank == 6 ||
          rank == 7
        ) {

          sensorState[i] =
            true;

        } else {

          sensorState[i] =
            false;
        }
      }


      scanSensors();

      checkStartingPosition();


      saveBaseline();

      clearMovement();
      clearModifier();


      if (
        gameState ==
        READY
      ) {

        Serial.println();

        printTurn();
      }


      return;
    }


    // ========================================================
    // CLOCK CLICK
    // ========================================================

    if (
      input.equalsIgnoreCase(
        "click"
      )
    ) {

      clockClick();

      return;
    }


    // ========================================================
    // MODIFIER
    // ========================================================

    if (
      input.equalsIgnoreCase("n")
    ) {

      setModifier('n');

      return;
    }


    if (
      input.equalsIgnoreCase("b")
    ) {

      setModifier('b');

      return;
    }


    if (
      input.equalsIgnoreCase("r")
    ) {

      setModifier('r');

      return;
    }


    if (
      input.equalsIgnoreCase("q")
    ) {

      setModifier('q');

      return;
    }


    // ========================================================
    // RESET
    // ========================================================

    if (
      input.equalsIgnoreCase(
        "reset"
      )
    ) {

      board =
        Board();


      for (int i = 0; i < SENSOR_COUNT; i++) {

        sensorState[i] =
          false;

        baselineState[i] =
          false;
      }


      clearMovement();
      clearModifier();


      gameState =
        WAITING_FOR_POSITION;


      Serial.println();

      Serial.println(
        "BOARD RESET"
      );

      Serial.println(
        "CHESSGRID NOT READY"
      );

      Serial.println(
        "Place pieces in starting position."
      );


      if (
        webSocketStarted &&
        webSocket.isConnected()
      ) {

        webSocket.sendTXT(
          "RESET"
        );

      }


      return;
    }


    // ========================================================
    // SENSOR COMMAND
    // ========================================================

    int separator =
      input.indexOf('=');


    if (
      separator != -1
    ) {

      String square =
        input.substring(
          0,
          separator
        );

      String value =
        input.substring(
          separator + 1
        );


      square.trim();

      value.trim();


      int index =
        findSquareIndex(
          square
        );


      if (
        index == -1
      ) {

        Serial.println(
          "INVALID SQUARE"
        );

        return;
      }


      if (
        value.equalsIgnoreCase(
          "on"
        )
      ) {

        updateSensorState(
          index,
          true
        );
      }

      else if (
        value.equalsIgnoreCase(
          "off"
        )
      ) {

        updateSensorState(
          index,
          false
        );
      }

      else {

        Serial.println(
          "INVALID SENSOR STATE"
        );

        return;
      }


      return;
    }


    // ========================================================
    // NOT READY
    // ========================================================

    if (
      gameState != READY
    ) {

      if (
        gameState ==
        RECOVERY
      ) {

        Serial.println();

        Serial.println(
          "CHESSGRID IN RECOVERY"
        );

        Serial.println(
          "Return piece to original position."
        );

      }

      else if (
        gameState ==
        MOVEMENT
      ) {

        Serial.println();

        Serial.println(
          "MOVEMENT IN PROGRESS"
        );

        Serial.println(
          "Place piece and click clock."
        );

      }

      else {

        Serial.println();

        Serial.println(
          "CHESSGRID NOT READY"
        );

        Serial.println(
          "Place pieces in starting position."
        );
      }


      return;
    }


    // ========================================================
    // UNKNOWN COMMAND
    // ========================================================

    Serial.println();

    Serial.println(
      "Unknown command."
    );

    Serial.println(
      "Use:"
    );

    Serial.println(
      "  e2 = off"
    );

    Serial.println(
      "  e4 = on"
    );

    Serial.println(
      "  n / b / r / q"
    );

    Serial.println(
      "  click"
    );
  }
}