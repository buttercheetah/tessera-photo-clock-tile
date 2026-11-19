# Installing Photo Clock Tile

## Via Tessera (recommended)

1. In Tessera, go to **Plugins**
2. Click **Add with a link**
3. Choose **Release**
4. Enter: `https://github.com/buttercheetah/tessera-photo-clock-tile`
5. Select your screen and click **Build**

New versions are published as GitHub releases tagged vX.Y.Z. Tessera will offer updates when newer releases are available.

## Manual installation

If you prefer manual installation:

1. Copy this folder to `/config/tessera-plugins/photo_clock_tile/` on your Home Assistant installation (via Samba or the File editor)
2. Ensure `/config/tessera-plugins/photo_clock_tile/tessera-plugin.yaml` shows `version: 0.2.2`
3. Open Tessera > Plugins and press **Build** for your screen (requires Tessera Screen Manager 0.4.98 or later)
4. The build takes a few minutes and updates over Wi-Fi

## Configuration

After installation:

1. Open Layout, page 1
2. Add the **Photo clock** tile from the library's Plugins group
3. Make it fill the whole page
4. In the inspector, configure:
   - **Image entity**: e.g., `image.immich_photo_cycle_album_random_image`
   - **Inside temperature**: defaults to a Nest sensor
   - **Outside temperature**: defaults to `weather.forecast_home`
   - **Where it was taken** (optional): sensor or input_text with location
   - **Tap action**: Go to a page (default), press a button, run a script, or nothing
5. Save

The tile will show photos with their captions (how long ago, where, when) and current conditions (time, temperature).
