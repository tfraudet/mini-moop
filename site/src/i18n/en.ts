// English texts: the reference. fr.ts must have exactly the same keys
// (checked when the site is built, see index.ts).

const en = {
  meta: {
    title: 'Mini Moop: a Minimoog Model D simulated component by component',
    description:
      'Mini Moop is a free synthesizer plugin for macOS (VST® 3, AU, standalone app) that simulates the Minimoog Model D at the level of its electronic components.',
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
      'A free synthesizer for macOS, as a VST® 3 plugin, an Audio Unit and a standalone app. Instead of imitating the sound, it calculates what every transistor, capacitor and resistor of the original circuit does, many thousands of times per second.',
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
      'Universal binaries for Apple Silicon and Intel Macs',
    ],
  },
  listen: {
    title: 'Listen',
    intro: 'Recorded straight from Mini Moop, with no added effects: what you hear is the simulated circuit.',
    soon: 'Recording coming soon',
    samples: {
      fatBass: 'Three oscillators stacked through the ladder filter',
      soloLead: 'The classic monophonic lead',
      whistle: 'The filter singing on its own at high emphasis, played as an oscillator',
      sciFi: 'Sound effects in the spirit of 1970s science fiction',
      polyBrass: 'Poly mode: a brass chord, each voice a slightly different unit',
      osc3Wobble: 'Oscillator 3 used as a modulation source',
    },
  },
  download: {
    title: 'Download',
    intro: 'Free and open source. Pick the format your music software uses.',
    version: 'Version {version}, released on {date}',
    noRelease: 'The first release is on its way: the buttons open the releases page on GitHub.',
    installIn: 'Install in',
    formats: {
      app: { name: 'Standalone app', detail: 'Plays on its own with any MIDI keyboard, no music software needed' },
      vst3: { name: 'VST3 plugin', detail: 'Ableton Live, Bitwig Studio, Cubase, Reaper, Studio One…' },
      au: { name: 'Audio Unit', detail: 'Logic Pro, GarageBand, MainStage…' },
    },
    requirements: 'macOS 11 or later, on an Apple Silicon or Intel Mac.',
    quarantineTitle: 'Before the first launch',
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
