# Changelog

## 0.2.2
- Use the configured image entity for the camera-still fallback (no hardcoded
  `image.immich_photo_cycle_albumn_random_image` id). Examples and preview follow
  Immich Random Image 1.5 entity ids (`album`, not `albumn`).
- Read a photo's place from the image entity's `city` / `state` / `country`
  attributes when those are present (1.5.0), still falling back to `media_city`
  and the optional location sensor. Messy EXIF such as "Harnett County, North Carolina" is shortened to NC.

- Date taken can be the image's `date_taken` attribute.


## 0.2.1
- Fix: photos over 4 MB (a phone's or a drone's full-size original, which Immich Random Image hands out as it is)
  showed a blank page, as the app takes at most 4 MB from Home Assistant's image proxy. Such a photo is now asked again
  as the entity's camera still (up to 8 MB), and one larger than that shows a note instead of a blank page.

## 0.2.0
- New look (Noah's pick, Memory): the photo's caption at the bottom left (how long ago or On this day, where it was
  taken, the day and time it was taken) and a slim strip of now at the top right (the time, inside and outside
  temperature).
- New options: Inside temperature and Outside temperature (entities, with defaults), Where it was taken (optional
  entity), Time taken, How long ago. Clock and Date taken are gone.
- Needs plugin API 0.27 (Tessera Screen Manager 0.4.98, firmware 0.61.0) for the entity defaults.

## 0.1.0
- First version: an image entity's photo over the whole tile, the time at the bottom left and the day the photo was
  taken at the bottom right, on soft shades. A tap goes to a page (default 2), presses a button, runs a script or does
  nothing. Needs a screen with plugin API 0.12; the Whole page size needs Tessera 0.4.98 (firmware 0.61.0).
