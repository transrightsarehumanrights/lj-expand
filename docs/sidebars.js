/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  apiSidebar: [
    { type: 'doc', id: 'intro', label: 'Introduction' },
    { type: 'doc', id: 'installation', label: 'Installation' },
    { type: 'doc', id: 'folder-structure', label: 'Folder Structure' },
    { type: 'doc', id: 'cli-options', label: 'Command Line Options' },
    {
      type: 'category',
      label: 'Guides',
      collapsed: false,
      items: [
        'guides/info-toml',
        'guides/getting-started',
        'guides/your-first-script',
        'guides/settings',
        'guides/detours',
        'guides/extending-functionality'
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
        'api/env',
        'api/util',
        'api/gc',
        'api/vm',
        'api/data',
        'api/secure',
        'api/proxy',
        'api/settings',
        'api/script',
      ],
    },
  ],
};

module.exports = sidebars;
