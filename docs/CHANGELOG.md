# SPASynth changelog

## 1.0.26

- Added "About SPASynth..." to the logo settings menu. It shows the version,
  build date and commit, which format is running (AU, VST3 or Standalone),
  a link to silverplatteraudio.com, and a "Copy Info" button for pasting a
  quick summary into a support message.
- The filter's KEYTRK knob now reads as a percentage, with a tooltip
  explaining what it does: 100% means the cutoff follows the note you play
  exactly, one octave up for every octave you play higher; C3 is left
  unchanged. Existing patches sound exactly the same, only the readout
  changed. This came from feedback.
- New "Key" source in the mod matrix. It tracks the note you play, so you
  can route it to any destination, for example the filter cutoff, to open
  it up on higher notes and darken it on lower ones. This came from
  feedback.
- The Convolve tab's "From library..." menu now remembers where you left
  off: the pack (and sample) your current impulse came from is ticked, or
  if nothing's loaded yet, the pack you last browsed shows up highlighted
  instead of the list always starting from the top. Picking an impulse this
  way also updates where the "browser" file chooser opens, so the two stay
  in sync. This came from feedback.
- The AMP, ENV 2 and ENV 3 displays now show a small glowing dot riding the
  envelope as it plays: through attack, decay, a gentle pulse while held,
  then release, following the real modulated envelope rather than just the
  raw knob settings. Held notes each get their own dot, with the most
  recently played note shown brightest. This came from feedback.
- Mod matrix rows now have a drag handle so you can reorder them, instead
  of ending up with empty rows stuck in the middle and scrolling past them
  for nothing. Clearing a row now automatically pulls the rows below it up
  to close the gap, and right-clicking a row offers a quick "Clear row"
  option. This came from feedback.

## 1.0.25

- You can now browse presets by sound type. A new TYPE menu next to the
  pack menu in the preset browser lets you narrow the list to just Bass,
  Pad, Lead, and so on. Start a preset's own name with a type tag such as
  BASS, PAD or LEAD and it lands in that type automatically; our factory
  presets are already sorted this way, so Keys, Texture and Pulse presets
  show up as Keys, Soundscape and Rhythmic respectively. This came from
  feedback.
- Delay ping-pong now actually bounces. Before, a centred or mono sound
  echoed the same whether ping-pong was on or off. A new WIDTH knob sets
  how far the echoes bounce between left and right: 100% bounces fully
  from side to side, lower settings narrow it. WIDTH only applies with
  ping-pong on and is dimmed otherwise, so presets and sessions with
  ping-pong on will now bounce noticeably wider than before. This came
  from feedback.
- Redesigned the Chorus, Delay, Mod and Trem/Vib displays so every knob
  actually shows up in the picture, not just one or two of them. Chorus now
  reacts to rate, width, mode and feedback as well as depth and mix; Delay
  draws real echo timing (down to milliseconds or the beat division you
  chose) and splits into separate left/right traces when ping-pong is on;
  Mod shows the phaser notches or flanger comb sweeping in time with your
  settings; and Trem/Vib finally shows its own tremolo and vibrato shapes.
  Previously the Mod and Trem/Vib tabs were showing the Chorus picture by
  mistake, so their own knobs did nothing visible; that's fixed too.
- The effect tabs are laid out more tightly, so the displays get more room
  and the knobs are larger. Knob labels inside each effect tab now read
  RATE, DEPTH, MIX and so on, since the tab already names the effect;
  automation names in your DAW are unchanged.
- Fixed a mod matrix display bug reported by one of our testers: when you
  pick a source and destination from the menus, the row now lights up
  straight away instead of staying greyed out until you move the mouse over
  it. The assignment was always working; only the row's own display was
  slow to catch up.
- RANDOMIZE ALL could sometimes hand you a patch that showed on the meters
  but was far too quiet to hear, especially after several rolls in a row.
  We tracked down the combinations responsible, including a filter tuned
  far from the note being played, a mod matrix route pushed too hard, a
  sparse arpeggiator, and a sample start landing in a quiet stretch of a
  longer sound file. Random sample starts now only land on parts of the
  file you can hear, so you still get a different slice each roll. This
  came from feedback.
- Fixed a possible crash when closing SPASynth right after clicking
  RANDOMIZE ALL several times in quick succession.
- Presets now save and restore the WILD setting. Before, it was only
  stored if you had moved the knob since opening SPASynth, and the knob
  could keep showing the old value after a preset loaded. This came from
  feedback.
- Loading a preset no longer changes your window size or the on-screen
  keyboard's octave. A preset is a sound, not a window layout.
- Reverb now starts right after the PRE time you set, instead of arriving
  noticeably later than the knob says. This came from tester feedback, and
  every reverb also blooms sooner and more naturally as a result, including
  in your existing presets and sessions. Decay time and each mode's tone
  stay close to what they were. We rebalanced every factory preset so it
  keeps its dry/wet mix; if you saved your own reverb settings, the wet
  level may sit a little different, mainly at longer decay times or a
  bigger room size.
- Each oscillator can now skip the filters. New A/B/C toggles on the
  Filter 1 and Filter 2 tabs let you keep, say, a sub or noise layer
  unfiltered while the rest of the patch is shaped as usual. Every
  oscillator defaults to going through the filters as before, so existing
  presets and sessions sound the same. This came from feedback.
