/* SeriousBridge core - a speedrun timer for Serious Sam, and LiveSplit kept in step with it.
 *
 * The game's own level times don't serve runners; this one counts:
 *   RTA - real time (the PC's clock) from the start, loads, menus and pauses included;
 *   IGT - the game's time: its ticks (1/20 s each) while the run goes - nothing while a level loads, the game is paused
 *         or a menu is up, and a loaded save doesn't jump it.
 * A run starts when a new game is started and the player first has control (no intro, no Netricsa; sr_iStartOn 0: on
 * the new game itself), splits at the end of every level (when the game records the level's stats; or when the level
 * changes without that), and ends with the game's end. Starting a new game resets it.
 *
 * LiveSplit: with its server running (LiveSplit -> Control -> Start TCP Server, port 16834; or the LiveSplit Server
 * component) the timer starts, splits, ends and resets it, and keeps its Game Time equal to the IGT (compare against
 * Game Time to run by IGT). The connection is tried every few seconds; without LiveSplit the timer works on its own.
 *
 * Console (persistent): sr_bShowTimer, sr_bLiveSplit, sr_strLiveSplit ("127.0.0.1:16834"), sr_iStartOn.
 */
#ifndef SBRIDGE_SPEEDRUN_H
#define SBRIDGE_SPEEDRUN_H

#include "StdH.h"

namespace speedrun {

// Plugin startup / shutdown (the LiveSplit thread).
void Init(void);
void Shutdown(void);
// Events: OnGameStart (a new game), OnChangeLevel, OnGameLoad (a save loaded).
void OnGameStart(void);
void OnChangeLevel(void);
void OnGameLoad(void);
// Every tick (OnStep), the local player (NULL between levels).
void Step(CEntity *penPlayer);
// Every frame (OnFrame).
void Frame(void);
// After the game is drawn: RTA and IGT in the top right corner (sr_bShowTimer).
void Draw(CDrawPort *pdp);

enum RunState { RS_IDLE, RS_ARMED, RS_RUNNING, RS_DONE };
RunState State(void);
DOUBLE RealTime(void); // s
DOUBLE GameTime(void); // s
INDEX Splits(void);
BOOL LiveSplitConnected(void);
CTString Status(void);
// "1:02:03.45"
CTString FormatTime(DOUBLE tm);

}; // namespace

#endif
