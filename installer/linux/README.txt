TapeRot for Linux - edge build
===============================

This archive contains an unsigned bleeding-edge build from the `develop` branch:

  VST3/TapeRot.vst3       - the plugin bundle
  Standalone/TapeRot      - the standalone app
  THIRD-PARTY-LICENCES.txt - copyright notices and licence text for the
                            typefaces embedded in the binaries

Installing the VST3
--------------------

Copy the bundle into a VST3 search path. For your user only:

    mkdir -p ~/.vst3
    cp -R VST3/TapeRot.vst3 ~/.vst3/

Or system-wide (all users, needs root):

    sudo cp -R VST3/TapeRot.vst3 /usr/lib/vst3/

Then re-scan plugins in your DAW (e.g. Reaper's "re-scan VST paths").

Running the Standalone
------------------------

    chmod +x Standalone/TapeRot
    ./Standalone/TapeRot

Runtime requirements
---------------------

TapeRot is dynamically linked against standard desktop Linux libraries (ALSA, FreeType,
Fontconfig, X11, GTK3). These are normally already present on a desktop Linux install; if the
plugin/app fails to load, install your distro's packages for those libraries.

This build isn't code-signed (there's no Linux equivalent of macOS Gatekeeper or Windows
SmartScreen to bypass) - it's simply an unreviewed automatic build from the tip of `develop`.
