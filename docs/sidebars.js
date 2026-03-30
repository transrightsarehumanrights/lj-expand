/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  apiSidebar: [
    { type: 'doc', id: 'intro', label: 'Introduction' },
    { type: 'doc', id: 'installation', label: 'Installation' },
    { type: 'doc', id: 'cli-options', label: 'Command Line Options' },
    {
      type: 'category',
      label: 'Guides',
      collapsed: false,
      items: [
        'guides/scripts',
        'guides/detours',
        'guides/hooks'
      ]
    },
    {
      type: 'category',
      label: 'API Reference',
      collapsed: false,
      items: [
        'api/base',
        'api/lua',
        'api/func',
        'api/hooks',
        'api/env',
        'api/util',
        'api/gc',
        'api/vm',
        'api/data',
      ],
    },
  ],
};

module.exports = sidebars;