- Each LFO now has a Custom shape you can draw yourself. Pick Custom from
  the LFO's shape menu and its display becomes an editor: drag points to
  reshape the curve, double-click empty space to add a point or double-click
  a point to remove it, and drag the small handle between two points to
  bend that segment. Your drawn shape is saved with the preset, so it
  travels with your sound just like every other setting. This came from
  feedback asking for more expressive LFO shapes than the built-in ones.

## 1.0.24

- You can now delete your own presets: right-click a preset in the
  browser and choose Move to Trash. It only works on presets you saved
  yourself; factory presets show the option greyed out, so a right-click
  still tells you where you stand rather than doing nothing. Deleted
  presets go to the system Trash, so nothing is lost and you can put one
  back if you change your mind. Worth noting because it is a change in
  behavior: right-clicking a preset no longer loads it, since right-click
  now opens this menu, and left-click still loads as before. This came
  from feedback.
- The oscillator waveform no longer moves for chaos drift that does not
  apply to the current engine. In 1.0.23 we made the waveform react to
  Organic Chaos, but it also moved in Sample and Granular modes, where
  that particular drift does not change what you hear: in those modes the
  display is showing the audio file itself, which chaos does not alter.
  Chaos position drift is still shown where it genuinely applies, by the
  moving grain cloud in Granular and by the playhead in Sample. Wavetable
  and the synth engines are unchanged. This came from feedback.
- The file browser now remembers where you were, separately for
  wavetables and for samples. Previously the two shared one remembered
  folder, so loading a wavetable moved the folder the sample browser
  would open in, and the other way round, which meant navigating back and
  forth every time. Each now keeps its own place. Your existing
  remembered folder carries over to both the first time. This came from
  feedback.



## 1.0.23

- Reverb: the five modes now sit at a consistent level, and the tone
  filters no longer drift with sample rate. The reverb engine and its mix
  control have not changed since 1.0.15; what we found instead was that
  four of the five modes (Plate, Chamber, Room and Spring) were running
  hotter than Hall, by up to about double, against the level they were
  meant to share. They are now trimmed to match Hall exactly, and Hall
  itself, the default, is unchanged. Factory presets using those four
  modes have had their reverb amount adjusted so every preset keeps the
  balance it had, and factory presets will refresh themselves on the next
  scan. Separately, the reverb's low cut and high cut were tuned for 48 kHz
  only, so at 96 kHz or with oversampling on they landed an octave or more
  low; they now track the session rate.
- Chorus: now genuinely stereo, with a WIDTH control and a Vintage /
  Modern mode. The old chorus moved both channels together, so it imaged
  as mono and could read as a phaser; WIDTH sets how far apart the two
  channels move, from together at 0 to fully opposed at 100. Vintage is
  modeled on the classic bucket-brigade chorus of an early-80s analog
  polysynth: short delay, gentle high-frequency rolloff, and the two sides
  in opposite phase for that wide swirl, while Modern is clean and
  full-bandwidth. Defaults are Modern and WIDTH 50, so existing presets
  pick up the stereo width; this changes how their chorus sounds, and that
  is intended. This came from feedback.
- ASSIGN could pick a hidden control. In DEST mode, clicking a knob could
  assign a different parameter that was hidden underneath it, one left
  over from a previous oscillator engine, so for example choosing FM Ratio
  could write Pluck Damp. Hidden controls are no longer assignable. This
  came from feedback.
- An oscillator could go silent at BLEND 0 with an even unison count. With
  an even number of unison voices there is no center voice, so turning
  BLEND fully down muted everything. The innermost pair now stands in for
  the center, so BLEND at 0 still sounds. This is also reachable by
  modulation, which is how we found it.
- The oscillator waveform displays now visibly react to Organic Chaos.
  Before, only chaos position drift reached the display, and only
  indirectly, so unless the chaos amounts were cranked the waveform barely
  moved even though the sound was drifting. The display now shows chaos
  pitch and phase drift too, in every oscillator mode while a note is
  sounding: the drawn shape slides sideways with phase drift and stretches
  or compresses with pitch drift, clearly visible but modest at the
  default settings, and capped at maximum so it never becomes a scribble.
  When chaos is off, or the oscillator is idle, the display draws exactly
  as it did before; the one limit is Noise mode, where the shape is random
  by nature, so the drift cue is not visible there. This came from
  feedback.



## 1.0.22

- Playing from the computer keyboard stopped as soon as you switched
  between effects, and you had to click the on-screen keyboard to get it
  back. Switching any tab group did it, not just the effects: bringing the
  newly shown tab to the front quietly took keyboard focus away. Focus now goes straight back, so
  QWERTY playing keeps working.
- RANDOMIZE ALL now stops any sounding notes first. Holding a key and
  hitting RANDOMIZE ALL could leave a note stuck on with no way to clear
  it. Randomizing now releases everything first, the same way loading a
  preset does. This came from feedback.
- Added a MACRO tab, beside LFO 1 to 3. The four macro controls have always
  existed as mod matrix sources, but there was no way to move them from
  inside the plugin, so routing one did nothing unless your DAW automated
  it. They now have knobs: route a macro in the matrix, then turn its knob,
  automate it from the host, or assign a MIDI controller to it. They are
  also selectable as sources in ASSIGN mode.
- Added an INIT button to the top bar, next to SAVE. It returns every
  parameter to the default patch. This was already available under the logo
  menu as Reset to Default, which almost nobody found; that menu item is
  still there. Note that it takes effect immediately with no confirmation,
  the same as RANDOMIZE ALL.
