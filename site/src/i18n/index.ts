import en from './en';
import fr from './fr';

export const languages = ['en', 'fr'] as const;
export type Lang = (typeof languages)[number];

/** The shape of en.ts with every text widened to `string`: fr.ts is typed with it. */
type Widen<T> = T extends string
  ? string
  : T extends readonly (infer U)[]
    ? Widen<U>[]
    : { [K in keyof T]: Widen<T[K]> };
export type Dict = Widen<typeof en>;

const dicts: Record<Lang, Dict> = { en, fr };

/** Keys present in one dictionary and not in the other ("a.b.c"); arrays must have the same length. */
function keyMismatches(a: unknown, b: unknown, path = ''): string[] {
  if (typeof a === 'string' || typeof b === 'string')
    return typeof a === typeof b ? [] : [path || '(root)'];
  if (Array.isArray(a) || Array.isArray(b)) {
    if (!Array.isArray(a) || !Array.isArray(b) || a.length !== b.length) return [path];
    return a.flatMap((item, i) => keyMismatches(item, b[i], `${path}[${i}]`));
  }
  const ao = a as Record<string, unknown>, bo = b as Record<string, unknown>;
  const keys = new Set([...Object.keys(ao ?? {}), ...Object.keys(bo ?? {})]);
  return [...keys].flatMap((k) => {
    const p = path ? `${path}.${k}` : k;
    return k in (ao ?? {}) && k in (bo ?? {}) ? keyMismatches(ao[k], bo[k], p) : [p];
  });
}

// Astro does not type-check during `astro build`: fail the build here instead.
for (const lang of languages) {
  const bad = keyMismatches(en, dicts[lang]);
  if (bad.length) throw new Error(`i18n: ${lang} does not match en on: ${bad.join(', ')}`);
}

export function t(lang: Lang): Dict {
  return dicts[lang];
}

/** Replaces {name} placeholders. */
export function fill(text: string, values: Record<string, string>): string {
  return text.replace(/\{(\w+)\}/g, (m, k: string) => values[k] ?? m);
}

export const otherLang = (lang: Lang): Lang => (lang === 'en' ? 'fr' : 'en');
