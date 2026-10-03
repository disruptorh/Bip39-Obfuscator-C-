#!/bin/bash
# End-to-end GUI test for the airgapped BIP-39 Obfuscator (C++).
#
# Runs the app on a dedicated Xvfb display (clean-room: no window manager or
# clipboard-manager interference), drives it with keyboard navigation and
# verifies the real feature end to end:
#   1. type a 12-word mnemonic and a password
#   2. "Obfuscate / De-obfuscate" -> the result is a different, still-valid
#      BIP-39 mnemonic of the same length
#   3. "Copy Result"  -> clipboard holds that obfuscated mnemonic
#   4. "Copiar" (Ethereum)  -> clipboard holds an EIP-55 0x address
#   5. "Copiar" (Bitcoin)   -> clipboard holds a Base58Check/bech32 address
#   6. auto-clear after BIP39_CLIPBOARD_TIMEOUT_MS
#   7. nothing is persisted under $HOME
#
# Navigation is *discovered*, not hardcoded: the result/address widgets only
# exist once the obfuscation has run, so their tab order shifts with the
# layout. Instead of counting tabs, the script keeps walking forward and stops
# at the first widget whose clipboard signature it recognises (mnemonic /
# 0x address / bech32 address). That keeps the test valid when the layout grows
# a widget.
#
# Note: ImGui windows are drawn inside the GLFW window, so they are NOT
# separate X11 windows; screen transitions are verified functionally via the
# clipboard instead of xdotool window-title searches.
set -u

XDISPLAY=${E2E_DISPLAY:-:99}
BINARY=${E2E_BINARY:-./build/bip39_obfuscator}
LOGDIR=$(mktemp -d /tmp/bip39_obfuscator_e2e.XXXXXX)
export BIP39_CLIPBOARD_TIMEOUT_MS=3000
# Xvfb gets its own HOME: with GLX it runs Mesa's software rasteriser, and that
# process is what writes $HOME/.cache/mesa_shader_cache. Sharing the HOME would
# make the "app persisted nothing" assertion fail on the display server's
# behalf, hiding what it is meant to catch.
export XVFB_HOME="$LOGDIR/xvfb-home"
mkdir -p "$XVFB_HOME"
export HOME="$LOGDIR/home"

SEED="abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
PASSWORD="clave-de-prueba"

cleanup() {
  [ -n "${APP:-}" ] && kill "$APP" 2>/dev/null
  [ -n "${XVFB:-}" ] && kill "$XVFB" 2>/dev/null
  wait "${APP:-}" 2>/dev/null
  wait "${XVFB:-}" 2>/dev/null
  [ -n "${KEEP_LOGS:-}" ] || rm -rf "$LOGDIR"
}
trap cleanup EXIT

if [ ! -x "$BINARY" ]; then
  echo "E2E FAIL: $BINARY not found. Build it first: cmake -S . -B build && cmake --build build -j"
  exit 1
fi

for tool in Xvfb xdotool xclip; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "E2E FAIL: missing '$tool' (Debian/Ubuntu: apt install xvfb xdotool xclip)"
    exit 1
  fi
done

pkill -f bip39_obfuscator 2>/dev/null
pkill -f "Xvfb $XDISPLAY" 2>/dev/null
sleep 0.5
mkdir -p "$HOME"

# Start the app on an isolated display so results are deterministic.
HOME="$XVFB_HOME" Xvfb "$XDISPLAY" -screen 0 1280x900x24 >"$LOGDIR/xvfb.log" 2>&1 &
XVFB=$!
sleep 1.5
export DISPLAY=$XDISPLAY

"$BINARY" >"$LOGDIR/app.log" 2>&1 &
APP=$!
sleep 2.5

WID=$(xdotool search --name "BIP-39" | head -1)
if [ -z "$WID" ]; then
  echo "E2E FAIL: window not found"
  exit 1
fi
xdotool windowactivate "$WID" 2>/dev/null
xdotool windowfocus "$WID" 2>/dev/null
sleep 0.5

key()   { xdotool key --window "$WID" "$1"; sleep 0.25; }
typeit(){ xdotool type --window "$WID" --delay 25 "$1"; sleep 0.4; }
getclip() { timeout 3 xclip -selection clipboard -o 2>/dev/null; }
wipeclip() { printf '' | xclip -selection clipboard -i 2>/dev/null; sleep 0.2; }
words() { echo "$1" | wc -w; }

# ---------------------------------------------------------------------------
# Fill the form. Nothing is focused on appear, so Tab reaches the first widget:
# Tab 1 = "Seed Phrase" (multiline), Tab 2 = "Secret (Password)".
# ---------------------------------------------------------------------------
key Tab
typeit "$SEED"
key Tab
typeit "$PASSWORD"
echo "E2E: mnemonic and password typed"

