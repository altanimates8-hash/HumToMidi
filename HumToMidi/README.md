# HumToMidi

Hum (or sing) into your mic and it turns your pitch into MIDI notes, so you can record them straight into the FL Studio piano roll. Built for sketching FNF vocal melodies.

It is monophonic (one note at a time), tracks roughly C2 to B5, and ignores silence and breath noise.

## 1. Get the Windows build

**No compiler needed (GitHub Actions)**
1. Create a GitHub repo and upload this whole folder (keep the `.github` folder).
2. Open the **Actions** tab. The `Build` workflow runs on its own (about 10 min). You can also press *Run workflow*.
3. Open the finished run and download the **HumToMidi-Windows** artifact. Inside:
   - `HumToMidi.exe` is the standalone app (recommended way in, see below)
   - `HumToMidi.vst3` is the plugin folder

**Local build:** install Visual Studio 2022 (Desktop development with C++) and CMake, then run
`cmake -B build` and `cmake --build build --config Release`. Output lands in `build/HumToMidi_artefacts/Release/`.

## 2. Record into FL Studio (standalone + virtual MIDI cable)

This route uses nothing FL-specific: FL records any MIDI input device into the piano roll.

1. Install [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and create one port (the default name is fine).
2. Run `HumToMidi.exe`. Click **Options → Audio/MIDI Settings** and set:
   - Audio input = your microphone
   - MIDI Output = the loopMIDI port
   - **Untick "Mute audio input"** (the standalone app mutes the mic by default, so nothing is detected until you do)
3. Tick **Mute audio output** in the plugin window (or wear headphones) so your mic doesn't feed back through the speakers.
4. In FL: **Options → MIDI settings → Input**, select the loopMIDI port and click **Enable**.
5. Select the channel you want to hold the notes, open its piano roll, arm recording (Record button or Ctrl+R, choose *Notes*), press Play and hum.

## 3. Using the VST3 inside FL (untested)

The plugin outputs MIDI, so FL's plugin wrapper settings (gear icon, **Settings** tab, **MIDI** section) give it an *Output port* that another channel can listen to with a matching *Input port*. I could not confirm that FL lets you record that routed MIDI into the piano roll, and FL's manual doesn't cover it. If notes don't arrive, use route 2.

## Controls

| Control | What it does |
|---|---|
| Input gate | Quietest level that counts as humming. The white mark on the meter shows where it sits. Raise it if background noise makes notes. |
| Note start time | How long a pitch must hold before a note begins. Lower = snappier, higher = fewer false notes. |
| Pitch change time | How long a new pitch must hold before the note switches. |
| Note end time | How long you can pause or drop out before the note ends. |
| Transpose | Shift the output in semitones. Handy to move a low hum up into the vocal range. |
| Velocity | *Follow volume* (louder hum = harder note) or a fixed value. |
| MIDI channel | Output channel, 1 to 16. |
| Mute audio output | Silences the audio passing through. |

## Tips

- Hum a steady, clear tone close to the mic in a quiet room. Vowels (mmm, ooo, aaa) track best; hard consonants break notes.
- Notes arrive about 80 ms after you start humming, because the plugin has to hear a stable pitch first. After recording, select all notes and nudge them slightly earlier in the piano roll, or quantize.
- Light vibrato (tested at plus or minus 30 cents) stays on one note. Wide slides turn into several notes.

## Tests

`HumDspTests` checks pitch accuracy, silence/noise rejection, vibrato, note changes, transpose and velocity (no JUCE needed). `HumProcessorSmoke` (`-DHUM_BUILD_SMOKE_TEST=ON`) runs a synthetic hum through the actual plugin. Both run in the GitHub workflow.
