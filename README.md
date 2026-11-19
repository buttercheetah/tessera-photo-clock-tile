# Photo clock tile

A photo frame for a screen that never sleeps. The tile shows the photo of a Home Assistant image entity over the whole
tile, such as the one from [Immich Random Image](https://github.com/strickdd/ImmichRandomImage), and keeps what belongs
to the photo apart from what is true now:

- **The photo** (bottom left, amber icon): how long ago it was taken in large type (*2 years ago*, or *On this day,
  7 years ago* on its anniversary), where it was taken (*Wilmington, NC*), and the day and time (*September 9, 2024 ·
  9:08 AM*) in grey.
- **Now** (a slim strip at the top right): the time, and the temperature inside and outside with blue icons.

The Waveshare 4.3, 5 and 7-inch screens never go into standby, so Tessera's screensaver never shows on them. Put this
tile on page 1 as a whole page and turn on **Back to page 1**: the screen comes back to the photos the way a
screensaver would.

## Installation

Install via Tessera > Plugins > Add with a link > Release > `https://github.com/buttercheetah/tessera-photo-clock-tile`

New versions are published as GitHub releases tagged vX.Y.Z matching the version in the plugin manifest.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. In Layout, place the **Photo clock** tile from the library's Plugins group on page 1 and make it the whole page.
3. In the inspector, choose the image entity, such as `image.immich_photo_cycle_album_random_image`, then the options
   below.

## Options

- **Inside temperature**: a temperature sensor or thermostat. Starts with `sensor.nest_01aa02ab151203wr_temperature`.
- **Outside temperature**: a weather entity (its `temperature` attribute; the icon follows its condition), a sensor or a
  thermostat. Starts with `weather.forecast_home`.
- **Where it was taken** (optional): a `sensor` or `input_text` entity, either
  - with `city` and `state` attributes (and optionally `country`), shown as *City, ST*; a US state's full name is
    written short (North Carolina → NC), another region as it is; or
  - whose state is the place as it should read, such as `Wilmington, NC`.
  When it is not set, or its state is `unknown`, `unavailable` or empty, the line is left out. Without the option the
  image entity's own `media_city` and `media_state` attributes are used if it ever has them.
- **Date taken from**: the attribute with the moment the photo was taken (`media_localdatetime` for Immich).
- **Time taken** and **How long ago**: turn either off.
- **Tap**: **Go to a page** (Page 2 by default, through `esphome.kitchen_control_screen_show_page`), **Press a button**,
  **Run a script** (the entity in **Button or script**), or **Nothing**. Leave the tile's own Tap setting on Auto.

## Good to know

- A new photo shows as soon as the image entity changes.
- Photo size: Tessera takes at most 4 MB from Home Assistant's image proxy, and Immich Random Image hands out each
  photo's full-size original (a drone's 48 MP JPEG, a Pixel's 12 MP JPEG, a large PNG are often more). Such a photo is
  asked again as the entity's camera still, which Tessera takes up to 8 MB; one over 8 MB shows *This photo is too
  large for the screen* with its caption. Having the integration hand out Immich's preview size instead
  (`/api/assets/<id>/thumbnail?size=preview`) makes every photo small enough.
- The time and the time taken follow the screen's 12 or 24-hour setting.
- The moment taken is read as written: Immich's `media_localdatetime` is the local time where the photo was taken.
- **Go to a page** goes through Home Assistant, so it needs Home Assistant connected. For another screen, add that
  screen's `esphome.<device>_show_page` to `permissions.home_assistant_actions`.
- A board without pictures shows a note instead of the photo.

## Privacy

The photo comes from Home Assistant's own image proxy, fetched by the Tessera app with its own token. Nothing goes to
the internet.