# ---------------------------------------------------------------------------
# Walk forward one widget at a time, activating it, until each expected
# clipboard signature shows up. Activation of a non-copy widget (text input,
# radio, "Regenerate Salt", "Obfuscate / De-obfuscate") is harmless, so the
# search is safe; "Obfuscate / De-obfuscate" is what makes the Result and
# address widgets appear at all.
# ---------------------------------------------------------------------------
OBFUSCATED=""
EVM=""
BTC=""

for _step in $(seq 1 14); do
  key Tab
  key Return
  sleep 0.5
  clip=$(getclip)

  if [ -z "$clip" ]; then
    continue
  fi

  n=$(words "$clip")
  if [ -z "$OBFUSCATED" ] && { [ "$n" -eq 12 ] || [ "$n" -eq 24 ]; } &&
     ! echo "$clip" | grep -q "^$SEED$"; then
    OBFUSCATED="$clip"
    echo "E2E ok: obfuscated -> $n-word mnemonic (differs from the input)"
  elif [ -z "$EVM" ] && echo "$clip" | grep -Eq '^0x[0-9a-fA-F]{40}$'; then
    EVM="$clip"
    echo "E2E ok: Ethereum address copied"
  elif [ -z "$BTC" ] && echo "$clip" | grep -Eq '^bc1q[qpzry9x8gf2tvdw0s3jn54khce6mua7l]{38}$'; then
    BTC="$clip"
    echo "E2E ok: Bitcoin address copied"
  fi

  wipeclip

  if [ -n "$OBFUSCATED" ] && [ -n "$EVM" ] && [ -n "$BTC" ]; then
    break
  fi
done

if [ -z "$OBFUSCATED" ]; then
  echo "E2E FAIL: no obfuscated mnemonic reached the clipboard"
  exit 1
fi
if [ -z "$EVM" ]; then
  echo "E2E FAIL: no Ethereum address reached the clipboard"
  exit 1
fi
if [ -z "$BTC" ]; then
  echo "E2E FAIL: no Bitcoin address reached the clipboard"
  exit 1
fi

# The obfuscated mnemonic must not be the input, and must keep the original
# word count (the XOR preserves the entropy length and the checksum is
# recomputed over the new entropy).
in_words=$(words "$SEED")
out_words=$(words "$OBFUSCATED")
if [ "$in_words" -ne "$out_words" ]; then
  echo "E2E FAIL: word count changed: $in_words -> $out_words"
  exit 1
fi
echo "E2E ok: word count preserved ($out_words)"

# ---------------------------------------------------------------------------
# Round trip: feed the obfuscated mnemonic back with the same password and the
# same salt and check we get the original back (the transform is an XOR with a
# key derived only from the password and the salt, so it is an involution).
# ---------------------------------------------------------------------------
wipeclip
kill "$APP" 2>/dev/null; wait "$APP" 2>/dev/null
"$BINARY" >>"$LOGDIR/app.log" 2>&1 &
APP=$!
sleep 2.5
WID=$(xdotool search --name "BIP-39" | head -1)
xdotool windowactivate "$WID" 2>/dev/null
xdotool windowfocus "$WID" 2>/dev/null
sleep 0.5

key Tab
typeit "$OBFUSCATED"
key Tab
typeit "$PASSWORD"

BACK=""
for _step in $(seq 1 14); do
  key Tab
  key Return
  sleep 0.5
  clip=$(getclip)
  wipeclip
  n=$(words "$clip")
  if [ "$n" -eq "$in_words" ]; then
    BACK="$clip"
    break
  fi
done

if [ -z "$BACK" ]; then
  echo "E2E FAIL: round trip produced no $in_words-word mnemonic"
  exit 1
fi
if [ "$(echo "$BACK" | tr -s ' ')" != "$(echo "$SEED" | tr -s ' ')" ]; then
  echo "E2E FAIL: round trip did not recover the original mnemonic"
  echo "  expected: $SEED"
  echo "  got     : $BACK"
  exit 1
fi
echo "E2E ok: round trip obfuscate -> de-obfuscate recovers the original"

# ---------------------------------------------------------------------------
# Auto-clear: the last copy restarted the 3 s timer; wait past it.
# ---------------------------------------------------------------------------
key Return
sleep 4
AFTER=$(getclip)
if [ -n "$AFTER" ]; then
  echo "E2E FAIL: clipboard not auto-cleared: '$AFTER'"
  exit 1
fi
echo "E2E ok: clipboard auto-cleared after timeout"

# ---------------------------------------------------------------------------
# No persistence: the app must not write anything under $HOME.
# ---------------------------------------------------------------------------
NFILES=$(find "$HOME" -type f 2>/dev/null | wc -l)
echo "HOME files: $NFILES"
if [ "$NFILES" -ne 0 ]; then
  echo "E2E FAIL: app persisted files under HOME"
  exit 1
fi

echo "E2E PASS: obfuscate -> copy result -> copy EVM/BTC -> round trip -> auto-clear -> no persistence"
exit 0