- Organic Chaos added a faint layer of grit, most audible on a clean tone.
  Its internal drives were updating in steps rather than moving smoothly,
  and each step was a tiny click. They now move smoothly between updates,
  which removes that noise without changing how deep or how fast chaos
  moves. This reduces the problem rather than eliminating it: we have
  identified a second, related source that affects wavetable oscillators
  specifically, and we will address that next.



## 1.0.21

- Added a SMOOTH knob to each LFO. Stepped shapes like Square and Sample and
  Hold jump instantly from one value to the next, which can click or pop in
  whatever they are modulating; SMOOTH rounds off those transitions instead.
  It works on every shape and defaults to 0, so existing presets sound
  exactly as they did before.
- Added a JITTER knob to each LFO. It blends a random value into the chosen
  shape, renewed once per cycle so it follows the LFO's own rate or
  division; at 0 you get the pure shape, at full a purely random one.
  Turning up SMOOTH and JITTER together gives a smooth random drift, a
  useful modulation source in its own right. Defaults to 0, so existing
  presets are unaffected.
- The Organic Chaos division menu showed no value with SYNC on. The menu
  was too narrow to draw the selected division, so it looked empty until
  you opened it. We shortened the chaos meter slightly to make room. The
  LFO panel was also rearranged a little to fit the two new knobs, with the
  shape and division menus now stacked.



## 1.0.20

- The loop XFADE display did not update until you moved a loop point.
  Setting XFADE appeared to do nothing until you nudged the loop start or
  end, at which point the crossfade shading suddenly appeared. The display
  was not listening for changes to the XFADE control itself; it only
  redrew when the loop points moved. It updates immediately now. The same
  omission affected the per oscillator time signature and the beat grid
  overlay, which are fixed the same way.
- Organic Chaos, routed through the mod matrix, did not run at the rate
  you chose. It ran at a random multiple of your chosen rate or division,
  and re-rolled that multiple on every new note. It now runs at exactly
  the rate or division shown, and stays there note to note. Chaos patches
  will wander a little differently than they did in 1.0.19 as a result;
  the character, depth and rate are unchanged.
- The check mark next to the selected item in a menu was too large. It now
  reads as a modest mark beside the label. This applies to every menu with
  a selected item (reverb algorithm, EQ band type and slope, MIDI Learn,
  the oscillator sample list, the settings menu), not just one.



## 1.0.19

- In Sample mode, the start point marker now runs the full height of the
  waveform display, like the loop markers, instead of the short tick at the
  top it used before. It reads at a glance now; it stays a dimmer color
  than the loop start/end markers so the two are still easy to tell apart.
  It also now stays visible while the sample is playing, with the moving
  playhead drawn over it, instead of being replaced by the playhead.
- Added a new XFADE control for looped samples. It crossfades the loop
  point so the seam stops clicking or jumping. The crossfade borrows audio
  from outside the loop itself (the run-up before the loop start, or the
  tail after the loop end), so turning it up never changes the loop's
  length or timing; that keeps beat-locked loops exactly in step with your
  project. It works on beat-locked (SYNC) loops as well as free-running
  ones. It defaults to 0, so every existing preset and session sounds
  exactly as it did before. If the loop points sit hard against the very
  start and end of the file, there is no audio outside the loop to borrow,
  so the control has nothing to work with and dims. The waveform display
  draws the crossfade as two shaded ramps at the loop seam, one of which
  sits outside the loop band, to show where the borrowed audio is coming
  from.
- The mod matrix's single ASSIGN button is now two buttons, SOURCE and
  DEST. Press SOURCE and only the mod sources light up; pick one, and only
  the matrix's source column lights up next. Press DEST and only the knobs
  light up, then only the destination column. Building a complete route
  takes one more click than before, and in exchange there is only ever one
  kind of thing to click at any moment. Assign mode now finishes after the
  single assignment you asked for, rather than staying on until both
  halves of a row are filled.
- A half-filled matrix row no longer calls attention to itself. In 1.0.18
  we added an outline and a ring around a half-assigned row to show it was
  waiting rather than stuck. With separate SOURCE and DEST buttons,
  assigning one half on its own is a deliberate, normal thing to do, so
  that callout was flagging intended behavior as unfinished; it is gone.
  A row that is not yet complete simply reads as dimmed, the same quiet
  treatment we already use for a row whose destination is inactive,
  with nothing chasing you.



## 1.0.18

- The color that marks a knob as wired into the mod matrix is now chosen
  automatically from your own accent color(s), so it always contrasts with
  whatever you have picked instead of being a fixed shade that could clash
  with, or disappear into, your own. It updates the moment you change your
  accents in the picker. If you use custom colors, your assigned knobs will
  look different than they did in 1.0.17.
- Removed the panic button from the header. Every host already offers its
  own way to stop stuck notes, and having a second one on our end was
  redundant; incoming MIDI panic messages are unaffected.
- ASSIGN mode improvements from tester feedback:
  - You can now switch tabs while ASSIGN is on, so a synth-wide assignment no
    longer means everything you want has to already be on the tab you
    started from. Filters, envelopes, LFOs and FX all stay reachable
    mid-assignment, and switching tabs immediately makes the newly shown
    controls assignable.
  - The blue highlighting now appears in stages instead of lighting up the
    whole synth and the mod matrix at once, which was leading people to
    click the matrix first, where nothing happens yet. Turning ASSIGN on now
    lights up only the knobs and sources you can pick from; once you pick
    one, it shows as selected and only the matching side of the matrix (the
    source column or the destination column, whichever you just picked)
    lights up next.
  - Assigning only half of a matrix row (a destination with no source yet,
    or vice versa) now clearly shows that row waiting for its other half:
    the row is outlined and a ring marks the exact field still to be filled.
    Previously this looked like ASSIGN mode had gotten stuck; it was always
    working as intended, just silently.
  - A matrix row aimed at Organic Chaos's RATE now dims while chaos SYNC is
    on, since SYNC replaces the free-running rate with a tempo division and
    modulating it does nothing until SYNC is switched off again. The row
    itself is untouched and starts working again the moment SYNC goes off.



