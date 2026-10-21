# Gitarren Audiorouter – Projektkontext für Claude Code

Frei routbarer Gitarren-Loop-Switcher (geplant 16–32 Loops, im Extremfall In, Out + 31 Loops).
Loops in beliebiger Reihenfolge und parallel routbar. Schaltmatrix-Hardware (z. B. ADG2188) existiert noch nicht.
Sprache mit dem Nutzer: Deutsch.

## Hardware / Build
- STM32L152RE Nucleo, HAL, CMake. Neue .c-Dateien in CMakeLists.txt eintragen.
- SPI1 (8 MHz, Prescaler 4): Display ST7735 + MCP23S17 (Tasten Shift GPB7/Return GPB6/Menu GPA0, Loop-1-Fußschalter GPB5). Dazu Drehencoder mit Taster.
- Status-LED Loop 1: WS2812B an PB15 (SPI2_MOSI, Morpho CN10-26), SPI2 allein mit 4 MHz, 4 SPI-Bits je LED-Bit, Treiber `Libraries/WS2812B`. Aus / gelb = aktiv unbestätigt / grün = bestätigt, alle 100 ms neu gesendet. Pegelwandler 3,3 → 5 V empfohlen.
- Eingabe: TIM6-Interrupt tastet Eingänge ab, Ring-Puffer (SPSC) -> Main-Loop.
- MIDI: USART1 PA9 (TX) / PA10 (RX), 31250 Bd (`MIDI_UART_PORT` in midi.h; 2 = PA2/PA3 kollidiert mit ST-LINK, SB13/SB14).
  Opto-Eingang H11L1, TRS Typ A.

## Dateien
- `ui.c/.h` – UI, Routing-Editor, Menüs (ROOT, GENERAL, SAVE_PRESET, SAVE_NAME, MANAGE_PRESETS, MANAGE_ACTIONS, PRESET_MIDI, DELETE_CONFIRM, LOOP_MIDI, LOOP_MIDI_GROUP, CONVERT_CONFIRM, MERGE_CONFIRM, MERGE_ERROR, MERGE_REPORT, PRESET_SELECT), Namens-Editor, Footer mit Bankanzeige. Loop-Status zentral über `UI_LoopSetActive`.
- `preset_store.c/.h` – Data-EEPROM (0x08080000, 16 KB): 64 Banknamen à 16 Byte, max. 30 Records à 480 Byte (380 belegt: Loop-Status-Bitmap an Offset 216, Loop-Programme an 220 = Marker + 224 = 32 Byte, zusätzliche MIDI-Meldungen an 256 = Marker/Anzahl + 260 = 16 × 3 Byte, On/Bypass-Zustände an 308 = Marker + 312 = Loop-Maske + 316 = max. 16 × 4 Byte; Rest 100 Byte reserviert für Schaltplan), danach Config-Block (0x3C40, 8 Byte Header + 952 Byte, Loop-MIDI). Gültig-Marker wird zuletzt geschrieben.
- `midi.c/.h` – UART-Treiber, Parser, schwache `MIDI_MessageReceived()`. Bytes mit Frame-/Noise-/Parity-Fehler werden verworfen (Parser-Reset), `MIDI_Poll()` in der Main-Loop startet den Empfang neu, falls das Re-Arm im Interrupt an der HAL-Sperre von `HAL_UART_Transmit` scheitert. Zähler `debugMidiRx*`.
- `midi_presets.c/.h` – MIDI-Steuerung der Presets, `MidiPresets_Process()` in der Main-Loop. Queue enthält rohe CCs; Zuordnung Loop/Preset erst in der Main-Loop.
- `loop_midi.c/.h` – MIDI-Steuerung der Effektgeräte in den Loops: Konfiguration (Kanal, On/Bypass-Gruppen), Gruppenlogik, Senden, Echo-Filter, EEPROM-Serialisierung.
- `font5x7.c` – Font: kein `.`-Glyph (`,` und `+` vorhanden, Haken = `FONT5X7_CHAR_CHECK` '\x01'), Kleinbuchstaben werden groß dargestellt, unbekannte Zeichen = Kästchen.

