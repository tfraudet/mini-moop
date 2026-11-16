import assert from 'node:assert/strict';
import { test } from 'node:test';

const release = {
  tag_name: 'v1.2.0',
  html_url: 'https://github.com/tfraudet/mini-moop/releases/tag/v1.2.0',
  published_at: '2026-11-16T12:00:00Z',
};
const asset = (name: string) => ({
  name,
  browser_download_url: `https://example.com/${name}`,
});

// Isolate the per-build cache for each scenario.
function loadRelease(scenario: string): Promise<typeof import('./release')> {
  return import(`./release.ts?${scenario}`);
}

test('links each platform to its ZIP and shares the fetch between languages', async (t) => {
  const mac = asset('MiniMoop-1.2.0-universal-macos.zip');
  const windows = asset('MiniMoop-1.2.0-x64-windows.zip');
  const fetchMock = t.mock.method(globalThis, 'fetch', async () => Response.json({
    ...release,
    assets: [
      asset('MiniMoop-1.2.0-macOS-App.zip'),
      asset('MiniMoop-1.2.0-x64-windows.zip.sha256'),
      mac,
      windows,
    ],
  }));
  const { getLatestRelease } = await loadRelease('both-platforms');
  const [en, fr] = await Promise.all([getLatestRelease(), getLatestRelease()]);
  assert.equal(en, fr);
  assert.equal(en.version, '1.2.0');
  assert.equal(en.links.macos, mac.browser_download_url);
  assert.equal(en.links.windows, windows.browser_download_url);
  assert.equal(fetchMock.mock.callCount(), 1);
});

test('missing platform stays unavailable without using a legacy per-format ZIP', async (t) => {
  t.mock.method(globalThis, 'fetch', async () => Response.json({
    ...release,
    assets: [
      asset('MINIMOOP-1.2.0-UNIVERSAL-MACOS.ZIP'),
      asset('MiniMoop-1.2.0-windows-VST3.zip'),
    ],
  }));
  const { getLatestRelease } = await loadRelease('missing-platform');
  const result = await getLatestRelease();
  assert.ok(result.links.macos?.endsWith('UNIVERSAL-MACOS.ZIP'));
  assert.equal(result.links.windows, null);
  assert.equal(result.url, release.html_url);
});

test('a release with no assets leaves both archives unavailable', async (t) => {
  t.mock.method(globalThis, 'fetch', async () => Response.json({ ...release, assets: [] }));
  const { getLatestRelease } = await loadRelease('empty-release');
  const result = await getLatestRelease();
  assert.deepEqual(result.links, { macos: null, windows: null });
  assert.equal(result.url, release.html_url);
});

test('no release falls back to the latest release page without download links', async (t) => {
  t.mock.method(globalThis, 'fetch', async () => new Response(null, { status: 404 }));
  const { getLatestRelease } = await loadRelease('no-release');
  const result = await getLatestRelease();
  assert.equal(result.version, null);
  assert.deepEqual(result.links, { macos: null, windows: null });
  assert.ok(result.url.endsWith('/releases/latest'));
});