## 1.0.17
- Added a small arrow next to the "LOCKS" label above the randomizer lock
  buttons, so it is clearer at a glance that it is pointing at them. Tightened
  up the label itself too: "LOCKS" now sits with a bit more breathing room
  off the window's left edge, and the gap to the arrow is snug rather than
  loose.
- Right-clicking an oscillator's title now offers Copy to Oscillator B/C or
  Swap with Oscillator B/C, so building a matching pair of oscillators or
  trying two sounds against each other no longer means re-dialing every knob
  and reloading the sound by hand. Copying or swapping carries the loaded
  sound or wavetable along with the settings; it does not touch anything you
  have wired up in the mod matrix.
- Organic Chaos can now lock to your tempo. Switch on SYNC and each drifting
  parameter still wanders at its own pace, but those paces become musical
  divisions of your song's tempo and every new wander lands on the beat, so
  pitch might settle into a new spot once a bar while amplitude resettles
  once a beat, all of it moving together rather than turning into a stepped,
  everything-at-once wobble. The section renames itself to Organized Chaos
  while this is on. Depth, mix and the six per-target amounts still work
  exactly as before; only the rate is tempo-driven now.
- Any knob wired into the mod matrix now shows it at a glance: it changes
  color the moment a route targets it, whether or not a note is playing and
  whether or not that route's depth is set to zero yet. A new travel arc
  also shows how far the assigned routes can actually push or pull the knob
  from where it sits right now, updating live as you drag a depth slider.
  The indicator uses a distinct color so it reads apart from the rest of
  the panel.
- Convolve now has a START knob for its impulse. Turning it up skips further
  into the loaded sound before the wet signal begins, so you can leave the
  direct hit and early reflections behind and keep only the long diffuse
  tail, a texture you could not reach before. It also opens up what you can
  do with a long recording used as an impulse: dial in to a later section of
  it instead of always starting at the very beginning. The knob never goes
  silent, even pushed all the way over, and RANDOMIZE ALL keeps it in the
  first half of its range so it stays musical on a random patch; drag it
  yourself for the rest.
- Every module with its own on/off switch now shows its power state right in
  its title: the name reads in a muted grey while the module is off and lights
  up in your accent color the moment you turn it on. This covers all three
  oscillators, both filters, Organic Chaos, the arpeggiator, and each FX tab.
  The mod matrix, envelopes and LFOs are always live, so their titles are
  unchanged.
- Clicking the centre of a knob that is already wired into the mod matrix
  now jumps straight to its routes: every matrix row that targets it lights
  up in that same indicator color, and the matrix scrolls
  so the first one is on screen right away. This is a plain click, not a
  drag, so it never gets in the way of turning the knob. The highlight stays
  put while you work on the row, and clears the moment you click anywhere
  else, press Esc, or switch on ASSIGN mode, whose own blue highlighting
  means something different and is unaffected by this.

## 1.0.16

- The library now refreshes itself when packs are added or removed, so
  packs installed by our companion app or copied in by hand appear without
  a rescan. The Rescan button remains for forcing one.
- Fixed a rare crash when the plugin window was closed within a moment of
  dismissing the VOICE panel.
- Setting up the sound library on Windows is friendlier. Our installer now
  creates the destination folder and adds a "SPASynth Sounds Folder" item to
  the Start Menu that opens it, so there is no hunting for the right place.
  We also find the library in more of the places it realistically ends up,
  including when Windows adds an extra folder of its own while extracting the
  zip, and when the library is still sitting in Downloads or on the Desktop.
- If we cannot find a sound library at all, we now say so the first time you
  open SPASynth and offer to take you straight to the folder picker, instead
  of leaving you with an empty preset browser and no explanation.
- The mod matrix ASSIGN button now switches itself back off once a route is
  complete, so a quick connection takes one click and no cleanup. Double
  click it instead to lock it on, the way caps lock works, and it stays on
  through as many assignments as you want. A small padlock on the button
  shows when it is locked, and Esc still leaves either mode.
- You can now drag audio straight onto an oscillator from the Finder, from
  Explorer, or from your DAW's own browser, instead of going through the LOAD
  button. The oscillator lights up as you drag over it so you can see where
  the sound will land, and dropping onto an oscillator that is not already
  playing a file switches it to Sample for you. The Convolve effect takes a
  dropped impulse response the same way.
- A new modulation route now arrives with its depth already set to half, so
  connecting a source to a destination does something straight away instead
  of staying silent until you also turn the depth up. Routes you have already
  dialled in, including ones you deliberately set to zero, are never changed,
  and loading a preset never alters the depths it was saved with.

## 1.0.15

A large round of features and fixes from our testers' playtest sessions.

- Analog oscillators gain a SUB knob: a square wave one octave below the
  main waveform, phase-locked so it never drifts, just like the classic
  sub-oscillator slider on a vintage analog synth.