## Presets
- Bänke 0–63, je 24 Presets (Anzeige "01: name"), insgesamt höchstens 30 im EEPROM.
- Layout-Blob: [Anzahl] + je Eintrag (id u8, order i8, lane i8). Auto-Nodes werden nicht gespeichert, beim Laden neu berechnet. `UI_PresetApplyLayout` validiert.
- Manage Presets: Rename bank, Load / Rename / Midi / Delete (Copy bewusst weggelassen, später).
- Preset-Midi (Aktion "Midi", Präfix `UI_Pm`): Liste aller Meldungen, die das Preset beim Laden sendet (außer Bank-PC/Preset-CC): zuerst je Kanal das Loop-Programm ("Ch2 PC5 L01"), dann zusätzliche Meldungen ("Ch1 CC20, 127", "Ch3 PC7"), "add message". Enter: Kanal -> Typ (CC/PC/empty = löschen) -> Nummer -> Wert (nur CC); MIDI Learn mit Kette wie bei Loop-MIDI. PC auf dem Kanal eines Loops mit Program Change wird als Loop-Programm gespeichert, sonst als zusätzliche Meldung (max. 16, `PRESET_MIDI_MAX_MESSAGES`). Gespeichert beim Verlassen (auch Menü-Taste) mit `PresetStore_WriteMidi`. Überschreiben per Save Preset/CC0 behält die zusätzlichen Meldungen.
- On/Bypass-Zustand je Preset (`PresetLoopStates`): für Loops im Layout mit Gruppen (max. 16) je Gruppe 4 Bit (0 = aus, k = Schalter k an). Gespeichert nur, wenn er zum Loop-Status passt; sonst / bei alten Presets wird wie bisher über den Loop-Status geschaltet. Laden: `LoopMidi_ApplyGroupState` sendet nur Gruppen, die vom bekannten Zustand abweichen (On des Zielschalters bzw. Off des aktiven), danach `UI_LoopSetActiveQuiet`. Vergleich in `UI_PresetMatchesCurrent` über `UI_LoopStatesMatch`.
- Preset-Midi zeigt nach "add message" je Gruppe "L01 On/Bypass 1: On 2" (Off / On / On k), Enter-Drehen-Enter. Ab der ersten Änderung wird der Zustand des ganzen Loops gespeichert, Loop-Status passend dazu.
- Laden sendet: Bank-PC + Preset-CC, Loop-Programme, zusätzliche Meldungen (über Echo-Filter, `LoopMidi_SendControlChange` / `LoopMidi_SendChannelProgram`), dann On/Bypass.

## Loop-MIDI (Effektgeräte)
- Loop-Menü (ROOT bei Loop-Item): Zeile "Midi" -> Seite "Midi Channel: n", "Program Change: Off/On", "On/Bypass n", "add On/Bypass".
- Program Change (je Loop, Bit 7 des Kanal-Bytes im Config-Block): PCs auf dem Loop-Kanal = aktuelles Programm des Geräts (Vorrang vor Bank-PC, Echo-Filter wie bei CCs). Presets speichern je Loop `0x80 | Programm` (0 = keins, alte Presets ohne Marker = keins); beim Laden zuerst PCs, dann On/Bypass. Unterschiedliches Programm verhindert den Rückkopplungsschutz-Treffer (`UI_PresetMatchesCurrent`).
- Je Loop max. 8 Gruppen à max. 8 Schalter; Schalter = 2 CC-Meldungen (On/Off, gleich = Toggle). In einer Gruppe max. ein Schalter an, Loop aktiv solange eine Gruppe an.
- Empfang: On -> Gruppe an; Off nur wirksam, wenn dieser Schalter an war; gleicher Toggle zweimal -> Gruppe aus. Vom Gerät gemeldet = `ACTIVE_CONFIRMED`.
- Senden nur bei Zustandswechsel durch den Router (Preset laden): an = letzte Schalter der zuletzt aktiven Gruppen (sonst Gruppe 1 Schalter 1), aus = Off der aktiven Schalter.
- Eingabe per Encoder (CC, dann Wert; "empty" löscht den Schalter) oder MIDI Learn (Kette: nach gelernter Meldung wartet die nächste "empty"-Zeile). Einzelne Meldung ohne Off wird beim Verlassen zum Toggle. Gespeichert wird beim Verlassen der Midi-Seite (auch per Menü-Taste).
- Loop-CCs haben Vorrang vor Preset-CCs (gleicher Kanal). Echo-Filter: eigene Loop-Meldungen und zusätzliche Preset-Meldungen innerhalb 200 ms werden ignoriert (64 Einträge). Loop-MIDI wird auch bei offenem Menü verarbeitet (ohne Neuzeichnen).
- Presets speichern den Loop-Status (Bit n = n-ter Loop in `uiItems`, max. 31, Bit 31 = gültig). Alte Presets (0) lassen die Loops unverändert.

