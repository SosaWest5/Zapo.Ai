# Zapo.Ai — your producer sidekick

A native Windows x64 VST3 source project for FL Studio. Your eight supplied fruit faces are embedded in the plugin. It passes the beat through unchanged and sends deliberately captured clips to an AI service for **text** replies.

## Get a Windows plugin from GitHub

1. Create a GitHub repository named `zapo-ai`.
2. Extract this source ZIP on your PC. Upload **the contents of the `zapo-ai` folder**, including `.github/workflows/build.yml`, to the repository root. Do not upload only the ZIP. If Windows hides `.github`, enable hidden items in File Explorer. GitHub desktop is also an option: add this folder, commit, publish.
3. Open **Actions → Build Zapo Windows VST3**. A push starts it automatically; you can also choose **Run workflow**. If GitHub asks, enable Actions.
4. Open the completed green run. Scroll to **Artifacts**, download **Zapo-Ai-Windows-VST3**, and extract it. If the run is red, open the failing step's log; no working build is claimed until this step succeeds.
5. Copy the **whole `ZapoAI.vst3` folder**, not just its internal file, into `C:\Program Files\Common Files\VST3`. Windows may ask for administrator permission to copy it.
6. In FL Studio, open **Options → Manage plugins → Find installed plugins**. Load Zapo.Ai into the **last effect slot on the Master** to hear the full mix as it reaches that slot. It doesn't alter the mix.

## Connect the AI (one-time setup)

Install Python **3.10 or newer** from https://www.python.org/downloads/windows/ (include the Python launcher). The bridge uses Python's standard library; no pip packages are required.

Create an OpenAI API key at https://platform.openai.com/api-keys and configure API billing. **ChatGPT Plus does not cover API usage.** Do not paste a key into GitHub, the plugin source, or chat.

On Windows search **Edit environment variables for your account**. Under User variables add:

- Name: `OPENAI_API_KEY`
- Value: your API key

Close and reopen terminal windows after changing environment variables. Double-click `bridge/start-windows.bat` in the downloaded package. Keep that window open while using Zapo. It should say **API key configured**. Your key stays in the local bridge and is not compiled into the plugin.

Optional user variables: `ZAPO_TEXT_MODEL` (default `gpt-4.1`), `ZAPO_AUDIO_MODEL` (default `gpt-audio-1.5`). The latter must support WAV audio input and text output through Chat Completions. Model access depends on your account; change these if your account cannot access the defaults.

## Use Zapo

- **Text chat:** type a question and press Enter or Send. Ask about drums, chords, arrangement, production, theory or history. Replies are text; there is no spoken output.
- **Listen to Beat:** start playback in FL, click the button, and capture 1–30 seconds. Click **Stop & Review** while playback is still running, or let it stop automatically at 30 seconds. Zapo hears this clip, gives reasons and suggestions, and selects a reaction face.
- **Hold to Talk:** hold the button with the mouse, speak, and release to send your spoken question. The reply is text.
- **Toggle mic:** click once to start speaking and again to stop/send. Capture also stops at 30 seconds.
- **Mic settings:** select the microphone separately. If FL's ASIO device prevents access, select Windows Audio/WASAPI and a working input device for Zapo. Hardware/driver combinations need testing; avoid routing your mic to the Master during beat review. The mic audio isn't fed back into the beat by Zapo.
- **Teach / Tips:** asks the typed question in teaching mode, or requests a tip if the text box is empty. Uses the **happy cap**.
- **Taking your advice:** switches to the **mad cap** for serious coach mode while you work through the suggestion. This is an explicit button; it doesn't pretend to detect your edits. Make your changes in FL, then listen again.

## Face meanings

| File | Meaning |
|---|---|
| `scared.jpg` | Shock / surprise |
| `disgusted.jpg` | Strong disapproval; based on your expression reference |
| `feeling.jpg` | Feeling the beat |
| `coach.jpg` | Mad cap: taking the advice / serious coach mode |
| `unimpressed.jpg` | Not feeling it |
| `teacher.jpg` | Happy cap: teaching, helping, tips |
| `stank.jpg` | Stank face / impressed groove |
| `happy.jpg` | Happy / positive |

The original images have white backgrounds. They are displayed on a white character panel without being regenerated. The AI chooses reactions in its reply; there are no random ratings.

## What this version does and doesn't do

This is a first source release, not a finished commercial installer. Real AI requires the running bridge, internet and a working API key. Beat feedback uses audio input, not canned messages or speech-only transcription. The audio model can mishear music or infer incorrectly: estimated key/chords should be treated as suggestions to check, not guaranteed note extraction. History answers may be wrong; this version has no live web retrieval or source lookup.

It only knows the audio at its mixer slot plus your questions and recent conversation. It cannot inspect the full FL project, edit the piano roll, move knobs, identify sample ownership, or watch you implement advice. **Listen again** is a fresh review with conversation context, not an automatic A/B audio comparison. Chat history lasts while the editor is open and is not saved in the project.

Capture is **mono 16-bit WAV**, up to 30 seconds and approximately 24 kHz maximum. Stereo is folded to mono, so stereo imaging and phase cancellation cannot be judged reliably. Simple averaging reduces high-frequency detail during downsampling. Peak/RMS and clipping counts refer to the captured mono signal, not LUFS, true peak or individual instruments. Capturing does not change the stereo output.

Captured audio/question/history goes to OpenAI **only when you send a chat or completed capture**. No automatic background uploads. The bridge doesn't save clips or log conversations. A private local token guards the loopback connection at port 8765. The mic stops on release/toggle and cannot run during beat capture. If FL audio processing is suspended, beat finalization waits for the next audio callback: resume playback before Stop & Review. Silence and very short clips receive an explicit error.

## Build locally

Windows: install Visual Studio 2022 with **Desktop development with C++**, Git, and CMake 3.22+. Then run from the repository root:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target ZapoAI_VST3 CaptureTests --parallel 2
ctest --test-dir build -C Release --output-on-failure
py -3 -m unittest discover -s tests -v
```

Output: `build/ZapoAI_artefacts/Release/VST3/ZapoAI.vst3`.

CMake fetches a pinned JUCE 8.0.6 commit. An offline checkout can be supplied with `-DJUCE_SOURCE_DIR=path/to/JUCE`. JUCE's own license applies: review https://juce.com/legal/juce-8-licence/ before distributing a commercial binary. This source project uses JUCE; it is not made or endorsed by Image-Line. The mascot uses the supplied FL-fruit-inspired artwork; verify artwork/branding rights before public commercial distribution.

On Linux, install JUCE's standard build dependencies (ALSA, X11, Freetype, Fontconfig, pkg-config, CMake, C++17 compiler), configure `-DCMAKE_BUILD_TYPE=Release`, then build `ZapoAI_VST3`. FL Studio testing must happen on Windows.

## Verification

`tests/test_bridge.py` tests audio payloads, spoken-question mode, face rules, input validation, loopback authorization and the HTTP response path with a mocked AI. `tests/capture_test.cpp` tests capture duration, mono folding, sample-rate conversion, stop handling and producer/consumer handoff. These tests do **not** prove model quality or FL/microphone compatibility. See `VALIDATION.md` for the checks actually performed for this package.

Official implementation references:
- https://developers.openai.com/api/docs/guides/audio-chat-completions
- https://github.com/juce-framework/JUCE/blob/8.0.6/docs/CMake%20API.md
- https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer_iorouting.htm