- With LOOP on, sample oscillators gain a SYNC option that turns the loop
  into a beat-locked, tempo-matched part of your project. SYNC snaps the
  loop's start and end to the sample's own beat grid, stretches it to your
  project's tempo while keeping its pitch (using the sample's own detected
  tempo), and follows your project's tempo, locking to the transport while it
  plays, like a clip launcher: press a key and the loop joins in on the beat,
  right where the transport currently is. Turning LOOP off hides SYNC, since
  it's a loop feature. Each sample oscillator can also choose its own time
  signature for its beat-locked loop, defaulting to your project's, so loops
  in different meters can run against each other for polyrhythms.
- A time signature setting is available for the standalone's tempo bar and,
  for hosts that don't tell us their own signature, from the settings menu
  as well, so synced sample loops always land on the right beat.
- You can now zoom and pan the oscillator waveform display, which makes it
  much easier to work with long samples. Scroll or pinch to zoom in and out
  around your cursor, drag to pan once zoomed, and double-click to snap back
  to the whole file. Loop points and other markers stay lined up with the
  waveform no matter how far in you zoom.
- Knobs that are being modulated now show it live, with a moving indicator
  and a shaded range on the ring, while the knob itself keeps your base
  setting.
- EQ bands can now be switched between Bell, Low Shelf, High Shelf, Low Cut,
  High Cut, Notch, Band Pass and Tilt Shelf by right-clicking a node in the
  EQ display. Low Cut and High Cut bands also get a slope choice from 6 to
  48 dB per octave, for a real low-cut or high-pass instead of a single gentle
  filter. Double-clicking near the left or right edge of the EQ display now
  drops in a low cut or high cut in one gesture.
- Pulse factory presets now combine the pack's own sound with a real synth
  oscillator (wavetable, analog or FM), with the sample's own dynamics,
  pitch or envelope driving that synth layer in a different way for each of
  the six variants (its level, its pitch, the shared filter, a wavetable's
  scan position, or an FM amount). Every Pulse preset is now a genuine mix
  rather than sample-only. Every wavetable layer across all our factory
  presets now picks one of our built-in wavetable shapes to suit its
  character (Supersaw, Unison Spread, PWM or Bells, among others) instead of
  defaulting to a plain basic shape, and every preset's reverb has been
  re-voiced for our newer reverb engine, choosing the reverb character (Room,
  Plate, Chamber, Hall or Spring) and amount that best suits each sound.
- Fixed a real cause of totally silent factory presets against long,
  real-world sound-effects files: a held note's sample loop is now a short,
  guaranteed-audible window near the start of the file rather than looping
  the file's full length, so a long recording's own natural decay, or a
  quiet stretch partway through, can no longer leave a held note silent for
  the rest of its hold; granular presets nudge their grain read position
  inward and widen its per-grain spray for the same reason. Every factory
  preset across every pack in our library has been verified audible on a
  held note.

- Added an ASSIGN mode to the mod matrix. Turn it on and everything that can
  be part of a modulation route starts glowing blue, both the things you can
  modulate and the matrix rows themselves. Click a glowing control, then
  click a glowing row menu, and that route is made for you. Works for
  sources as well as destinations, and stays on so you can wire up several
  routes in a row. Turn it off with the same button or the Esc key.
- Fixed sample loops that stopped after one pass when the loop end was left
  at the very end of the file.
- Enabled effects now show their tab name in bold and in the brighter text
  colour in the FX section, so you can see at a glance which effects are
  engaged without clicking through each tab.
- The DIST effect has a new Crush type, a bit crusher. DRIVE controls both
  the bit depth and the sample rate reduction together, and TONE and MIX
  work exactly as they do for the other distortion types.
- The ORGANIC CHAOS display now shows the real chaos modulation scrolling
  across in real time, like a seismograph, instead of a static preview curve.
- Switching LATCH off now stops the arpeggio immediately, unless keys are
  still held, in which case it continues on just those.
- RANDOMIZE ALL can no longer land on a silent patch. Every roll is checked
  and gently nudged so a played note always produces sound. The arpeggiator's
  first step of a new chord now always plays, with the chance control only
  affecting the steps after it, so a low chance setting can't roll a
  fully-silent arp pattern.
- Factory presets have been redesigned with several distinct recipes for
  each type (Keys, Texture and Pulse), varying the synthesis engine,
  envelopes, filters, modulation and effects, so packs no longer all sound
  alike browsing through the preset list. Every factory preset now always
  features the pack's own sound in oscillator A, so its waveform is visible
  and audible rather than hidden behind another engine; neighbouring packs
  in the browser get different recipes so adjacent presets don't sound the
  same; and none of them use the arpeggiator. Existing factory presets are
  refreshed automatically the next time your library is scanned; your own
  saved presets are untouched.
- The preset browser no longer covers the synth. It now eases open beside
  the instrument, and closing it restores the original window width. Where
  the host allows it, the window grows to the left so the instrument stays
  put on screen; otherwise it grows to the right, because the host owns the
  window's position there. Hosts that cannot resize the window at all fall
  back to the previous behavior, with the browser overlaying the instrument.
- Wavetable oscillators now have a TABLE menu with built-in tables to choose
  from: Basic Shapes, Supersaw, PWM, Formant, Additive, Unison Spread, Sync
  Sweep and Bells, all morphable with the POSITION knob. Loading your own
  wavetable file still works exactly as before and takes priority over the
  TABLE menu.
