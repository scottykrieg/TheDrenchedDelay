# ChromaDelay — VST3 Plugin

A professional-grade stereo delay effect with a modular effects chain, built with:
- **JUCE 8** C++ backend (VST3 + Standalone)
- **React + TypeScript** frontend (served via WebBrowserComponent)
- **Projucer** project management / **Visual Studio 2022** for C++ compilation

---

## Project Structure

```
ChromaDelay/
├── ChromaDelay.jucer          ← Open this in Projucer to generate VS solution
├── Source/
│   ├── PluginProcessor.h/cpp  ← AudioProcessor + all parameter definitions
│   ├── PluginEditor.h/cpp     ← WebBrowserComponent editor + JS bridge
│   └── DSP/
│       ├── DelayEngine.h/cpp  ← Core delay loop + effect orchestration
│       └── Effects.h          ← All DSP effect classes (header-only)
└── UI/                        ← React/TypeScript frontend
    ├── package.json
    ├── vite.config.ts
    ├── index.html
    └── src/
        ├── main.tsx
        ├── App.tsx             ← Full plugin UI
        ├── types/plugin.ts     ← Parameter IDs + PluginState types
        ├── hooks/usePluginBridge.ts ← C++ ↔ React bridge hook
        └── components/
            ├── Knob.tsx        ← SVG rotary control
            └── Controls.tsx    ← Toggle, LedButton, Selector, Section
```

---

## Build Instructions

