# Mini Moop presentation site

The site published at https://tfraudet.github.io/mini-moop/ (English at `/`, French at `/fr/`). It is a static [Astro](https://astro.build) site, built and deployed by `.github/workflows/pages.yml`. It is independent from the plugin build: the C++ project ignores this folder, and the plugin workflow does not run when only the site changes.

## Working on it

Needs Node.js 22.23 or later.

```sh
cd site
npm ci              # once
npm run dev         # live preview at http://localhost:4321/mini-moop/
npm run build       # static site in site/dist
npm run preview     # serves site/dist
```

| What | Where |
|---|---|
| Texts | `src/i18n/en.ts` (reference) and `src/i18n/fr.ts`. Both must have the same keys: the build stops on a missing or extra key. |
| Page sections | `src/components/` (`Hero`, `HowItWorks`, `Listen`, `Download`), assembled by `Home.astro`; header and footer in `src/layouts/Base.astro` |
| Styles | `src/styles/global.css` |
| Sound samples | `src/data/samples.ts` and the files in `public/audio/` (see below) |
| Panel screenshots | `src/assets/panel-modern.jpg`, `panel-classic.jpg` (see below) |

## Sound samples

The Listen section shows the samples listed in `src/data/samples.ts`, in that order. Each one is played from `public/audio/<file>`; while the file is missing, its card shows "Recording coming soon". To add a recording, drop the file in `public/audio/` with the expected name. To use another preset, change its line in `samples.ts`, and its description in `listen.samples` in both `en.ts` and `fr.ts`.

| File | Preset |
|---|---|
| `01-fat-bass.mp3` | Fat Bass |
| `02-solo-lead.mp3` | Solo Lead |
| `03-self-osc-whistle.mp3` | Self-Osc Whistle |
| `04-70s-french-sci-fi-movie.mp3` | '70s French Sci-Fi Movie |
| `05-poly-brass.mp3` | Poly Brass |
| `06-osc3-wobble.mp3` | Osc 3 Wobble |

Recording advice:
- No effects (reverb, delay, compression): what is heard should be the simulated circuit. Say so in `listen.intro` if you add some.
- 10 to 25 seconds, a short phrase that shows the sound, ending on a natural release rather than a cut.
- Peaks below -1 dBFS and a similar loudness for all six (about -16 LUFS), so that the visitor does not have to change the volume between samples.
- MP3 at 160 to 192 kbit/s, or AAC (`.m4a`, `afconvert -f m4af -d aac -b 192000 in.wav out.m4a` on macOS). With AAC, change the extension in `samples.ts`. Keep each file under 1 MB if possible: they are committed to the repository. WAV files are ignored by git (`.gitignore`).

## Panel screenshots

Rendered by the snapshot tool at scale 2, then converted to JPEG (the PNGs weigh several megabytes). Astro makes the resized WebP versions at build time.

```sh
S=build/release/MiniMoopSnapshot_artefacts/Release/MiniMoopSnapshot
for theme in modern classic; do
  $S /tmp/panel-$theme.png 2 $theme
  sips -s format jpeg -s formatOptions 70 /tmp/panel-$theme.png --out site/src/assets/panel-$theme.jpg
done
```

## Downloads and releases

The two platform download cards are filled at build time from the repository's latest GitHub Release (`src/lib/release.ts`). Upload the archives produced by the Build workflow to that release with these names (matching is case-insensitive):

```
MiniMoop-<version>-universal-macos.zip
MiniMoop-<version>-x64-windows.zip
```

The macOS ZIP contains the app, VST3 and AU; the Windows ZIP contains the executable and VST3. Actions artifacts alone are not public release downloads: upload the inner ZIPs, not the artifact wrappers. Old per-format ZIPs are not matched.

Publishing, editing or deleting a release rebuilds the site, so the cards follow the latest release. When an archive is missing, its card says so and opens the release page instead of claiming to download a ZIP. After adding assets to an already published release, manually run the Pages workflow (or edit the release to trigger a rebuild). To check the matching against another repository's releases: `MINIMOOP_RELEASE_REPO=owner/name npm run build`. Run `npm test` for the asset-matching and missing-release checks.

## Deployment

`.github/workflows/pages.yml` builds the site and deploys it with GitHub's own Pages actions (no `gh-pages` branch). It runs on every push to `main` that touches `site/`, on release events and by hand from the Actions tab. It needs **Settings > Pages > Source: GitHub Actions**, set once.