- The on-screen keyboard's computer-key playing can now be shifted by octave
  with the Z and X keys, or the new minus and plus buttons on the keyboard
  strip. The setting is saved with your session.
- The REVERB effect has a new engine, based on the classic plate reverb
  design, for a smoother and more natural tail across all five modes (Hall,
  Plate, Chamber, Room, Spring).
- All the effects' MIX knobs, including REVERB, now read in percent, and an
  even 50% now gives an exact half-and-half blend of the dry and processed
  signal instead of the more sensitive curve REVERB used before. Factory
  presets have been refreshed for the new REVERB feel.
- If a preset's sound file can't be found (a moved or renamed library
  folder, an unplugged drive), the strip now says so clearly instead of
  reporting an unrecognized audio format.
- The plugin window now opens at a size that fits the screen it appears on,
  and remembers the size you resize it to.
- Fixed MIDI Learn not actually learning: moving a hardware control while a
  learn was armed could leave it waiting indefinitely. While a learn is
  armed, the control now shows what kinds of MIDI messages are arriving from
  your controller (notes, pitch bend, aftertouch, and CC) so you can tell at
  a glance whether it's sending anything at all, or sending everything
  except the CC needed for MIDI Learn (a common controller/DAW setting
  issue) -- and confirms exactly which controller it learned once a CC
  arrives.

**Notes for this build**

- The macOS installer is signed and notarized by Apple, so it installs cleanly.
- The Windows installer is unsigned by design. On first launch, click More info
  and then Run anyway to get past the SmartScreen prompt.

## 1.0.14

Three stability fixes, two of them found with a memory checker (AddressSanitizer)
that we have now added to our release checks.

- Fixed a crash when recording in Logic (and other hosts) with a count-in or
  pre-roll while the arpeggiator was on. Before bar 1 the host reports a
  negative song position, and the arpeggiator used it to look up its pattern
  with a negative index. The pattern now wraps correctly from any position, so
  the arp plays the same notes during a count-in that it plays after bar 1.
- Fixed rare, sudden bursts of noise in the reverb tail. When the modulated
  read position of a reverb delay line landed exactly on the buffer boundary,
  a rounding quirk read one sample past the end of the buffer and fed whatever
  happened to be in memory there into the reverb's feedback. Whether you ever
  heard it depended on memory layout, which is why it came and went between
  builds. The same boundary guard is now applied to the delay, the phaser and
  flanger, and the vibrato, which used the same read pattern.
- Hardened the VOICE panel against a project being closed while it is open:
  the panel now cuts its ties to the synth the moment the plugin window goes
  away, so the host's deferred cleanup can never touch a synth that is
  already gone.

## 1.0.13

Two small fixes on top of the 1.0.11 redesign.

- Fixed the VOICE panel's MODE and PRIORITY dropdowns. They could refuse to
  stay open unless you held the mouse down, and selections would not
  register. Both now open and select normally, and closing the panel hands
  control straight back to QWERTY note play.
- Fixed a crash in Logic when the plugin window was closed while the VOICE
  panel was still open. The panel now closes cleanly with the window.

**Notes for this build**

- The macOS installer is signed and notarized by Apple, so it installs cleanly.
- The Windows installer is unsigned by design. On first launch, click More info
  and then Run anyway to get past the SmartScreen prompt.
- Runs on macOS 11 or later (Apple silicon and Intel) as AU, VST3, and
  Standalone, and on Windows 10 or later as VST3 and Standalone.
- The Limiter's optional lookahead mode adds a small amount of latency and
  reports it to your host automatically so playback stays in sync. It's off
  by default, so live play stays at zero added latency until you turn it on.
- Known limitation: on a Mac with a Retina laptop screen plus an external
  monitor, the standalone window may not drag across onto the external display
  (a fixed-aspect window plus mixed-resolution quirk in the window system). It
  works normally as a plugin in your DAW. Workaround: set the external display
  as your main display in System Settings, or use SPASynth as a plugin.

## 1.0.11

A visual redesign of the faceplate, plus a tester-requested feature and two
polish fixes.

- SPASynth has a new look: one continuous dark faceplate instead of separate
  bordered modules, with recessed selector and title bands and softly
  layered shadows between rows. This is a visual pass only, nothing about
  how SPASynth sounds or behaves has changed.
- Sample oscillators now show their loop points right on the waveform
  display. When LOOP is on, the loop region is shaded and its start/end
  points are marked with crisp lines, so you can see exactly where a long
  sample repeats instead of guessing from the knobs. The playback start
  point gets its own subtle marker. Turn LOOP off and the shading/markers
  clear, leaving just the start marker; this doesn't apply to granular mode,
  which already has its own live grain view.
- Fixed a layout bug where a module's tab row (ENV, LFO, FILTER, the FX
  tabs) would condense and re-space itself the first time you clicked a
  tab. Tab rows now lay out the same way from the moment SPASynth opens.
- Fixed a bug where an effect panel's bottom row of knob labels could get
  squashed or clipped on control-heavy tabs like TREM/VIB. Labels now
  always get the room they need; the panel's live display shrinks to make
  room for them if needed.

**Notes for this build**

- The macOS installer is signed and notarized by Apple, so it installs cleanly.
- The Windows installer is unsigned by design. On first launch, click More info
  and then Run anyway to get past the SmartScreen prompt.
- Runs on macOS 11 or later (Apple silicon and Intel) as AU, VST3, and
  Standalone, and on Windows 10 or later as VST3 and Standalone.
