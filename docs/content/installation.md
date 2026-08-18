---
id: installation
title: Installation
slug: /installation
---

# Installation

Head over to the [releases page](https://github.com/lj-expand/lje-launcher) of the new LJE Launcher and download the latest release, everything else is automatically done for you now.

## Legacy Method

**Note:** This is an old installation guide - you should use the new LJE Launcher.
Installing LJE is a multi-step process. Easy to anyone who understands how their computer works.

### Step 1: Download LJE

1. Go to the [LJE GitHub Repository](https://github.com/lj-expand/lj-expand).
2. Click on the latest commit's status icon like so: ![Artifacts hover](/img/01-01-gh.png).
3. Click on the `Details` link when the modal opens up: ![Click on link](/img/01-02-details.png).
4. Once you're in the Details page, click on `Summary`, then click on the latest artifact: ![Artifacts](/img/01-03-artifacts.png)
5. Download the zip containing LJE to a folder **where you'll keep LJE consistently**.

### Step 2: Prepare GMod

1. Go to Steam.
2. Right click on Garry's Mod, open the `Properties...` menu.
3. Go to `Game Versions & Beta` and **ENSURE** you're on the x86-64 branch: ![Betas](/img/02-01-betas.png).

### Step 3: Set `GMOD_PATH`

LJE needs to know where you keep your GMod. This is also useful if you have a non-standard GMod installation.
It knows this by the `GMOD_PATH` environment variable, which you will set up.

1. Open up Settings.
2. Search for `environment` in the search bar.
3. Click on the item specifically for your account:

  ![Add env to account](/img/03-01-env.png)

4. In the menu, click `New...` to open up the dialog.
5. Type in `GMOD_PATH` for variable name.
6. Click `Browse File` next, and navigate to where your GMod is installed.
7. **Select `bin/win64/gmod.exe`** from your GMod installation folder: ![Select](/img/03-02-select.png)
8. Close the dialog now.

### Step 4: Run LJE

1. Extract LJE from the zip.
2. Run `lje-launcher.exe` to open GMod with LJE: ![Run](/img/04-01-run.png)
3. GMod should open with the LJE console appearing.

### Troubleshooting

1. **Make sure** your GMod is on `x86-64`, LJE does not support any other version of GMod.
2. The environment variable **has to** point to the `gmod.exe` in `bin/win64`, make sure it does.
3. Some antiviruses might find the injection of `lje-launcher.exe` suspicious, explicitly allow the file if it becomes an issue.
