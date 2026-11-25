// English texts: the reference. fr.ts must have exactly the same keys
// (checked when the site is built, see index.ts).

const en = {
  meta: {
    title: 'Mini Moop: a Minimoog Model D simulated component by component',
    description:
      'Mini Moop is a free synthesizer for macOS and Windows (VST® 3, standalone app, and AU on macOS) that simulates the Minimoog Model D at the level of its electronic components.',
  },
  nav: {
    how: 'How it works',
    listen: 'Listen',
    download: 'Download',
    github: 'GitHub',
    otherLanguage: 'Français',
    otherLanguageLabel: 'Lire cette page en français',
  },
  hero: {
    tagline: 'A Minimoog Model D, simulated component by component',
    lead:
      'A free synthesizer for macOS and Windows, as a VST® 3 plugin and a standalone app, plus an Audio Unit on macOS. Instead of imitating the sound, it calculates what every transistor, capacitor and resistor of the original circuit does, many thousands of times per second.',
    download: 'Download',
    source: 'Source code on GitHub',
    themes: 'Panel theme',
    modern: 'Modern',
    classic: 'Classic',
    panelAlt: 'The Mini Moop panel: controllers, oscillator bank, mixer, modifiers, output, and the keyboard with its wheels',
  },
  how: {
    title: 'How the emulation works',
    intro:
      'Most software synthesizers imitate the sound of an instrument with recipes tuned by ear. Mini Moop imitates the circuit: it rebuilds the schematics of the original and lets the sound come out of them.',
    cards: [
      {
        title: 'The circuit, not a recipe',
        text: 'The oscillators, the famous ladder filter, the envelopes, the amplifier, the mixer and the noise source are each described by their actual parts: transistors, diodes, capacitors and resistors, with the values of Moog’s service manual.',
      },
      {
        title: 'Real parts, real imperfections',
        text: 'A transistor is not a perfect switch: it depends on its temperature and on small manufacturing differences. These imperfections give the instrument its character, such as the warm overdrive when the mixer is pushed or the filter whistling at high emphasis.',
      },
      {
        title: 'Every voice is its own instrument',
        text: 'Each voice draws its own component values, then is calibrated trimmer by trimmer, following the service manual, as a technician would. The Unit setting lets you play another example of the instrument.',
      },
      {
        title: 'Drift that comes from physics',
        text: 'The circuits warm up and cool down slightly over time, and the pitch drifts as a consequence, as on the original. The Drift setting goes from perfectly stable to a hot stage.',
      },
      {
        title: 'Checked against the documentation',
        text: 'The circuits are compared with ngspice, a circuit simulator used by electronics engineers, and every unit goes through the service manual’s own checks: tuning, ranges and levels.',
      },
      {
        title: 'One precise instrument',
        text: 'Mini Moop simulates revision 2 of the Model D’s oscillator board, the “old board” with a CA3046 transistor array (serial numbers 1237 to 10174).',
      },
    ],
    featuresTitle: 'On top of the original',
    features: [
      'Monophonic like the original, or polyphonic with 2 to 8 voices',
      'Quality setting: 2x, 4x or 8x oversampling',
      'A Factory bank of 52 presets, and your own sound banks to create, import and export',
      'Two panel themes, Modern and Classic, on a resizable vector panel',
      'Adjustable pitch-bend range, stored modulation depth, output-to-input feedback patch',
      'Universal binaries for Apple Silicon and Intel Macs, and x64 binaries for Windows',
    ],
  },
  listen: {
    title: 'Listen',
    intro: 'The only added effects on some samples are reverb and delay.',
    soon: 'Recording coming soon',
    play: 'Play',
    pause: 'Pause',
    seek: 'Seek through recording',
    playError: 'Unable to play this recording.',
    waveformError: 'Unable to display the waveform.',
    samples: {
      'sample-1': 'A pulsing, highly resonant bass with a feedback edge (Factory bank).',
      'sample-2': 'A dense bass built from three slightly detuned sawtooths, shaped by a punchy filter envelope (inpired by CA-72).',
      'sample-3': 'A sawtooth bass line joined by a lower triangle and a resonant filter (inspired by Arturia Mini V4).',
      'sample-4': 'A percussive, metallic tone combining a high rectangle wave, a low triangle and noise (Factory bank).',
      'sample-5': 'A gliding lead combining a high square wave and a lower triangle through a resonant filter (Factory bank).',
      'sample-6': 'A gliding sawtooth bass sequence with a resonant filter (inspred by Arturia Mini V4).',
    },
  },
  download: {
    title: 'Download',
    intro: 'Free and open source. Choose your platform: one ZIP contains the app and plugins.',
    version: 'Version {version}, released on {date}',
    noRelease: 'The first release is on its way: the buttons open the releases page on GitHub.',
    downloadZip: 'Download ZIP',
    archivePending: 'Archive not uploaded yet — view release',
    platforms: {
      macos: {
        name: 'macOS',
        detail: 'Standalone app + VST3 + Audio Unit',
        requirements: 'macOS 11 or later · Apple Silicon & Intel',
        install: 'Extract the ZIP. Move Mini Moop.app to /Applications, Mini Moop.vst3 to ~/Library/Audio/Plug-Ins/VST3, and Mini Moop.component to ~/Library/Audio/Plug-Ins/Components. Create these plugin folders if needed, then restart your music software.',
      },
      windows: {
        name: 'Windows',
        detail: 'Standalone app + VST3',
        requirements: 'Windows 10 or later · x64',
        install: 'Extract the ZIP before launching Mini Moop.exe. Copy the entire Mini Moop.vst3 folder to C:\\Program Files\\Common Files\\VST3 (administrator permission may be needed), then restart your music software. Audio Units are only available on macOS.',
      },
    },
    installIntro: 'The standalone app works with a MIDI keyboard, without a host. Use VST3 in compatible music software, or Audio Unit in Logic Pro, GarageBand and MainStage.',
    quarantineTitle: 'macOS: before the first launch',
    quarantineText:
      'Mini Moop is not signed with an Apple developer certificate, so macOS blocks it after a download. Clear the quarantine flag once, in the Terminal app, on the bundles you installed:',
    copy: 'Copy',
    copied: 'Copied',
    allReleases: 'All releases and release notes',
  },
  footer: {
    license: 'Mini Moop is free software, licensed under the GNU AGPLv3 or later.',
    trademark:
      'Minimoog and Model D are trademarks of Moog Music Inc. Mini Moop is an independent personal project, not affiliated with or endorsed by Moog Music, and uses no original logo or artwork.',
    builtWith: 'Built with JUCE.',
    vstTrademark: 'VST is a registered trademark of Steinberg Media Technologies GmbH.',
    auTrademark: 'Audio Units and the Audio Units logo are trademarks of Apple Inc.',
    source: 'Source code',
  },
};

export default en;