- The Limiter's optional lookahead mode adds a small amount of latency and
  reports it to your host automatically so playback stays in sync. It's off
  by default, so live play stays at zero added latency until you turn it on.
- Known limitation: on a Mac with a Retina laptop screen plus an external
  monitor, the standalone window may not drag across onto the external display
  (a fixed-aspect window plus mixed-resolution quirk in the window system). It
  works normally as a plugin in your DAW. Workaround: set the external display
  as your main display in System Settings, or use SPASynth as a plugin.

## 1.0.10

Fixes and polish from the latest round of tester feedback.

- Playing notes from the computer keyboard no longer stops after clicking
  buttons such as RANDOMIZE ALL, SAVE, the preset arrows, or the panic
  button. A previous fix already covered knobs and dropdowns; every button
  in SPASynth now leaves your QWERTY playing alone. This also covers the
  preset browser (opening the browser itself while the on-screen keyboard is
  showing, picking a preset from the list, changing the pack category, and
  using the KEYS/TEXTURE/PULSE/USER filter chips) and every remaining on/off
  switch and dropdown across the FX sections and filters (for example an
  effect's own ON toggle or character menu) that had been missed by the
  earlier pass. Computer-keyboard playing should now survive any click
  anywhere in SPASynth, including opening the preset browser itself, and
  Esc still closes the browser either way.
- Fixed a library folder bug: picking a folder that held sound files
  directly (with no pack subfolders inside it) was silently rejected, and
  in some cases quietly replaced your chosen folder with the default
  install location, with no explanation shown. SPASynth now accepts a
  plain folder of sound files as a library, and if a folder truly has none,
  it tells you plainly instead of failing silently or discarding your
  choice.
- Controls that don't apply to the current setting are now visibly greyed
  out and can't be touched, instead of looking active but doing nothing.
  For example: an LFO's rate knob while it's synced to tempo (and its
  division menu while it isn't), a sample oscillator's loop range while
  looping is off, the delay's time knob while it's synced to tempo, and
  glide time while glide is off.
- Fixed a short burst of noise on clicking a preset in the browser. It could
  happen if you'd just played and released a note (or the delay/reverb/mod
  tail was still ringing) right before switching patches, since the new
  preset's settings were being applied to that still-active leftover sound.
  Loading a preset now clears that leftover state first, so switching
  patches is always silent unless you're actually playing.
- Added user preset banks. Inside the Save dialog, choose New Folder to
  create a bank in your User presets folder, and it shows up as its own
  category in the preset browser, right alongside your other presets.

**Notes for this build**

- The macOS installer is signed and notarized by Apple, so it installs cleanly.
- The Windows installer is unsigned by design. On first launch, click More info
  and then Run anyway to get past the SmartScreen prompt.
- Runs on macOS 11 or later (Apple silicon and Intel) as AU, VST3, and
  Standalone, and on Windows 10 or later as VST3 and Standalone.
- The Limiter's optional lookahead mode adds a small amount of latency and
  reports it to your host automatically so playback stays in sync. It's off
  by default, so live play stays at zero added latency until you turn it on.
- Known limitation: on a Mac with a Retina laptop screen plus an external
  monitor, the standalone window may not drag across onto the external display
  (a fixed-aspect window plus mixed-resolution quirk in the window system). It
  works normally as a plugin in your DAW. Workaround: set the external display
  as your main display in System Settings, or use SPASynth as a plugin.

## 1.0.9

A performance and stability pass, wrapping up ahead of launch.

- Bypassing SPASynth in your host now lets reverb, delay, and other effect
  tails ring out naturally instead of cutting them off instantly.
- Lower CPU use for long convolution impulses, and a lighter-weight reverb
  tail engine, with no change to how either sounds.
- The EQ's spectrum analyzer no longer uses CPU while its tab isn't in view.
- General hardening: tightened internal safety margins and a smaller memory
  footprint in a couple of engine components. Nothing here changes how any
  patch sounds.

**Notes for this build**

- The macOS installer is signed and notarized by Apple, so it installs cleanly.
- The Windows installer is unsigned by design. On first launch, click More info
  and then Run anyway to get past the SmartScreen prompt.
- Runs on macOS 11 or later (Apple silicon and Intel) as AU, VST3, and
  Standalone, and on Windows 10 or later as VST3 and Standalone.
- The Limiter's optional lookahead mode adds a small amount of latency and
  reports it to your host automatically so playback stays in sync. It's off
  by default, so live play stays at zero added latency until you turn it on.
- Known limitation: on a Mac with a Retina laptop screen plus an external
  monitor, the standalone window may not drag across onto the external display
  (a fixed-aspect window plus mixed-resolution quirk in the window system). It
  works normally as a plugin in your DAW. Workaround: set the external display
  as your main display in System Settings, or use SPASynth as a plugin.

## 1.0.8

A small fix for anyone using the on-screen keyboard's QWERTY (computer
keyboard) note input.

- Fixed QWERTY note input stopping after touching a knob or dropdown.
  Previously, playing notes from your computer keyboard would stop working
  the moment you turned any knob or changed any setting, and would only
  resume after clicking directly on a key in the on-screen keyboard. Knobs,
  dropdowns, and toggles no longer take over keyboard focus, so QWERTY play
  keeps working while you tweak the sound.

## 1.0.7

A targeted fix for a serious tester-reported bug, plus extra safety hardening.

