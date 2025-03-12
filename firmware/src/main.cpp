#include <Arduino.h>

/* =================================================================================== 
*  >> Firmware für Ringspiel <<
*  Benutzte Hardware:
*    - Arduino Nano
*    - Mosfets https://www.infineon.com/cms/en/product/power/mosfet/n-channel/irfz44n/
*    - Zwischen den Haken und den Input Pins hängt ein RC-Tiefpass (+- 100Hz)
*  
*  Softwarestruktur:
*    - Zustandsautomat basierend auf STATE_t
*    - Zustände: Waiting for Game > Spiel kann über 'restart_btn' gestartet werden. Animation läuft auf Dauerschleife.
*                Game Running     > Spiel kann mit 'restart_btn' neugestartet werden. Wenn abs(spielstand) >= 3 wird zu Winner Display gewechselt.
*                Winner Display   > Spiel kann auch hier neugestartet werden. Siegeranimation läuft auf Dauerschleife.
*    - Animationen laufen meist nicht-blockierend in Abhängigkeit zum Zustand
*
*  Ein original Brainchild von
*  @author Aleksander Stepien (c) 2023
*
*  Lizenziert unter GNU GPL-3.0  
* =================================================================================== */

/*********************************************/
/* Pinout für Arduino Nano und Hardware Defs */
/*********************************************/

/*
 *              Lampe1          Lampe 2         Lampe3          Lampe4          Lampe5
 *      --------------------------------------------------------------------------------------
 *           |                                    ||                                    |
 *           |                                    ||                                    |
 *           |                                    ||                                    |
 *           |                                    ||                                    |
 *           o                                    ||                                    o
 *        Spieler1                                ||                                 Spieler2
 *                                              ,_||_,
 *                                                ||
 *                          ----------------------------------------------
 */

#define lamp1 11
#define lamp2 10
#define lamp3 9
#define lamp4 8
#define lamp5 7
#define hook1 12
#define hook2 2
#define restart_btn 5
#define player1_btn 4
#define player2_btn 3
#define DEBOUNCE_TIME 300            // in ms
#define HOOK_DEBOUNCE_TIME 100       // in ms => Software Tiefpass auf 10Hz
#define HOOK_RESET_DEBOUNCE_TIME 500 // in ms

/*********************************************/
/*  ENUM                                     */
/*********************************************/

typedef enum STATE
{
    WAITING_FOR_GAME = 1,
    GAME_RUNNING = 2,
    WINNER_DISPLAY = 3
} STATE_t;

// Ein Byte durch das Lichter-Hardware abstrahiert wird.
typedef enum CHANDELIER
{
    D_LAMPS_OFF = 0,
    D_LAMP1 = 0x1,
    D_LAMP2 = 0x2,
    D_LAMP3 = 0x4,
    D_LAMP4 = 0x8,
    D_LAMP5 = 0x10,
    D_LAMPS_P1 = B00000011,
    D_LAMPS_P2 = B00011000,
    D_LAMPS_ALL = B00011111,
} CHANDELIER_t;

typedef enum PLAYER
{
    PLAYER_1 = 0,
    PLAYER_2 = 1
} PLAYER_t;

/*********************************************/
/*  Globale Variablen                         */
/*********************************************/

STATE_t state = WAITING_FOR_GAME; // Status des Spiels {1: kein laufendes Spiel, warten auf restart, Leuchtanimation
                                  //                    2: laufendes Spiel, Spielstand wird angezeigt, aknn auch manuell nachgestellt werden
                                  //                    3: Spiel wurde beendet, Gewinner wird angezeigt, nach 2 min in State 1 wechseln}
signed int spielstand = 0;        // Start-Spielstand:  Lampe in der Mitte leuchtet

/*********************************************/
/*  HELPER                                   */
/*********************************************/