## Convert to In + Out / Merge to Loop
- Loop-Menü (4. Zeile, Menü-Overlay jetzt 4 Zeilen hoch): "Convert to In + Out" -> "Convert presets too?" Yes / No / Select (Vorauswahl No, Return bricht ab).
- Je Loop feste Ersatz-Items (`UI_CONVERT_ITEMS`, id = Basis + Loopnummer): manueller Knoten "L01"/"Node L01" (104+n), Input "RL01"/"Return L01" (136+n), Output "SL01"/"Send L01" (168+n). Für weitere Loops dort ergänzen. Nicht aus dem normalen Knoten-Pool, nicht Pflicht im Preset-Layout.
- Umwandeln: Knoten an die Stelle des Loops, Return in die linke Spalte unter den Input mit der niedrigsten Lane, Send in die rechte Spalte unter den Output mit der niedrigsten Lane; Loop wird ausgeschaltet (Off an das Gerät) und inaktiv. Danach normales Auto-Routing (die neuen In/Out werden wie jedes Item verbunden).
- Ins/Outs sind horizontal verschiebbar. Randregel (`UI_ValidateEdgeRules`): ganz links nur und mindestens ein In, ganz rechts nur und mindestens ein Out; weitere Ins/Outs dürfen auch innen liegen. In am linken / Out am rechten Rand kann nicht weiter nach außen. Auf ein belegtes Feld: Tausch; landet dadurch ein anderes Item in einer Randspalte, greift wie bei Loops "Item betritt die Randspalte" (`UI_EnsureEdgeColumns` schiebt den Rand nach außen). `UI_EnsureEdgeColumns` verschiebt nur die Ins/Outs der Randspalten, innere bleiben (bzw. wandern mit).
- Vertikal (`UI_LaneWithinNeighbourRange`): auf ein freies Feld höchstens eine Lane über/unter die höchste/niedrigste Lane der permanenten Items in den nächsten Spalten links/rechts mit permanenten Items (Spalten nur mit Auto-Nodes zählen nicht). Belegtes Feld: Regel 2.6 (rechts daneben).
- Routing: Ist das nächste Ziel in derselben Lane, aber die Lane davor blockiert (innerer In/Out), versucht der Auto-Node auch die Nachbar-Lanes (`sideDirection`), sonst konvergierte das Routing nicht.
- Merge to Loop: In-/Out-Menü von "Return Lnn"/"Send Lnn" (3. Zeile, nur wenn der Loop nicht im Layout ist) -> "Merge presets too?" Yes / No / Select. Vorher `UI_MergeCheck`: ohne Return/Send muss ganz links nur und mindestens ein In, ganz rechts nur und mindestens ein Out bleiben, sonst Seite MERGE_ERROR ("Merge not possible:" / "Leftmost column needs an In" bzw. "Rightmost column needs an Out", OK). Loop nimmt den Platz des Knotens "Lnn" ein; ohne Knoten zweite Spalte von rechts, Lane 0 (belegt oder nur Randspalten: neue Spalte vor der Out-Spalte). Loop bleibt aus. "Yes" baut Presets über `UI_ConvertPresets(ids, 1)` zurück, Presets, in denen die Randregel verletzt würde, bleiben unverändert und werden danach auf der Seite MERGE_REPORT aufgelistet ("Not merged (In/Out):", je Zeile "B<Bank> <Preset>: Name", scrollbar, Enter/Return schließt das Menü).
- Select (Seite PRESET_SELECT, Liste von Manage Presets wiederverwendet über `UI_ListIsManage`/`UI_ListIsSelect`): Zeile 0 "Confirm", Bank-Zeile, nur belegte Presets mit Haken (Enter schaltet; `UI_ListSlotOfRow` ordnet Zeile -> Preset zu), leere Bänke werden beim Öffnen und in der Bankauswahl übersprungen, Status "Selected n". Auswahl `uiPresetSelection[bank]` (Bit = Preset), bei jedem Öffnen leer. Confirm baut nur die markierten Presets um (`UI_PRESETS_SELECTED`), Return zurück zur Abfrage, Menü-Taste bricht ab; die vorher gewählte Bank wird wiederhergestellt.
- Zustand steht nur im Layout: alte Presets bringen den Loop zurück. "Yes" baut alle Presets um (Layout wird in uiItems hergestellt, umgewandelt, neu berechnet und serialisiert, danach aktuelles Layout wiederhergestellt). Loop-Status und On/Bypass des Loops bleiben im Preset gespeichert (beim Laden ignoriert, solange der Loop nicht im Layout ist; `UI_PresetMatchesCurrent` vergleicht nur Loops im Layout), nach Merge schaltet das Preset den Loop wieder wie vorher. Mit gesplittetem Loop gespeicherte Presets haben ihn als aus. Presets ohne den Loop bekommen nur Return/Send.

