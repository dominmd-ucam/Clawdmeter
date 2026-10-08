#pragma once
#include <Arduino.h>

struct UsageData {
    float session_pct;       // utilization 0-100 (5h window Pro/Max; spending % Enterprise)
    int session_reset_mins;  // minutes until reset
    float weekly_pct;        // 7-day utilization (Pro/Max only; 0 for Enterprise)
    int weekly_reset_mins;   // minutes until weekly reset (Pro/Max only)
    char status[16];         // "allowed", "limited", etc.
    bool chime;              // play the session-reset chime; false unless daemon opts in
    bool enterprise;         // true = Enterprise spending-limit account
    int time_pct;            // 0-100: fraction of billing period elapsed (Enterprise)
    int period_days;         // total billing period length in days (Enterprise)
    char reset_date[12];     // formatted reset date e.g. "Jul 1" (Enterprise)
    long clock_epoch;        // local wall-clock epoch (s) from daemon; 0 = not provided
    int  clock_fmt;          // 12 or 24 (hour format from daemon); defaults to 24
    // ---- Status screen (weather today / agents / weather tomorrow) ----
    int  weather_temp;       // London temperature °C (today); -999 = no data
    int  weather_cond;       // WMO weather code (today); -1 = no data
    bool agents[5];          // per-agent alive flags: general,etsy,upwork,appdev,heirpaws
    bool agents_present;     // true if the daemon sent the `ag` field
    bool agent_unknown[5];   // slot state unknown ('?' in ag): shown gray
    bool agent_busy[5];      // per-agent "actively working" flags (from `bz`)
    int  tomorrow_high;      // London tomorrow max temp °C; -999 = no data
    int  tomorrow_cond;      // WMO weather code (tomorrow); -1 = no data
    bool ok;                 // data parse succeeded
    bool valid;              // false until first successful parse
};

// Now-playing info for the Música screen (daemon reads Windows SMTC).
// Text is UTF-8; the daemon caps each field so the JSON fits one BLE write.
struct MusicData {
    bool active;             // something is playing/paused on the host
    char title[136];
    char artist[136];
    char album[136];
    bool playing;            // false = paused
    int  position;           // seconds, as of when this update landed
    int  duration;           // seconds; 0 = unknown
    uint16_t art_id;         // cover id; 0 = no cover
};