### Prerequisites
- **JUCE 8** installed (https://juce.com)
- **Visual Studio 2022** with C++ workload
- **Node.js 20+** and **npm**
- **Projucer** (ships with JUCE)

---

### Step 1 — Configure Projucer

1. Open `ChromaDelay.jucer` in Projucer
2. In **Exporters → VS2022**, update the **Module Paths** to point to your JUCE installation:
   ```
   C:/JUCE/modules   (or wherever your JUCE 8 lives)
   ```
3. Fill in **Company / Plugin Manufacturer Code** with your own 4-letter code
4. Click **Save and Open in IDE**

---

### Step 2 — Build C++ Backend

1. Open the generated `.sln` in Visual Studio 2022
2. Set configuration to **Release | x64**
3. Build → **Build Solution** (Ctrl+Shift+B)
4. Output VST3 appears in `Builds/VisualStudio2022/x64/Release/VST3/`

---

### Step 3 — Build React UI

```bash
cd UI
npm install
npm run build      # Production build → UI/dist/
# OR
npm run dev        # Dev server at http://localhost:5173
```

---

### Step 4 — Development Workflow

The editor loads `http://localhost:5173` in debug builds. For rapid iteration:

1. Start the Vite dev server: `npm run dev`
2. Load the plugin in your DAW (or Standalone)
3. The WebView shows the React UI — hot module reload works!

> **Tip:** In VS Code, install the **C/C++**, **ESLint**, and **Prettier** extensions.

---

### Step 5 — Production Embedding

For release builds, embed the React bundle as a JUCE BinaryData resource:

1. Build the UI: `npm run build`
2. In Projucer, add all files in `UI/dist/` as **Binary Resources**
3. In `PluginEditor.cpp → getUIUrl()`, replace the localhost URL with:

```cpp
// Serve from embedded resource
// Load index.html from BinaryData and serve via WebBrowserComponent resource provider
```

The recommended pattern for JUCE 8:

```cpp
// In PluginEditor constructor, use Options::withResourceProvider:
auto opts = juce::WebBrowserComponent::Options{}
    .withResourceProvider([](const auto& url) -> std::optional<juce::WebBrowserComponent::Resource> {
        // Map URL path → BinaryData
        if (url == "/" || url.endsWith("index.html"))
            return {{ BinaryData::index_html, BinaryData::index_htmlSize, "text/html" }};
        // ... map other assets
        return std::nullopt;
    });
```

---

## Parameter Bridge Architecture

```
JUCE AudioProcessor (audio thread)
    └─ AudioProcessorValueTreeState (APVTS)
           │
           ├─► PluginEditor::parameterChanged()      [any thread]
           │       │
           │       └─► AbstractFifo (lock-free queue)
           │                   │
           │           MessageManager::callAsync()  [message thread]
           │                   │
           │           WebBrowserComponent::evaluateJavascript()
           │                   │
           │           window.__chromaDelay._receiveParam(id, value)
           │                   │
           │           React: usePluginBridge → setState → re-render
           │
           └─◄ React setParam() 
                   │
                   window.__chromaDelay.sendMessage({ type:'setParam', ... })
                   │
                   WebBrowserComponent resource interception
                   │
                   PluginEditor::nativeCallFromJS()
                   │
                   APVTS param → setValueNotifyingHost()
```

---

## DSP Signal Flow

```
Input Signal
    │
    ├─[InputGain dB]─────────────────────────────────────┐
    │                                                     │
    │  ┌──────────────────────────────────────────────┐  │  Dry path
    │  │              DELAY FEEDBACK LOOP             │  │
    │  │                                              │  │
    │  │  Read delay buf (with GridOffset jitter)     │  │
    │  │      │                                       │  │
    │  │      ├─ [Reverse]  (if enabled)              │  │
    │  │      ├─ [Bitcrusher]                         │  │
    │  │      ├─ [Saturation]                         │  │
    │  │      ├─ [Resonant Filter LP/HP/BP]           │  │
    │  │      ├─ [Chorus/Vibrato]                     │  │
    │  │      ├─ [Phaser]                             │  │
    │  │      ├─ [Tremolo]                            │  │
    │  │      ├─ [Harmonizer — Cascade mode]          │  │
    │  │      └─ [Ghost Delay]                        │  │
    │  │      │                                       │  │
    │  │      ├─ [Reverb — stereo smear]              │  │
    │  │      ├─ [Phase Invert]                       │  │
    │  │      └─ [Harmonizer — Static mode]           │  │
    │  │                                              │  │
    │  │  Write: input + (wet × feedback) → buf       │  │
    │  │  [Lock/Hold: freeze buffer, read only]       │  │
    │  └──────────────────────────────────────────────┘  │
    │                                                     │
    └─────────── Mix (dry/wet blend) ────────────────────┘
                        │
                   [OutputLevel dB]
                        │
                   ► Output
```

---

## Commercial Release Checklist

### DSP Hardening
- [ ] Replace per-sample `raw()` calls in `DelayEngine::process()` with block-start cache
- [ ] Add DC blocker (one-pole HPF) after feedback loop to prevent DC buildup
- [ ] Add soft-knee limiter (not just hard clip) at output
- [ ] Validate CPU usage at 44.1k, 48k, 88.2k, 96k, 192k sample rates
- [ ] Test mono/stereo in both VST3 and standalone

### Harmonizer
- [ ] Upgrade to phase-vocoder pitch shifter for better quality at slow rates
- [ ] Add anti-aliasing filter before pitch shifting

### UI
- [ ] Implement `withResourceProvider` for embedded assets (no Vite server in production)
- [ ] Handle WebView2 not installed on user's Windows machine (fallback UI)
- [ ] Add preset load/save UI controls
- [ ] Keyboard accessibility (tab order, ARIA labels for screen readers)

### Plugin Metadata
- [ ] Set your real `pluginCode` (unique 4-char code, register at Steinberg)
- [ ] Set your real `pluginManufacturerCode`
- [ ] Fill in `bundleIdentifier` with your reverse-DNS company ID

### Testing
- [ ] Reaper, Ableton, FL Studio, Pro Tools (AAX if needed) compatibility
- [ ] Windows 10 + 11 (x64)
- [ ] Denormal performance test (flush-to-zero compiler flag `/fp:fast` in VS)

### Safety
- [ ] Fuzz test: automation of all params simultaneously for 60 seconds
- [ ] State save/recall round-trip test
- [ ] CPU spike test at max feedback + all effects on

---

## Known Prototype Limitations

1. **Harmonizer** uses overlap-add granular shifting. Phase vocoder would give cleaner results, especially at slow rates.
2. **JS Bridge** uses `evaluateJavascript` polling + FIFO. For production, implement the JUCE 8 `WebBrowserComponent::Options::withNativeFunction` API for direct C++ function exposure.
3. **Reverse effect** buffers per delay window. True time-reversed audio requires buffering the entire delay time — the current approach is approximate.
4. **Ghost Delay** has a minor variable `c` aliasing issue in the implementation — replace the `[c = ch]` capture with a direct channel index parameter before release.
5. **processEffectChain** calls `apvts.getRawParameterValue()` per sample. Cache values at block start for production.

---

## License

Commercial — All Rights Reserved. Replace with your license before shipping.
