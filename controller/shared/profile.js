// Hype Dial: what the controllers show and the CC each control sends (channel 1).
// src/app.cpp binds the same numbers in kBind.
export const profile = {
  name: 'Hype Dial',
  accent: '#e4ff1a',
  pages: [
    {key: 'P', label: 'PLAY', title: 'Work the floor', color: '#E4FF1A', controls: [
      {id: 'build', cc: 30, kind: 'trigger', label: 'BUILD', key: 'A'},
      {id: 'stutter', cc: 31, kind: 'trigger', label: 'STUTTER', key: 'S'},
      {id: 'fill', cc: 32, kind: 'trigger', label: 'FILL', key: 'D'},
      {id: 'stop', cc: 33, kind: 'trigger', label: 'STOP', key: 'F'},
      {id: 'deck', kind: 'xy', label: 'Energy × Filter', x: 'energy', y: 'filter'},
      {id: 'status', cc: 27, kind: 'readout', label: 'Now', options: ['LIVE', 'BUILD', 'DROP', 'STOP']},
    ]},
    {key: 'G', label: 'GROOVE', title: 'The groove', color: '#FF5A1F', controls: [
      {id: 'energy', cc: 20, kind: 'range', label: 'Energy', min: 0, max: 10, step: 1, def: 2},
      {id: 'genre', cc: 21, kind: 'choice', label: 'Genre', options: ['HOUSE', 'TECHNO', 'HIPHOP', 'DNB', 'FUNK', 'LATIN'], def: 0},
      {id: 'tune', cc: 22, kind: 'choice', label: 'Tune', options: ['1', '2', '3', '4', '5', '6', '7', '8'], def: 0},
      {id: 'kill', cc: 23, kind: 'toggle', label: 'Kick kill', def: 0},
    ]},
    {key: 'FX', label: 'FX', title: 'The mix', color: '#19E3FF', controls: [
      {id: 'filter', cc: 24, kind: 'range', label: 'Filter', min: -64, max: 63, def: 0,
        format: v => v === 0 ? 'Clean' : v < 0 ? `LP ${Math.round(-v * 100 / 64)}%` : `HP ${Math.round(v * 100 / 63)}%`},
      {id: 'roll', cc: 25, kind: 'choice', label: 'Roll', options: ['OFF', '1/4', '1/8', '1/16', '1/32'], def: 0},
      {id: 'level', cc: 26, kind: 'range', label: 'Level', min: 0, max: 100, unit: '%', def: 65},
      {id: 'backing', cc: 28, kind: 'range', label: 'Backing', min: 0, max: 150, unit: '%', def: 100},
    ]},
  ],
};
