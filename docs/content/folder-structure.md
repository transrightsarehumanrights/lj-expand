---
id: folder-structure
title: Folder Structure 
---

The folder structure of LJE is relatively simple; all binaries, scripts, and related files are stored in `%USERPROFILE%/.lje`, which is automatically created on first launch.

```bash
 .lje/
   ├── binaries/
   │    └── .lje_bin_warned
   ├── crashes/
   ├── data/
   └── scripts/
```

- `/binaries/` - Where LJE binary modules should be placed. See [`LJE Binary Module SDK`](https://github.com/lj-expand/lj-expand/tree/expansion/sdk). <!-- Should really just move this to the docs -->
    - `/.lje_bin_warned` - File that lets LJE know you have already been warned in console, deleting will trigger the warning again.
- `/crashes/` - Created when a crash dump is generated.
- `/data/` - Simple persistent blob storage for scripts. See [`lje.data`](/api/data).
- `/scripts/` - Where LJE scripts should go. See [`Scripts`](/guides/scripts).