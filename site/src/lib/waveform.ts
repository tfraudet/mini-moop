export function waveformHeights(channels: Float32Array[], barCount: number): number[] {
  const sampleCount = channels[0]?.length ?? 0;
  if (!Number.isInteger(barCount) || barCount <= 0 || !sampleCount ||
      channels.some((channel) => channel.length !== sampleCount)) {
    throw new Error('Invalid audio data or waveform bar count');
  }

  const amplitudes = Array.from({ length: barCount }, (_, bar) => {
    const start = Math.floor(bar * sampleCount / barCount);
    const end = Math.floor((bar + 1) * sampleCount / barCount);
    if (start === end) return 0;
    let energy = 0;
    for (const channel of channels) {
      for (let i = start; i < end; i++) energy += channel[i] * channel[i];
    }
    return Math.sqrt(energy / ((end - start) * channels.length));
  });
  const max = Math.max(...amplitudes);
  return amplitudes.map((amplitude) => max ? Math.round(4 + 92 * amplitude / max) : 4);
}
