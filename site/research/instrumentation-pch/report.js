import data from './data.json'
import dataUrl from './data.json?url'

const get = id => document.getElementById(id)
const escape = value => String(value).replace(/[&<>"']/g,
  char => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[char]))
const percent = value => `${value > 0 ? '+' : ''}${value.toFixed(1)}%`
const color = value => value > 0 ? 'positive' : value < 0 ? 'negative' : ''
const duration = seconds => `${Math.floor(Math.round(seconds) / 60)}m ${String(Math.round(seconds) % 60).padStart(2, '0')}s`
const comparisons = {
  'master-disabled': ['master', 'prototype-disabled'],
  'master-targeted': ['master', 'targeted-current'],
  'disabled-targeted': ['prototype-disabled', 'targeted-current'],
  'earlier-broad': ['baseline', 'cached'],
}
const names = {
  master: 'Base master · 1d72e76',
  'prototype-disabled': 'Prototype · PCH/cache off',
  'targeted-current': 'Prototype · multi-source PCH + cache',
  'ci-baseline': 'Earlier prototype · PCH/cache off',
  baseline: 'Earlier broad PCH · cache off',
  cached: 'Earlier broad PCH · cache on',
}
const runs = ['master', 'prototype-disabled', 'targeted-current'].map(mode =>
  data.fullSuite.runs.find(run => run.mode === mode))
const reference = runs[0]
const relative = (before, after) => {
  const gain = (before - after) / before * 100
  return `<span class="${color(gain)}">${Math.abs(gain).toFixed(1)}% ${gain >= 0 ? 'faster' : 'slower'}</span>`
}
get('build-results').innerHTML = runs.map(run => `<tr>
  <td>${names[run.mode]}</td><td class="numeric">${run.build_seconds.toFixed(1)}s</td>
  <td class="numeric">${run.mode === 'master' ? 'Baseline' : relative(reference.build_seconds, run.build_seconds)}</td></tr>`).join('')
const disabled = runs[1]
const targeted = runs[2]
get('decision-summary').textContent = `The baseline is base master 1d72e76: ${reference.build_seconds.toFixed(1)}s for a clean package build. The prototype took ${disabled.build_seconds.toFixed(1)}s with PCH/cache off, and ${targeted.build_seconds.toFixed(1)}s with the multi-source PCH policy. Configuration and CTest are excluded from these build times.`
const phaseSum = mode => data.files.reduce((sum, file) => sum + file.policies[mode].instrumentation, 0)
get('disabled-phases').innerHTML = `<tr><td>Master</td><td class="numeric">${phaseSum('master').toFixed(1)}s</td></tr><tr><td>Prototype · PCH/cache off</td><td class="numeric">${phaseSum('prototype-disabled').toFixed(1)}s</td></tr>`
const probe = data.disabledPathProbe
get('disabled-probe').textContent = `A separate check alternated the two binaries over identical C++20 commands for five existing metadata test sources, for three rounds on one pinned CPU. Median sequence: master ${probe.master.median_seconds.toFixed(3)}s; prototype, PCH disabled ${probe['prototype-disabled'].median_seconds.toFixed(3)}s (${Math.abs(probe.change_percent).toFixed(1)}% ${probe.change_percent > 0 ? 'slower' : 'faster'}). Generated reflection contents matched. These measurements do not establish a material disabled-path regression.`
const earlierRuns = data.fullSuite.earlier_runs
get('suite-results').innerHTML = ['ci-baseline', 'baseline', 'cached', 'targeted'].map(mode => {
  const run = earlierRuns.find(run => run.mode === mode)
  return `<tr><td>${mode === 'targeted' ? 'Earlier multi-source PCH + cache' : names[mode]}</td><td class="numeric">${run.build_seconds.toFixed(1)}s</td><td class="numeric">${run.test_seconds.toFixed(1)}s</td><td class="numeric">${run.total_seconds.toFixed(1)}s</td></tr>`
}).join('')
get('coverage').textContent = `${data.coverage.files} distinct .cpp files across ${data.coverage.compiledContexts} matched target/standard builds, including all ${data.coverage.instrumentedContexts} instrumentation calls. Each source file gets equal weight in the percentile distribution.`
get('download-data').href = dataUrl