## MIDI-Protokoll
- Empfang: Program Change = Bank (0–63); CC1–24 = Preset laden (Wert egal).
- CC0 = Speichermodus scharf (zweites CC0 bricht ab); nächstes CC1–24 speichert aktuelles Layout in aktuelle Bank/Preset. Endet auch beim Öffnen des Menüs. Konfigurierbar: `MIDI_PRESETS_CC_SAVE`.
- Senden nach Speichern/Laden (Menü oder MIDI): Program Change (Bank) + CC (Preset, Wert 127).
- Rückkopplungsschutz: bereits aktives Layout (`UI_PresetMatchesCurrent`) wird weder geladen noch gemeldet. MIDI wird bei offenem Menü ignoriert.

## Konventionen
- Zeilenenden der vorhandenen Dateien beibehalten (ui.c/ui.h/preset_store.*/midi_presets.* sind CRLF).
- Display: nur geänderte Zeichen neu zeichnen (Shadow-Cache); SPI-Zugriffe sparsam.
  Treiber: `ST7735_FillRect` sendet blockweise, waagerechte/senkrechte Linien laufen über FillRect, Zeichen (Skalierung ≤ 2) gehen als ein Block über `ST7735_DrawBitmap`. Keine Einzelpixel-Schleifen mit `ST7735_DrawPixel` für Flächen.
- Keine Blockierung in Interrupts; EEPROM-Schreiben blockiert kurz (bewusst).
- Host-Tests (gcc, Stub-HAL) sind möglich; Stand: Alles bisher nur teilweise auf dem Board geprüft (MIDI-Empfang/Senden bestätigt). MIDI-Speichern per CC0 ist nur am PC getestet.

## Offen / nächste Schritte
- Schaltplan im Preset (Platz reserviert), ADG2188-Treiber / `routing.c`.
- Loop manuell ein/aus schalten: Loop 1 per Fußschalter GPB5 (`UI_LoopToggle`, sendet On/Bypass wie beim Preset-Laden); weitere Loops / Bedienung im Menü offen.
- Loop-MIDI und Preset-Midi am Board testen (bisher nur Host-Tests).
- Boss DD-500 (CC27 127/0): sendet beim ersten Tritt nach einer per MIDI empfangenen Schaltmeldung gar nichts (vom Nutzer bestätigt), danach wieder den echten Zustand. Router-seitig nicht erkennbar, Loop-Status läuft bis zum nächsten Tritt auseinander. Lösung über Einstellungen am DD-500 offen. Eine Option "Feedback: Toggle" wurde probiert und wieder entfernt (hilft hier nicht).
- Copy-Funktion in Manage Presets.
- MIDI-Empfang per DMA statt Byte-Interrupt.
- Externer Speicher für 64×24 Presets (EEPROM reicht nur für 30).
- Auto-Routing findet bei sehr verschachtelten Layouts (viele Spalten, weit auseinanderliegende Lanes) keine Lösung, dann fehlen alle Verbindungen (Zufallstest, auch vor Convert to In + Out).