/// @brief Schaltet lampen entsprechend der Binärdarstellung.
/// @param display Binärdarstellung der Lampen. 0 | 0 | 0 | lamp5 | lamp4 | lamp3 | lamp2 | lamp1
void writeToChandelier(CHANDELIER_t display)
{
    digitalWrite(lamp1, display & 0x1);
    digitalWrite(lamp2, (display & 0x2) >> 1);
    digitalWrite(lamp3, (display & 0x4) >> 2);
    digitalWrite(lamp4, (display & 0x8) >> 3);
    digitalWrite(lamp5, (display & 0x10) >> 4);
}

inline bool pressed(int pin)
{
    // Pullup => HIGH wenn nicht gedrückt
    return !(digitalRead(pin) == HIGH);
}

bool isP1Winner()
{
    return spielstand == -3;
}
bool isP2Winner()
{
    return spielstand == 3;
}

void addPointFor(PLAYER_t player)
{
    if (player == PLAYER_1)
        spielstand--;
    else
        spielstand++;

    if (spielstand > 3)
        spielstand = 3;
    if (spielstand < -3)
        spielstand = -3;
}

void removePointFor(PLAYER_t player)
{
    PLAYER_t spielerinverses = ((PLAYER_t)((((uint8_t)player) + 1) % 2)); // Addiere 1 auf und overflowe.
    addPointFor(spielerinverses);
}

/*********************************************/
/*  SEQUENZEN / ANZEIGE                      */
/*********************************************/

#define SEQUENCE_DELTA_RUNNINGLIGHT 500
#define SEQUENCE_COUNT_RUNNINGLIGHT 5

CHANDELIER_t sequenceRunningLight[] = {D_LAMP1, D_LAMP2, D_LAMP3, D_LAMP4, D_LAMP5};
uint8_t runningLightSequenceNumber = 0; // Position der leuchtenden Lampe

int runningLightSequenceDirection = 1;     // 1: von Spieler 1 nach Spieler 2; -1: von Spieler 2 nach Spieler 1
unsigned long runningLightTime = millis(); // Zeitvariable zum Wechseln der Lampen in State 1

// Sequenz läuft "asynchron"
void lampsRunningLight()
{
    if (runningLightTime + SEQUENCE_DELTA_RUNNINGLIGHT < millis())
    {
        runningLightTime = millis();
        // Sache anzeigen
        CHANDELIER_t scene = sequenceRunningLight[runningLightSequenceNumber];
        writeToChandelier(scene);
        // Vektor hochzählen oder überlaufen
        runningLightSequenceNumber += runningLightSequenceDirection;
        // Ändern der Richtung
        if (runningLightSequenceNumber >= SEQUENCE_COUNT_RUNNINGLIGHT - 1 || runningLightSequenceNumber <= 0)
            runningLightSequenceDirection = -1 * runningLightSequenceDirection; // Invertieren
    }
}

#define SEQUENCE_COUNT_NEWGAME 9
#define SEQUENCE_DELTA_NEWGAME 250 // in ms

// Diese Sequenz läuft synchron
void lampsNewGame()
{
    CHANDELIER_t sequenceNewGame[] = {D_LAMPS_OFF, D_LAMPS_ALL, D_LAMPS_OFF, D_LAMPS_ALL, D_LAMPS_OFF, D_LAMPS_ALL, D_LAMPS_OFF, D_LAMPS_ALL, D_LAMP3};
    uint8_t newGameSequenceNumber = 0; // Sequenz für neues Spiel
    while (newGameSequenceNumber < SEQUENCE_COUNT_NEWGAME)
    {
        CHANDELIER_t scene = sequenceNewGame[newGameSequenceNumber];
        writeToChandelier(scene);
        newGameSequenceNumber++;
        delay(SEQUENCE_DELTA_NEWGAME);
    }
}

void lampsSpielstand()
{
    switch (spielstand)
    {
    case -2:
        writeToChandelier(D_LAMP1);
        break;
    case -1:
        writeToChandelier(D_LAMP2);
        break;
    case 0:
        writeToChandelier(D_LAMP3);
        break;
    case 1:
        writeToChandelier(D_LAMP4);
        break;
    case 2:
        writeToChandelier(D_LAMP5);
        break;
    default:
        break;
    };
}

