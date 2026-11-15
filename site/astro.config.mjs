// @ts-check
import { defineConfig } from 'astro/config';

// Published by .github/workflows/pages.yml at https://tfraudet.github.io/mini-moop/
export default defineConfig({
  site: 'https://tfraudet.github.io',
  base: '/mini-moop',
  trailingSlash: 'always',
  i18n: {
    defaultLocale: 'en',
    locales: ['en', 'fr'],
    routing: { prefixDefaultLocale: false },
  },
});