const quantile = (values, q) => {
  const index = q * (values.length - 1)
  const lower = Math.floor(index)
  return values[lower] + (values[Math.ceil(index)] - values[lower]) * (index - lower)
}
const profiles = {standard: 'standard headers', gtest: 'GTest', functional: 'GTest + functional.hpp', json: 'json_match.hpp'}
get('compiler-results').innerHTML = data.compilerProfiles.map(profile => {
  const median = (mode, key) => quantile(profile.rounds.map(round => round[mode][key]).sort((a, b) => a - b), .5)
  const before = median('baseline', 'sequence_seconds')
  const after = median('pch', 'sequence_seconds')
  return `<tr><td>${escape(profile.compiler)} / ${profiles[profile.prefix_profile] || escape(profile.prefix_profile)}</td><td class="numeric">${median('baseline', 'second_file_seconds').toFixed(3)} → ${median('pch', 'second_file_seconds').toFixed(3)}</td><td class="numeric">${before.toFixed(3)} → ${after.toFixed(3)}</td><td class="numeric positive">${((before - after) / before * 100).toFixed(1)}%</td></tr>`
}).join('')
let selected = []
function renderFiles() {
  const query = get('file-search').value.trim().toLowerCase()
  const rows = selected.filter(row => row.source.toLowerCase().includes(query))
  const order = {
    least: (a, b) => a.gain - b.gain,
    most: (a, b) => b.gain - a.gain,
    name: (a, b) => a.source.localeCompare(b.source),
    time: (a, b) => b.before - a.before,
  }[get('sort').value]
  rows.sort(order)
  get('file-count').textContent = `${rows.length} of ${selected.length} files`
  get('file-results').innerHTML = rows.map(row => `<tr>
    <td><code>${escape(row.source)}</code><span class="variants">${row.contexts.map(escape).join(' · ')}</span></td>
    <td class="numeric">${row.before.toFixed(3)}</td><td class="numeric">${row.after.toFixed(3)}</td>
    <td class="numeric ${color(row.gain)}">${row.saved.toFixed(3)}</td><td class="numeric ${color(row.gain)}">${percent(row.gain)}</td></tr>`).join('')
    || '<tr><td colspan="5" class="empty">No matching source files.</td></tr>'
}
function renderDistribution() {
  const [baseline, candidate] = comparisons[get('comparison').value]
  const metric = get('metric').value
  selected = data.files.filter(file => file.policies[baseline][metric] > 0).map(file => {
    const before = file.policies[baseline][metric]
    const after = file.policies[candidate][metric]
    return {...file, before, after, saved: before - after, gain: (before - after) / before * 100}
  }).sort((a, b) => a.gain - b.gain)
  const values = selected.map(row => row.gain)
  const groups = Array.from({length: 5}, () => [])
  selected.forEach((row, index) => groups[Math.min(4, Math.floor(index * 5 / selected.length))].push(row.gain))
  const bands = groups.map((values, index) => ({
    label: `${index * 20}–${(index + 1) * 20}%`, count: values.length,
    median: quantile(values, .5), min: values[0], max: values.at(-1),
  }))
  const scale = Math.max(1, ...bands.map(band => Math.abs(band.median)))
  get('distribution-summary').textContent = `${selected.length} files · ${selected.filter(row => row.gain > 0).length} faster · ${selected.filter(row => row.gain < 0).length} slower · median change ${percent(quantile(values, .5))}. Positive means faster; negative means slower.`
  get('quintile-bars').innerHTML = bands.map(band => {
    const width = Math.abs(band.median) / scale * 47
    return `<div class="bar-row"><span>${band.label}</span><div class="bar-track"><span class="bar ${color(band.median)}" style="left:${band.median < 0 ? 50 - width : 50}%;width:${width}%"></span></div><span class="bar-value ${color(band.median)}">${percent(band.median)}</span></div>`
  }).join('')
  get('quintile-results').innerHTML = bands.map(band => `<tr><td>${band.label}</td><td class="numeric">${band.count}</td><td class="numeric ${color(band.median)}">${percent(band.median)}</td><td class="numeric">${percent(band.min)} to ${percent(band.max)}</td></tr>`).join('')
  get('percentile-boundaries').innerHTML = [0, 20, 40, 60, 80, 100].map(p => `<span>P${p} <strong>${percent(quantile(values, p / 100))}</strong></span>`).join('')
  const extremes = [...selected.slice(0, 2), ...selected.slice(-2).reverse()]
  get('extreme-files').innerHTML = extremes.map((row, index) => `<li><strong>${index < 2 ? 'Slowest' : 'Fastest'} ${index % 2 + 1}:</strong> <code>${escape(row.source)}</code> — <span class="${color(row.gain)}">${percent(row.gain)}</span> (${row.before.toFixed(3)}s → ${row.after.toFixed(3)}s)<br><span class="note">CMake targets: ${row.contexts.map(name => `<code>${escape(name)}</code>`).join(', ')}</span></li>`).join('')
  renderFiles()
}
for (const id of ['comparison', 'metric']) get(id).addEventListener('change', renderDistribution)
get('file-search').addEventListener('input', renderFiles)
get('sort').addEventListener('change', renderFiles)
renderDistribution()
document.documentElement.dataset.reportReady = 'true'