// SEQUENCE_COUNT_WINNER bestimmt, wieviele Einträge des Arrays benutzt werden.
#define SEQUENCE_COUNT_WINNER 16
#define SEQUENCE_DELTA_WINNER 150 // in ms

//
CHANDELIER_t sequenceWinnerP1[] = {D_LAMPS_OFF, D_LAMP5, D_LAMP4, D_LAMP3, D_LAMP2, D_LAMP1, D_LAMP5, D_LAMP4, D_LAMP3, D_LAMP2, D_LAMP1, D_LAMP5, D_LAMP4, D_LAMP3, D_LAMP2, D_LAMP1};
CHANDELIER_t sequenceWinner2P1[] = {D_LAMPS_P2, D_LAMPS_P2, D_LAMPS_P2, D_LAMPS_P2, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_P2, D_LAMPS_P2, D_LAMPS_P2, D_LAMPS_P2, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF};
CHANDELIER_t sequenceWinnerP2[] = {D_LAMPS_OFF, D_LAMP1, D_LAMP2, D_LAMP3, D_LAMP4, D_LAMP5, D_LAMP1, D_LAMP2, D_LAMP3, D_LAMP4, D_LAMP5, D_LAMP1, D_LAMP2, D_LAMP3, D_LAMP4, D_LAMP5};
CHANDELIER_t sequenceWinner2P2[] = {D_LAMPS_P1, D_LAMPS_P1, D_LAMPS_P1, D_LAMPS_P1, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_P1, D_LAMPS_P1, D_LAMPS_P1, D_LAMPS_P1, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF, D_LAMPS_OFF};

unsigned long winnerAnimationLightTime = millis(); // Animationslaufvariable für Gewinneranzeige
uint8_t winnerAnimationSequenceNumber = 0;         // Binärdarstellung von Lampe
bool sequenceStepWinner = 0;                       // 0: pfeil Animation, 1: dauerblinken
// Diese Sequenz läuft "asynchron"
void lampsWinner()
{
    if (winnerAnimationLightTime + SEQUENCE_DELTA_WINNER < millis())
    {
        winnerAnimationLightTime = millis();
        // Sache anzeigen
        CHANDELIER_t scene = (isP1Winner() ? (sequenceStepWinner == 1 ? sequenceWinner2P1[winnerAnimationSequenceNumber]
                                                                      : sequenceWinnerP1[winnerAnimationSequenceNumber])
                                           : (sequenceStepWinner == 1 ? sequenceWinner2P2[winnerAnimationSequenceNumber]
                                                                      : sequenceWinnerP2[winnerAnimationSequenceNumber]));
        writeToChandelier(scene);
        // State hochzählen oder überlaufen
        winnerAnimationSequenceNumber++;
        if (winnerAnimationSequenceNumber >= SEQUENCE_COUNT_WINNER)
        {
            sequenceStepWinner = 1;
        }
        winnerAnimationSequenceNumber %= SEQUENCE_COUNT_WINNER; // This is dirty but who cares now. Im sitting in the car towards Toms party lol.
    }
}

/*********************************************/
/* Spielmechanik                             */
/*********************************************/

#define WINNER_LIGHT_TIME 15000       // in ms => Dauer der Siegesanimation
unsigned long winnerLightTimeMillis = millis(); // Zeitvariable zum Wechseln von Gewinneranzeige auf runningLight (wie lange maximal laufend?)

void evaluateAndDisplayWinner()
{
    if (winnerLightTimeMillis + WINNER_LIGHT_TIME > millis())
    {
        // Zeit noch nicht ausgelaufen.
        lampsWinner();
    }
    else
    {
        // Animation is ausgelaufen gehe zurück zum Init Zustand.
        Serial.println("RESET");
        runningLightSequenceNumber = 0;
        runningLightSequenceDirection = 1;
        state = WAITING_FOR_GAME;
    }
}

unsigned long hookPressedTime[] = {0, 0};
unsigned long hookDepressedTime[] = {0, 0};
bool lastHookState[] = {false, false};
bool hookRegistered[] = {true, true};

