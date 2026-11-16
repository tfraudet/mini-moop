// The latest GitHub Release, read once when the site is built. The Pages
// workflow runs again when a release is published, so the buttons follow it.
// Without a release (or if GitHub can't be reached) every button falls back
// to the releases page, and the build still succeeds.

export const REPO_URL = 'https://github.com/tfraudet/mini-moop';
// MINIMOOP_RELEASE_REPO=owner/name reads another repo's releases (to test the asset matching)
const REPO = process.env.MINIMOOP_RELEASE_REPO ?? 'tfraudet/mini-moop';
const RELEASES_URL = `https://github.com/${REPO}/releases`;
const LATEST_URL = `${RELEASES_URL}/latest`;

export type Format = 'app' | 'vst3' | 'au';
export const platforms = ['macos', 'windows'] as const;
export type Platform = (typeof platforms)[number];

/** How a release asset is recognised, from its file name (case-insensitive).
    Matches the complete platform archives produced by the Build workflow. */
const patterns: Record<Platform, RegExp> = {
  macos: /^MiniMoop-[0-9.]+-universal-macos\.zip$/i,
  windows: /^MiniMoop-[0-9.]+-x64-windows\.zip$/i,
};

export interface Release {
  version: string | null;  // null: no release found
  date: Date | null;
  url: string;      // the release page, or the releases list
  all: string;      // the releases list
  links: Record<Platform, string | null>; // null: archive not uploaded yet
}

interface GitHubAsset { name: string; browser_download_url: string }
interface GitHubRelease { tag_name: string; html_url: string; published_at: string; assets: GitHubAsset[] }

let cached: Promise<Release> | undefined;

/** Fetched once per build, shared by both languages. */
export function getLatestRelease(): Promise<Release> {
  return (cached ??= fetchLatest());
}

async function fetchLatest(): Promise<Release> {
  const fallback: Release = {
    version: null,
    date: null,
    url: LATEST_URL,
    all: RELEASES_URL,
    links: { macos: null, windows: null },
  };
  try {
    const headers: Record<string, string> = { Accept: 'application/vnd.github+json' };
    if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
    const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, { headers });
    if (!res.ok) {
      console.warn(`[release] ${REPO}: no latest release (HTTP ${res.status}), links go to ${LATEST_URL}`);
      return fallback;
    }
    const rel = (await res.json()) as GitHubRelease;
    const links = { ...fallback.links };
    for (const platform of platforms) {
      const asset = rel.assets.find((a) => patterns[platform].test(a.name));
      if (asset) {
        links[platform] = asset.browser_download_url;
      } else {
        console.warn(`[release] ${rel.tag_name}: no ${platform} archive, its card opens the release page`);
      }
    }
    return {
      version: rel.tag_name.replace(/^v/i, ''),
      date: new Date(rel.published_at),
      url: rel.html_url,
      all: RELEASES_URL,
      links,
    };
  } catch (err) {
    console.warn(`[release] ${REPO}: ${err}, links go to ${LATEST_URL}`);
    return fallback;
  }
}
