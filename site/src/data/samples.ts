// The sound samples of the Listen section, in display order. The MP3 files go
// in public/audio/ (see its README); a sample whose file is missing is shown as
// "coming soon". `text` is a key of listen.samples in src/i18n/en.ts and fr.ts.

import type en from '../i18n/en';

export interface Sample {
  file: string;
  preset: string; // name of the preset, shown as is in both languages
  text: keyof typeof en.listen.samples;
}

export const samples: Sample[] = [
  { file: 'sample-1.mp3', preset: 'Bubble Bass', text: 'sample-1' },
  { file: 'sample-2.mp3', preset: 'Saw Monolith', text: 'sample-2' },
  { file: 'sample-3.mp3', preset: 'Let\'s Groove Bass Bridge', text: 'sample-3' },
  { file: 'sample-4.mp3', preset: 'Steel Wound', text: 'sample-4' },
  { file: 'sample-5.mp3', preset: 'Classic Stanley', text: 'sample-5' },
  { file: 'sample-6.mp3', preset: 'Goom Ba', text: 'sample-6' },
];