void checkHook(PLAYER_t player)
{
    int pinToCheck = (player == PLAYER_1 ? hook1 : hook2);
    int i = (int)player;
    if (pressed(pinToCheck))
    {
        if (!lastHookState[i])
        {
            // Vorher noch nicht gedrückt
            hookPressedTime[i] = millis();
            lastHookState[i] = true;
        }
        if (!hookRegistered[i] && hookPressedTime[i] + HOOK_DEBOUNCE_TIME < millis())
        {
            // Haken lang genug eingehängt
            hookPressedTime[i] = millis();
            if (i == 0)
                addPointFor(PLAYER_1);
            else
                addPointFor(PLAYER_2);
            hookRegistered[i] = true;
        }
    }
    else
    {
        if (lastHookState[i])
        {
            // Vorher gedrückt
            lastHookState[i] = false;
            hookDepressedTime[i] = millis();
        }
        if (hookRegistered[i] && hookDepressedTime[i] + HOOK_RESET_DEBOUNCE_TIME < millis())
        {
            // Der haken wurde registriert.
            // Warte HOOK_RESET_DEBOUNCE_TIME und setze zurück
            hookDepressedTime[i] = millis();
            hookRegistered[i] = false;
        }
    }
}

// Evaluiere und wechsele Zustände basierend auf Inputs
void evaluateButtonsAndHooks()
{
    if (pressed(restart_btn))
    {
        lampsNewGame(); // Zeige kurze Feedback-Animation an
        spielstand = 0;
        state = GAME_RUNNING;
        delay(DEBOUNCE_TIME);
        while (pressed(restart_btn))
            delay(10);
    }
    if (state == GAME_RUNNING)
    {
        // Punktezählung
        checkHook(PLAYER_1);
        checkHook(PLAYER_2);
    }
    if (state == GAME_RUNNING || state == WINNER_DISPLAY)
    {
        if (pressed(player1_btn))
        {
            addPointFor(PLAYER_1);
            if (state == WINNER_DISPLAY)
            {
                state = GAME_RUNNING;
                writeToChandelier(D_LAMP1);
            }
            delay(DEBOUNCE_TIME);
            while (pressed(player1_btn))
                delay(10);
        }
        if (pressed(player2_btn))
        {
            addPointFor(PLAYER_2);
            if (state == WINNER_DISPLAY)
            {
                state = GAME_RUNNING;
                writeToChandelier(D_LAMP5);
            }
            delay(DEBOUNCE_TIME);
            while (pressed(player2_btn))
                delay(10);
        }
    }
}

/*********************************************/
/* Setup / Loop                              */
/*********************************************/
void setup()
{
    Serial.begin(9600);
    pinMode(lamp1, OUTPUT);
    pinMode(lamp2, OUTPUT);
    pinMode(lamp3, OUTPUT);
    pinMode(lamp4, OUTPUT);
    pinMode(lamp5, OUTPUT);

    pinMode(restart_btn, INPUT_PULLUP); // restart-Button für Neustart oder Reset des Spiels
    pinMode(player1_btn, INPUT_PULLUP); // Haken-Pin für Spieler 1
    pinMode(player2_btn, INPUT_PULLUP); // Haken-Pin für Spieler 2
}

void loop()
{
    // Auswerten von Änderungsbedingungen
    evaluateButtonsAndHooks();
    // checken, ob es einen Gewinner gibt.
    if (abs(spielstand) >= 3 && state == GAME_RUNNING)
    {
        winnerAnimationSequenceNumber = 0;
        sequenceStepWinner = 0;
        winnerLightTimeMillis = millis(); // Initialisiere maximale Auslaufzeit
        state = WINNER_DISPLAY;
        Serial.println("INIT WINNING SEQ");
    }
    // Zustandsbasierte Outputs
    switch (state)
    {
    case WAITING_FOR_GAME:
        lampsRunningLight();
        break;
    case GAME_RUNNING:
        lampsSpielstand();
        break;
    case WINNER_DISPLAY:
        evaluateAndDisplayWinner();
        break;
    default:
        break;
    };
}
