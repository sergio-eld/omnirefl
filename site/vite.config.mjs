import {readFile} from 'node:fs/promises'
import {fileURLToPath} from 'node:url'
import path from 'node:path'

import hljs from 'highlight.js'
import {Marked} from 'marked'
import {markedHighlight} from 'marked-highlight'
import {defineConfig} from 'vite'

const site = path.dirname(fileURLToPath(import.meta.url))
const repository = path.dirname(site)
const readme = path.join(repository, 'README.md')
const highlightTheme = {
  dark: fileURLToPath(
    import.meta.resolve('highlight.js/styles/github-dark.css')),
  light: fileURLToPath(
    import.meta.resolve('highlight.js/styles/github.css')),
}
const sections = [
  'introduction',
  'sneak-peek',
  'experience',
  'scope',
  'install',
  'limitations',
]

const extract = (markdown, section) => {
  const begin = `<!-- pages:${section}:start -->`
  const end = `<!-- pages:${section}:end -->`
  const beginAt = markdown.indexOf(begin)
  const endAt = markdown.indexOf(end)

  if (-1 === beginAt || -1 === endAt || endAt <= beginAt)
    throw new Error(`invalid README markers for '${section}'`)

  return markdown.slice(beginAt + begin.length, endAt).trim()
}

const markdown = new Marked(markedHighlight({
  emptyLangClass: 'hljs',
  langPrefix: 'hljs language-',
  highlight(code, language) {
    const supported = hljs.getLanguage(language) ? language : 'plaintext'
    return hljs.highlight(code, {language: supported}).value
  },
}))

const renderReadme = async () => {
  const source = await readFile(readme, 'utf8')
  const rendered = (await Promise.all(sections.map(async section =>
    `<section id="${section}">
      ${await markdown.parse(extract(source, section))}
    </section>`))).join('\n')

  return rendered
    .replaceAll(
      'src="omnirefl-banner.png"',
      'src="https://raw.githubusercontent.com/sergio-eld/omnirefl/master/omnirefl-banner.png"')
    .replaceAll(/href="#([^"]+)"/g, (link, fragment) =>
      sections.includes(fragment)
        ? link
        : `href="https://github.com/sergio-eld/omnirefl/blob/master/README.md#${fragment}"`)
    .replaceAll(
      /href="(?!#|https?:\/\/)([^"]+)"/g,
      'href="https://github.com/sergio-eld/omnirefl/blob/master/$1"')
}

const readmePlugin = () => ({
  name: 'omnirefl-readme',
  configureServer(server) {
    server.watcher.add(readme)
    server.watcher.on('change', changed => {
      if (path.resolve(changed) === readme)
        server.ws.send({type: 'full-reload'})
    })
  },
  transformIndexHtml: {
    order: 'pre',
    async handler(html) {
      const placeholder = '<!-- omnirefl:readme -->'
      const highlightPlaceholder = '/* omnirefl:highlight */'

      if (!html.includes(placeholder))
        throw new Error(`index.html is missing '${placeholder}'`)
      if (!html.includes(highlightPlaceholder))
        throw new Error(`index.html is missing '${highlightPlaceholder}'`)

      const highlight = `
        @media (prefers-color-scheme: light) {
          ${await readFile(highlightTheme.light, 'utf8')}
        }

        @media (prefers-color-scheme: dark) {
          ${await readFile(highlightTheme.dark, 'utf8')}
        }
      `

      return html
        .replace(highlightPlaceholder, highlight)
        .replace(placeholder, await renderReadme())
    },
  },
})

export default defineConfig(({command}) => ({
  base: 'build' === command ? '/omnirefl/' : '/',
  plugins: [readmePlugin()],
  server: {host: '0.0.0.0'},
}))
