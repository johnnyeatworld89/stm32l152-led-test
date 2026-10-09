# Gitarren Audiorouter – Projektkontext für Claude Code

Frei routbarer Gitarren-Loop-Switcher (geplant 16–32 Loops, im Extremfall In, Out + 31 Loops).
Loops in beliebiger Reihenfolge und parallel routbar. Schaltmatrix-Hardware (z. B. ADG2188) existiert noch nicht.
Sprache mit dem Nutzer: Deutsch.

## Hardware / Build
- STM32L152RE Nucleo, HAL, CMake. Neue .c-Dateien in CMakeLists.txt eintragen.
- SPI1: Display ST7735 + MCP23S17 (Tasten Shift/Return/Menu). Dazu Drehencoder mit Taster.
- Eingabe: TIM6-Interrupt tastet Eingänge ab, Ring-Puffer (SPSC) -> Main-Loop.
- MIDI: USART1 PA9 (TX) / PA10 (RX), 31250 Bd (`MIDI_UART_PORT` in midi.h; 2 = PA2/PA3 kollidiert mit ST-LINK, SB13/SB14).
  Opto-Eingang H11L1, TRS Typ A.

## Dateien
- `ui.c/.h` – UI, Routing-Editor, Menüs (ROOT, GENERAL, SAVE_PRESET, SAVE_NAME, MANAGE_PRESETS, MANAGE_ACTIONS, DELETE_CONFIRM), Namens-Editor, Footer mit Bankanzeige.
- `preset_store.c/.h` – Data-EEPROM (0x08080000, 16 KB): max. 30 Records à 480 Byte (220 belegt, Rest reserviert für Schaltplan/Loop-Status), 64 Banknamen à 16 Byte, Gültig-Marker wird zuletzt geschrieben.
- `midi.c/.h` – UART-Treiber, Parser, schwache `MIDI_MessageReceived()`.
- `midi_presets.c/.h` – MIDI-Steuerung der Presets, `MidiPresets_Process()` in der Main-Loop.
- `font5x7.c` – Font: kein `.`-Glyph, Kleinbuchstaben werden groß dargestellt, unbekannte Zeichen = Kästchen.

## Presets
- Bänke 0–63, je 24 Presets (Anzeige "01: name"), insgesamt höchstens 30 im EEPROM.
- Layout-Blob: [Anzahl] + je Eintrag (id u8, order i8, lane i8). Auto-Nodes werden nicht gespeichert, beim Laden neu berechnet. `UI_PresetApplyLayout` validiert.
- Manage Presets: Rename bank, Load / Rename / Delete (Copy bewusst weggelassen, später).

## MIDI-Protokoll
- Empfang: Program Change = Bank (0–63); CC1–24 = Preset laden (Wert egal).
- CC0 = Speichermodus scharf (zweites CC0 bricht ab); nächstes CC1–24 speichert aktuelles Layout in aktuelle Bank/Preset. Endet auch beim Öffnen des Menüs. Konfigurierbar: `MIDI_PRESETS_CC_SAVE`.
- Senden nach Speichern/Laden (Menü oder MIDI): Program Change (Bank) + CC (Preset, Wert 127).
- Rückkopplungsschutz: bereits aktives Layout (`UI_PresetMatchesCurrent`) wird weder geladen noch gemeldet. MIDI wird bei offenem Menü ignoriert.

## Konventionen
- Zeilenenden der vorhandenen Dateien beibehalten (ui.c/ui.h/preset_store.*/midi_presets.* sind CRLF).
- Display: nur geänderte Zeichen neu zeichnen (Shadow-Cache); SPI-Zugriffe sparsam.
- Keine Blockierung in Interrupts; EEPROM-Schreiben blockiert kurz (bewusst).
- Host-Tests (gcc, Stub-HAL) sind möglich; Stand: Alles bisher nur teilweise auf dem Board geprüft (MIDI-Empfang/Senden bestätigt). MIDI-Speichern per CC0 ist nur am PC getestet.

## Offen / nächste Schritte
- Schaltplan + Loop-Status im Preset (Platz reserviert), ADG2188-Treiber / `routing.c`.
- Copy-Funktion in Manage Presets.
- MIDI-Empfang per DMA statt Byte-Interrupt.
- Externer Speicher für 64×24 Presets (EEPROM reicht nur für 30).