- Fixed loud noise bursts in reopened DAW sessions. Reopening a saved session
  could occasionally produce intermittent loud blasts of noise over the
  patch, even while the session sat idle. The cause was a race while the
  session's settings were being restored: the audio engine could briefly run
  on a half-applied mix of old and new settings, and the resulting burst of
  energy then kept re-emerging from the delay's feedback loop. Restoring is
  now properly synchronized with the audio engine, so this cannot happen.
- Effects no longer carry stale energy across an off/on toggle. Switching an
  EQ band or the phaser/flanger off and back on could release a burst of
  sound trapped from before the toggle; those effects now start clean every
  time they are re-enabled.
- New output safety net. As an extra layer of protection, the synth now
  detects and clears any invalid audio state instead of letting it circulate,
  and the final output is capped at a hard ceiling so no malfunction,
  whatever the cause, can produce ear-damaging levels in headphones.

## 1.0.6

A response to tester feedback, focused on levels and safety.

- Reverb levels rebalanced. The reverb's wet signal was running much hotter
  than it should have, which made the MIX knob feel touchy (a little went a
  long way) and let 100 percent mix get loud enough to distort. The wet path
  now sits at a natural level: MIX sweeps smoothly from subtle to full wash,
  and full wet no longer overloads. Factory presets were retuned to match.
  Note: patches you saved with heavy reverb will sound a bit drier than
  before; nudge MIX up to taste.
- RANDOMIZE ALL volume safety. Random patches could occasionally land
  painfully loud, especially in headphones, when several loud settings
  stacked together. Randomize now keeps the combined oscillator level within
  a sensible ceiling (preserving the balance between oscillators, so patches
  stay just as varied in character) and leaves the limiter switched on at
  transparent settings as a safety net. You can switch the limiter off
  afterward if you prefer.

## 1.0.5

A stability and hardening release. We ran a full audit of the code ahead of
launch and fixed every crash, freeze, and reliability issue it turned up;
there are no new features here, just a more solid foundation.

- Fixed a crash when loading a damaged or hand-edited preset file.
- Fixed a possible crash when loading a corrupted WAV file.
- Fixed a rare freeze when a host reports invalid timeline data to the
  arpeggiator.
- Fixed possible crashes when closing the plugin while samples were still
  loading, or while a menu or dialog was open.
- The reported effect tail now includes the Convolve impulse, so bounces and
  freezes no longer cut it short.
- Smoother performance when oversampling is active.

## 1.0.4

Fixes and small improvements from real-world testing feedback.

- Filter 1 now has its own on/off switch, matching Filter 2.
- Reset to Default. A new "Reset to Default" option in the settings menu
  brings every parameter back to its starting point, handy after RANDOMIZE
  ALL or a long tweaking session.
- File browsers now remember the last folder you opened, for both
  sample/wavetable loading and the Convolve impulse browser.
- Fixed the Convolve impulse browser showing valid WAV files as greyed out.
- Tuned the reverb's Decay range so Hall mode no longer produces an overly
  long, uncontrolled-sounding tail at high settings.
- If your library lives on an external drive, reconnecting after unplugging
  it is now much more reliable. Rescan Library offers to point you at a new
  folder instead of silently doing nothing, and sample/wavetable loading
  retries automatically instead of occasionally showing a spurious
  "unrecognized format" error right after a reconnect.

## 1.0.3

A big update to the effects, the voice engine, and sound quality.

**New effects**

- Panic button. If a note ever gets stuck, one click stops all sound and clears
  any held or latched notes. It also responds to your host's panic control.
- Reorderable effects. Drag the effect tabs (there is a grip handle on each one)
  to change the order the effects run in, and the chain order is saved with your
  preset. RANDOMIZE ALL can now shuffle the order too, and the limiter keeps its
  place at the end of the chain.
- Mod tab. A new modulation effect that switches between Phaser and Flanger,
  with rate (free or tempo-synced), depth, feedback, stages, and stereo spread.
- Tremolo / Vibrato tab. Independent tremolo (amplitude) and vibrato (pitch),
  usable together, with shape, rate, depth, and stereo controls.
- Limiter / Maximizer. A loudness stage with drive, ceiling, character modes,
  optional lookahead, and auto-gain, shown on a scrolling gain-reduction meter
  so you can see it work in real time. It sits last in the chain by default.
- Convolve. Use any sound from your library, or your own WAV, as a convolution
  impulse for reverbs, spaces, and creative textures. Browse your library packs
  right from the tab, shape the impulse with pre-delay, decay, and damping, and
  watch it on a live waveform view.
- Reverb algorithms. The reverb now offers Hall, Plate, Chamber, Room, and
  Spring, with pre-delay, size, decay, damping, tail modulation, and tone
  controls.
- Parametric EQ. A new Pro-style EQ with up to 8 bands, draggable nodes (drag
  for frequency and gain, mouse wheel or Cmd/Ctrl-drag for Q, double-click to
  add or remove a band), a live spectrum analyzer, and Clean / Modern / Vintage
  / Tube character modes.

**New voice engine**

- Voice modes. Choose Poly, Mono, Duo, Paraphonic, or Unison from the new VOICE
  button in the top bar, with note priority (Last / High / Low) and unison
  voices, detune, and width.
- Standalone tempo. The standalone app now has its own tempo: set the BPM, tap
  it in, or follow an external MIDI clock, so tempo-synced effects and the
  arpeggiator lock to the right speed without a host.

**Sound quality**

- Oversampling. Run the whole synth at 2x, 4x, or 8x for cleaner, lower-alias
  sound on bright and hard-driven patches. Off by default; choose it from the
  settings menu.

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
