# SPASynth changelog

## 1.0.2

**New**

- On-screen keyboard. Click the keyboard icon at the bottom-right of the window
  (or open the settings menu) to show a playable keyboard strip. Play it with
  the mouse or with your computer keyboard (QWERTY), so you can audition sounds
  without a MIDI controller connected. Click the icon again to hide it.
- Settings menu. Click the SPASynth logo in the top-left to open a settings menu
  with Set Library Folder, Rescan Library, Accent Colors, Show Keyboard, and
  Clear All MIDI Learn. It works the same in the plugin and the standalone.

**Fixed**

- The standalone app now launches on macOS 11 (Big Sur) and later. A missing
  build setting made it require a much newer macOS, so it would refuse to open
  on older systems even though the plugins loaded fine.
- Reverb Mix now works as a true dry/wet dial: fully dry at 0 percent, fully wet
  (pure reverb) at 100 percent, with a smooth, even sweep in between. Before, the
  dry signal never fully left and enabling reverb nudged the level up.
- Arpeggiator swing no longer drops notes. With swing turned up, every second
  step could be skipped depending on the audio buffer size.
- The master output meter no longer sits flush against the right edge of the
  window.

**Notes for this build**

- The macOS installer is signed and notarized by Apple, so it installs cleanly.
- The Windows installer is unsigned by design. On first launch, click More info
  and then Run anyway to get past the SmartScreen prompt.
- Runs on macOS 11 or later (Apple silicon and Intel) as AU, VST3, and
  Standalone, and on Windows 10 or later as VST3 and Standalone.
- Known limitation: on a Mac with a Retina laptop screen plus an external
  monitor, the standalone window may not drag across onto the external display
  (a fixed-aspect window plus mixed-resolution quirk in the window system). It
  works normally as a plugin in your DAW. Workaround: set the external display
  as your main display in System Settings, or use SPASynth as a plugin.
