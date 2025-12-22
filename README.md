<div align="center">
  <img src="doc/lje-logo.png" width="128" height="128" alt="LJE Logo" />
  <h3>lj-expand</h3>
  <p>Stealthy code execution tool for 64-bit Garry's Mod</p>
  <a href="https://discord.gg/ZXP2tG8J"><img alt="Discord" src="https://img.shields.io/discord/1450731263412670518"></a>
  <hr />
</div>

# Installation

**DO NOT INSTALL THIS IN THE GARRY'S MOD FOLDER.** Instead, keep it in a separate folder somewhere else on your computer. Be aware that you will need
to manually update this folder. Right now, there is no versioning as it is very early in development. You will need to manually download artifacts.

1. Look at the latest commit for a green checkmark icon. Click on it to go to the artifacts page.
2. At the left sidebar, click on "Summary"
3. Click on the `lje-***.zip` file to download the latest build.
4. Extract the zip file somewhere safe.
5. Setup the `GMOD_PATH` environment variable to point to the GMod 64-bit executable in `bin\win64\gmod.exe`. **DO NOT** point it to the `gmod.exe` in the root folder.
6. From now on, you can run `lje-launcher.exe` to launch Garry's Mod with lj-expand.

## Disclaimer
I don't condone cheating, or exploiting a server. I do however believe that you should have the freedom to audit and run your own code on your own machine.

# More Information

See [doc/mitigations.md](doc/mitigations.md) for more information about the various anti-detection techniques used in LJE.
See [doc/scripting.md](doc/scripting.md) for more information about scripting with LJE and the API it provides.

# Licensing

No license file on purpose since it is a fork of LuaJIT, which is MIT licensed anyways, which means this project is also MIT licensed.