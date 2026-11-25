// Textes français : mêmes clés que en.ts (vérifié à la construction du site).

import type { Dict } from './index';

const fr: Dict = {
  meta: {
    title: 'Mini Moop : un Minimoog Model D simulé composant par composant',
    description:
      'Mini Moop est un synthétiseur gratuit pour macOS et Windows (VST® 3, application autonome et AU sur macOS) qui simule le Minimoog Model D au niveau de ses composants électroniques.',
  },
  nav: {
    how: 'Fonctionnement',
    listen: 'Écouter',
    download: 'Télécharger',
    github: 'GitHub',
    otherLanguage: 'English',
    otherLanguageLabel: 'Read this page in English',
  },
  hero: {
    tagline: 'Un Minimoog Model D, simulé composant par composant',
    lead:
      'Un synthétiseur gratuit pour macOS et Windows, en plugin VST® 3 et en application autonome, ainsi qu’en Audio Unit sur macOS. Au lieu d’imiter le son, il calcule ce que fait chaque transistor, condensateur et résistance du circuit d’origine, des milliers de fois par seconde.',
    compatibility: 'Plateformes et formats',
    platforms: 'Systèmes',
    formats: 'Formats',
    standalone: 'Application autonome',
    download: 'Télécharger',
    source: 'Code source sur GitHub',
    themes: 'Thème du panneau',
    modern: 'Moderne',
    classic: 'Classique',
    panelAlt: 'Le panneau de Mini Moop : contrôleurs, oscillateurs, mixeur, modificateurs, sortie, et le clavier avec ses molettes',
  },
  how: {
    title: 'Comment fonctionne l’émulation',
    intro:
      'La plupart des synthétiseurs logiciels imitent le son d’un instrument avec des recettes réglées à l’oreille. Mini Moop imite le circuit : il reconstruit les schémas de l’original et laisse le son en sortir.',
    cards: [
      {
        title: 'Le circuit, pas une recette',
        text: 'Les oscillateurs, le fameux filtre en échelle, les enveloppes, l’amplificateur, le mixeur et la source de bruit sont décrits par leurs vrais composants : transistors, diodes, condensateurs et résistances, avec les valeurs du manuel de service de Moog.',
      },
      {
        title: 'De vrais composants, de vraies imperfections',
        text: 'Un transistor n’est pas un interrupteur parfait : il dépend de sa température et de petites différences de fabrication. Ces imperfections font le caractère de l’instrument, comme la saturation chaleureuse quand on pousse le mixeur ou le sifflement du filtre à forte résonance.',
      },
      {
        title: 'Chaque voix est un instrument',
        text: 'Chaque voix tire ses propres valeurs de composants, puis elle est étalonnée réglage après réglage en suivant le manuel de service, comme un technicien le ferait sur les potentiomètres d’ajustage d’un vrai Minimoog. Le réglage Unit permet de jouer un autre exemplaire de l’instrument.',
      },
      {
        title: 'Une dérive qui vient de la physique',
        text: 'Les circuits chauffent et refroidissent légèrement au fil du temps, et la hauteur des notes dérive en conséquence, comme sur l’original. Le réglage Drift va de parfaitement stable à une scène surchauffée.',
      },
      {
        title: 'Vérifié face à la documentation',
        text: 'Les circuits sont comparés à ngspice, un simulateur de circuits utilisé par les électroniciens, et chaque exemplaire passe les contrôles du manuel de service : accord, plages et niveaux.',
      },
      {
        title: 'Un instrument précis',
        text: 'Mini Moop simule la révision 2 de la carte oscillateurs du Model D, l’« ancienne carte » avec un réseau de transistors CA3046 (numéros de série 1237 à 10174).',
      },
    ],
    featuresTitle: 'En plus de l’original',
    features: [
      'Monophonique comme l’original, ou polyphonique de 2 à 8 voix',
      'Réglage de qualité : suréchantillonnage 2x, 4x ou 8x',
      'Une banque Factory de 52 presets, et vos propres banques de sons à créer, importer et exporter',
      'Deux thèmes de panneau, Moderne et Classique, sur un panneau vectoriel redimensionnable',
      'Amplitude du pitch bend réglable, profondeur de modulation mémorisée, boucle de la sortie vers l’entrée externe',
      'Binaires universels pour les Mac Apple Silicon et Intel, et binaires x64 pour Windows',
    ],
  },
  listen: {
    title: 'Écouter',
    intro: 'Les seuls effets ajoutés à certains échantillons sont la réverbération et le delay.',
    soon: 'Enregistrement bientôt disponible',
    play: 'Lire',
    pause: 'Mettre en pause',
    seek: 'Parcourir l’enregistrement',
    playError: 'Impossible de lire cet enregistrement.',
    waveformError: 'Impossible d’afficher la forme d’onde.',
    samples: {
      'sample-1': 'Une basse pulsée et très résonante, avec une boucle de feedback (banque Factory).',
      'sample-2': 'Une basse dense formée de trois dents de scie légèrement désaccordées, sculptée par une enveloppe de filtre marquée (inspiré par CA-72).',
      'sample-3': 'Une ligne de basse en dents de scie, épaulée par un triangle plus grave et un filtre résonant (inspiré par Arturia Mini V4).',
      'sample-4': 'Un son percussif et métallique mêlant rectangle aigu, triangle grave et bruit (banque Factory).',
      'sample-5': 'Un lead glissant qui mêle onde carrée aiguë et triangle grave dans un filtre résonant (banque Factory).',
      'sample-6': 'Une séquence de basse en dents de scie avec glide et un filtre au caractère résonant (inspiré par Arturia Mini V4).',
    },
  },
  download: {
    title: 'Télécharger',
    intro: 'Gratuit et open source. Choisissez votre plateforme : un ZIP contient l’application et les plugins.',
    version: 'Version {version}, publiée le {date}',
    noRelease: 'La première version arrive bientôt : les boutons ouvrent la page des versions sur GitHub.',
    downloadZip: 'Télécharger le ZIP',
    archivePending: 'Archive pas encore ajoutée — voir la version',
    platforms: {
      macos: {
        name: 'macOS',
        detail: 'Application autonome + VST3 + Audio Unit',
        requirements: 'macOS 11 ou plus récent · Apple Silicon et Intel',
        install: 'Décompressez le ZIP. Placez Mini Moop.app dans /Applications, Mini Moop.vst3 dans ~/Library/Audio/Plug-Ins/VST3 et Mini Moop.component dans ~/Library/Audio/Plug-Ins/Components. Créez ces dossiers de plugins si nécessaire, puis redémarrez votre logiciel de musique.',
      },
      windows: {
        name: 'Windows',
        detail: 'Application autonome + VST3',
        requirements: 'Windows 10 ou plus récent · x64',
        install: 'Décompressez le ZIP avant de lancer Mini Moop.exe. Copiez le dossier Mini Moop.vst3 entier dans C:\\Program Files\\Common Files\\VST3 (une autorisation administrateur peut être nécessaire), puis redémarrez votre logiciel de musique. Les Audio Units sont réservées à macOS.',
      },
    },
    installIntro: 'L’application autonome se joue avec un clavier MIDI, sans logiciel hôte. Utilisez le VST3 dans un logiciel de musique compatible, ou l’Audio Unit dans Logic Pro, GarageBand et MainStage.',
    quarantineTitle: 'macOS : avant le premier lancement',
    quarantineText:
      'Mini Moop n’est pas signé avec un certificat de développeur Apple, donc macOS le bloque après un téléchargement. Retirez une fois l’attribut de quarantaine, dans l’application Terminal, sur les éléments que vous avez installés :',
    copy: 'Copier',
    copied: 'Copié',
    allReleases: 'Toutes les versions et leurs notes',
  },
  footer: {
    license: 'Mini Moop est un logiciel libre, sous licence GNU AGPLv3 ou ultérieure.',
    trademark:
      'Minimoog et Model D sont des marques de Moog Music Inc. Mini Moop est un projet personnel indépendant, sans lien avec Moog Music ni approuvé par elle, et n’utilise aucun logo ni visuel d’origine.',
    builtWith: 'Réalisé avec JUCE.',
    vstTrademark: 'VST est une marque déposée de Steinberg Media Technologies GmbH.',
    auTrademark: 'Audio Units et le logo Audio Units sont des marques d’Apple Inc.',
    source: 'Code source',
  },
};

export default fr;
