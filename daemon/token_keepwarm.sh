#!/bin/bash
# Clawdmeter token keep-warm.
#
# WHY: the usage daemon reads your Claude Code OAuth access token from the
# macOS Keychain ("Claude Code-credentials") each poll. That token has an ~8h
# TTL and is only refreshed when SOME Claude Code process makes an API call.
# During idle stretches it expires, the daemon gets HTTP 401, sends nothing,
# and the device shows no Claude data until the next Claude activity refreshes
# it (observed gaps of tens of minutes).
#
# WHAT: launchd runs this every 3 min. It only spends a real API call when the
# token is within ~6 min of expiry, and that call is a throwaway haiku prompt
# that loads NO MCP servers (--strict-mcp-config with an empty config) so it
# CANNOT start a second Telegram getUpdates consumer / conflict with the 5
# agent bots. The call makes Claude Code do its OWN native token refresh, which
# rewrites the shared Keychain entry safely. We never touch OAuth/Keychain
# ourselves (no refresh-token rotation risk).
#
# Logs: ~/Library/Logs/claude-token-keepwarm.log

# launchd (and cron) can start with an empty environment, so set HOME and PATH
# explicitly. If HOME is already set we keep it; otherwise edit YOUR_USERNAME to
# match your macOS account. PATH must include Homebrew (where `claude` lives).
export HOME="${HOME:-/Users/YOUR_USERNAME}"
export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin

LOG="$HOME/Library/Logs/claude-token-keepwarm.log"
TS() { date "+%Y-%m-%d %H:%M:%S"; }
REFRESH_IF_UNDER_SEC=360   # refresh when the token has < 6 min left (or expired)

EXP=$(security find-generic-password -s "Claude Code-credentials" -w 2>/dev/null \
  | python3 -c "import sys,json;d=json.load(sys.stdin);o=d.get('claudeAiOauth',d);print(int(o.get('expiresAt',0)))" 2>/dev/null)
NOW=$(python3 -c "import time;print(int(time.time()*1000))")

if [ -z "$EXP" ] || [ "$EXP" -eq 0 ]; then
  echo "$(TS) WARN: no expiresAt in Keychain (login missing?), skipping" >> "$LOG"
  exit 0
fi

LEFT=$(( (EXP - NOW) / 1000 ))   # seconds until expiry (negative = already expired)

if [ "$LEFT" -lt "$REFRESH_IF_UNDER_SEC" ]; then
  echo "$(TS) token ${LEFT}s left -> refreshing via native keep-warm" >> "$LOG"
  OUT=$(cd /tmp && claude -p "reply with only the word: ok" \
        --model claude-haiku-4-5 --strict-mcp-config --mcp-config '{"mcpServers":{}}' 2>&1 | tail -1)
  NEW=$(security find-generic-password -s "Claude Code-credentials" -w 2>/dev/null \
    | python3 -c "import sys,json,datetime;d=json.load(sys.stdin);o=d.get('claudeAiOauth',d);print(datetime.datetime.fromtimestamp(int(o.get('expiresAt',0))/1000))" 2>/dev/null)
  echo "$(TS) keep-warm reply='${OUT}' new_expiry='${NEW}'" >> "$LOG"
else
  echo "$(TS) token ${LEFT}s left, ok (no refresh needed)" >> "$LOG"
fi
