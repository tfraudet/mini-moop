// The sound samples of the Listen section, in display order. The MP3 files go
// in public/audio/ (see its README); a sample whose file is missing is shown as
// "coming soon". `text` is a key of listen.samples in src/i18n/en.ts and fr.ts.

import type en from '../i18n/en';

export interface Sample {
  file: string;
  preset: string; // name of the Factory preset, shown as is in both languages
  text: keyof typeof en.listen.samples;
}

export const samples: Sample[] = [
  { file: '01-fat-bass.mp3', preset: 'Fat Bass', text: 'fatBass' },
  { file: '02-solo-lead.mp3', preset: 'Solo Lead', text: 'soloLead' },
  { file: '03-self-osc-whistle.mp3', preset: 'Self-Osc Whistle', text: 'whistle' },
  { file: '04-70s-french-sci-fi-movie.mp3', preset: "'70s French Sci-Fi Movie", text: 'sciFi' },
  { file: '05-poly-brass.mp3', preset: 'Poly Brass', text: 'polyBrass' },
  { file: '06-osc3-wobble.mp3', preset: 'Osc 3 Wobble', text: 'osc3Wobble' },
];
