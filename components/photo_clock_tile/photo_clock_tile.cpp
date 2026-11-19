#include "photo_clock_tile.h"
#include "esphome/core/application.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::photo_clock_tile {

using tessera::Font;
namespace ui = tessera::ui;

// Icons of Tessera's set (tile_icon_names.h).
constexpr uint32_t ICON_HOME = 0xF02DC, ICON_THERMOMETER = 0xF050F, ICON_MEMORY = 0xF0674 /* creation */,
                   ICON_SUNNY = 0xF0599, ICON_NIGHT = 0xF0594,
                   ICON_PARTLY = 0xF0595, ICON_CLOUDY = 0xF0590, ICON_RAINY = 0xF0597, ICON_POURING = 0xF0596,
                   ICON_SNOWY = 0xF0598, ICON_SNOWY_RAINY = 0xF067F, ICON_FOG = 0xF0591, ICON_HAIL = 0xF0592,
                   ICON_LIGHTNING = 0xF0593, ICON_LIGHTNING_RAINY = 0xF067E, ICON_WINDY = 0xF059D;

// The photo over the whole tile, and two groups over it that never mix. The PHOTO's caption at the bottom left, with
// the amber of the mark: how long ago (or On this day), where it was taken, and the day and time it was taken in grey.
// NOW as a slim strip at the top right: the time and the temperature inside and outside, blue icons and white text.
class PhotoTile : public tessera::Tile {
 public:
  explicit PhotoTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    w_ = c.width;
    h_ = c.height;
    entity_ = c.entity ? c.entity : "";
    attribute_ = c.options["date_attribute"] | "media_localdatetime";
    show_time_ = c.options["show_time_taken"] | true;
    show_ago_ = c.options["show_ago"] | true;
    const std::string tap = c.options["tap_action"] | "page";
    tap_ = tap == "button" ? TAP_BUTTON : tap == "script" ? TAP_SCRIPT : tap == "none" ? TAP_NONE : TAP_PAGE;
    tap_page_ = std::max(1, std::min(24, (int) (c.options["tap_page"] | 2)));
    tap_entity_ = c.options["tap_entity"] | "";

    if (ui::pictures()) {
      place_ = ui::picture(c.parent, tessera::Placeholder::NONE);  // no block: a photo that never comes leaves the card plain under the note
      lv_obj_set_size(place_, w_, h_);
      lv_obj_set_pos(place_, 0, 0);
      if (entity_.empty()) lv_obj_add_flag(place_, LV_OBJ_FLAG_HIDDEN);
    }
    // The photo's caption: ✦ 2 years ago / Wilmington, NC / September 9, 2024 · 9:08 AM.
    photo_ = shade(c.parent, LV_OPA_80);
    photo_icon_ = ui::label(photo_, Font::ICON_SMALL, theme::MARK_AMBER);
    ui::set_text(photo_icon_, ui::icon(ICON_MEMORY));
    head_ = ui::label(photo_, Font::HEADLINE, theme::CAMERA_INK);
    loc_ = ui::label(photo_, Font::BODY_LARGE, theme::CAMERA_INK);
    sub_ = ui::label(photo_, Font::BODY, theme::CAMERA_NOTE);
    // Now: 10:38 PM  ⌂ 71°  ☾ 46°
    now_ = shade(c.parent, LV_OPA_80);
    time_ = ui::label(now_, Font::BODY_LARGE, theme::CAMERA_INK);
    in_icon_ = ui::label(now_, Font::ICON_SMALL, theme::MARK_BLUE);
    in_ = ui::label(now_, Font::BODY_LARGE, theme::CAMERA_INK);
    out_icon_ = ui::label(now_, Font::ICON_SMALL, theme::MARK_BLUE);
    out_ = ui::label(now_, Font::BODY_LARGE, theme::CAMERA_INK);

    note_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(note_, w_);
    lv_obj_set_pos(note_, 0, (h_ - ui::line_height(Font::BODY)) / 2);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
  }

  void on_state(JsonObjectConst data) override {
    if (place_) {
      std::string address;
      if (!entity_.empty()) {
        const std::string state = data["state"] | "";
        std::string version;
        for (char ch : state)
          if (version.size() < 32 && std::isalnum((unsigned char) ch)) version += ch;
        address = "/api/image_proxy/" + roomy::plain(entity_) + (version.empty() ? "" : "?v=" + version);
      }
      // A new photo starts again at Home Assistant's proxy; the same one keeps what it shows (the proxy's picture, or
      // the camera still it fell back to), so an attribute changing asks nothing again.
      if (!roomy::same(address_, address)) {
        address_ = roomy::text(address);
        step_ = PROXY;
        ui::set_picture(place_, address, tessera::Fit::COVER);
      }
    }
    show_note();
    read_taken(data["attributes"][roomy::plain(attribute_).c_str()]);
    loc_text_ = roomy::text(place(data));
    in_text_ = roomy::text(temperature(data["entities"]["inside"], nullptr));
    uint32_t icon = ICON_THERMOMETER;
    out_text_ = roomy::text(temperature(data["entities"]["outside"], &icon));
    out_glyph_ = icon;
    minute_ = 0;  // paint again now
    on_tick(tessera::epoch());
  }

  void on_tick(uint32_t epoch) override {
    if (epoch && epoch / 60 == minute_) return;
    minute_ = epoch ? epoch / 60 : 0;
    layout(epoch);
  }

  void on_theme() override { minute_ = 0; on_tick(tessera::epoch()); }

  // A photo the app could not take from the proxy (one over 4 MB: a phone's or a drone's full-size original, as Immich
  // Random Image hands out) is asked again as the entity's camera still, which the app takes up to 8 MB. One that does
  // not come that way either leaves a note in place of a blank page; the caption and the now strip stay.
  void on_picture(lv_obj_t *place, tessera::PictureState state) override {
    if (place != place_ || entity_.empty()) return;
    if (state == tessera::PictureState::SHOWN) {
      if (step_ == GAVE_UP) step_ = CAMERA;
    } else if (step_ == PROXY) {
      step_ = CAMERA;
      ui::set_picture(place_, "camera:" + roomy::plain(entity_), tessera::Fit::COVER);
    } else {
      step_ = GAVE_UP;
    }
    show_note();
  }

  void on_tap() override {
    switch (tap_) {
      case TAP_PAGE: {
        std::string device = App.get_name();
        std::replace(device.begin(), device.end(), '-', '_');
        JsonDocument fields;
        fields["page"] = tap_page_;
        const std::string service = "esphome." + device + "_show_page";
        tessera::action(service.c_str(), "", fields.as<JsonVariantConst>());
        break;
      }
      case TAP_BUTTON:
        if (!tap_entity_.empty()) tessera::action("button.press", roomy::plain(tap_entity_));
        break;
      case TAP_SCRIPT:
        if (!tap_entity_.empty()) tessera::action("script.turn_on", roomy::plain(tap_entity_));
        break;
      case TAP_NONE:
        break;
    }
  }

 private:
  enum Step : uint8_t { PROXY, CAMERA, GAVE_UP };
  void show_note() {
    ui::set_text(note_, !place_ ? plugin_->text("no_picture")
                        : entity_.empty() ? plugin_->text("choose")
                        : step_ == GAVE_UP ? plugin_->text("too_large") : "");
  }
  enum Tap : uint8_t { TAP_PAGE, TAP_BUTTON, TAP_SCRIPT, TAP_NONE };

  static lv_obj_t *shade(lv_obj_t *parent, lv_opa_t opa) {
    lv_obj_t *block = ui::block(parent, theme::CAMERA_PAGE);
    lv_obj_set_style_bg_opa(block, opa, 0);
    lv_obj_add_flag(block, LV_OBJ_FLAG_HIDDEN);
    return block;
  }
  static int width(const std::string &text, Font font) { return text.empty() ? 0 : ui::text_width(text, font); }
  static void put(lv_obj_t *label, const std::string &text, Font font, int x, int y) {
    if (!label) return;
    ui::set_text(label, text);
    lv_obj_set_width(label, std::max(1, width(text, font)));
    lv_obj_set_pos(label, x, y);
    if (text.empty()) lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
  }
  static void hide(lv_obj_t *o) { if (o) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); }
  static void box(lv_obj_t *o, int x, int y, int w, int h) {
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  }

  // A temperature as "71°" from a sensor's state, a thermostat's current_temperature or a weather entity's
  // temperature; `icon` gets the weather's condition when it is one.
  static std::string temperature(JsonObjectConst e, uint32_t *icon) {
    if (e.isNull()) return "";
    JsonObjectConst a = e["attributes"];
    const char *state = e["state"] | "";
    float v = NAN;
    if (a["current_temperature"].is<float>()) v = a["current_temperature"].as<float>();
    else if (a["temperature"].is<float>() && !(state[0] >= '0' && state[0] <= '9') && state[0] != '-') {
      v = a["temperature"].as<float>();
      if (icon) *icon = condition(state);
    } else if (std::strcmp(state, "unavailable") && std::strcmp(state, "unknown") && *state) {
      char *end = nullptr;
      const float x = std::strtof(state, &end);
      if (end != state) v = x;
    }
    if (std::isnan(v)) return "";
    char out[16];
    std::snprintf(out, sizeof(out), "%d\u00B0", (int) std::lround(v));
    return out;
  }
  static uint32_t condition(const char *s) {
    const std::string c = s;
    if (c == "clear-night") return ICON_NIGHT;
    if (c == "sunny") return ICON_SUNNY;
    if (c == "partlycloudy") return ICON_PARTLY;
    if (c == "cloudy") return ICON_CLOUDY;
    if (c == "rainy") return ICON_RAINY;
    if (c == "pouring") return ICON_POURING;
    if (c == "snowy") return ICON_SNOWY;
    if (c == "snowy-rainy") return ICON_SNOWY_RAINY;
    if (c == "fog") return ICON_FOG;
    if (c == "hail") return ICON_HAIL;
    if (c == "lightning") return ICON_LIGHTNING;
    if (c == "lightning-rainy") return ICON_LIGHTNING_RAINY;
    if (c == "windy" || c == "windy-variant") return ICON_WINDY;
    return ICON_THERMOMETER;
  }

  // Where the photo was taken: the `location` entity's state, else the image's city/state/country (Immich Random
  // Image 1.5), else media_city and media_state (a US state written short, as Wilmington, NC), else media_country.
  static std::string attr_text(JsonObjectConst a, const char *key) {
    const char *v = a[key] | "";
    return v ? v : "";
  }
  static std::string first_attr(JsonObjectConst a, const char *key, const char *alt) {
    const std::string v = attr_text(a, key);
    return v.empty() ? attr_text(a, alt) : v;
  }
  std::string place(JsonObjectConst data) const {
    JsonObjectConst chosen = data["entities"]["location"];
    if (!chosen.isNull()) {
      const std::string s = chosen["state"] | "";
      if (s.empty() || s == "unknown" || s == "unavailable" || s == "none") return "";
      if (chosen["attributes"]["city"].isNull() && chosen["attributes"]["state"].isNull()) return s;
    }
    JsonObjectConst a = chosen.isNull() ? data["attributes"] : chosen["attributes"];
    std::string city, state, country;
    if (chosen.isNull()) {
      city = first_attr(a, "city", "media_city");
      state = first_attr(a, "state", "media_state");
      country = first_attr(a, "country", "media_country");
    } else {
      city = attr_text(a, "city");
      state = attr_text(a, "state");
      country = attr_text(a, "country");
    }
    std::string region = short_state(state);
    if (region.empty()) region = state.empty() ? country : state;
    if (city.empty() || city == "None") return region;
    if (region.empty()) return city;
    return tessera::fill(tessera::fill(plugin_->text("place"), "city", city).c_str(), "state", region);
  }
  static std::string short_state(const std::string &name) {
    static const char *const STATES[][2] = {
        {"Alabama", "AL"}, {"Alaska", "AK"}, {"Arizona", "AZ"}, {"Arkansas", "AR"}, {"California", "CA"},
        {"Colorado", "CO"}, {"Connecticut", "CT"}, {"Delaware", "DE"}, {"District of Columbia", "DC"},
        {"Florida", "FL"}, {"Georgia", "GA"}, {"Hawaii", "HI"}, {"Idaho", "ID"}, {"Illinois", "IL"},
        {"Indiana", "IN"}, {"Iowa", "IA"}, {"Kansas", "KS"}, {"Kentucky", "KY"}, {"Louisiana", "LA"},
        {"Maine", "ME"}, {"Maryland", "MD"}, {"Massachusetts", "MA"}, {"Michigan", "MI"}, {"Minnesota", "MN"},
        {"Mississippi", "MS"}, {"Missouri", "MO"}, {"Montana", "MT"}, {"Nebraska", "NE"}, {"Nevada", "NV"},
        {"New Hampshire", "NH"}, {"New Jersey", "NJ"}, {"New Mexico", "NM"}, {"New York", "NY"},
        {"North Carolina", "NC"}, {"North Dakota", "ND"}, {"Ohio", "OH"}, {"Oklahoma", "OK"}, {"Oregon", "OR"},
        {"Pennsylvania", "PA"}, {"Rhode Island", "RI"}, {"South Carolina", "SC"}, {"South Dakota", "SD"},
        {"Tennessee", "TN"}, {"Texas", "TX"}, {"Utah", "UT"}, {"Vermont", "VT"}, {"Virginia", "VA"},
        {"Washington", "WA"}, {"West Virginia", "WV"}, {"Wisconsin", "WI"}, {"Wyoming", "WY"}, {"Puerto Rico", "PR"}};
    if (name.size() == 2) {
      for (const auto &s : STATES)
        if (name == s[1]) return s[1];
    }
    const char *best = "";
    size_t best_len = 0;
    for (const auto &s : STATES) {
      const std::string full = s[0];
      if (name == full) return s[1];
      if (name.size() >= full.size() && name.find(full) != std::string::npos && full.size() > best_len) {
        best = s[1];
        best_len = full.size();
      }
    }
    return best_len ? best : "";
  }

  // The moment the photo was taken: an ISO text (Immich's local time, as written) or seconds since 1970.
  void read_taken(JsonVariantConst value) {
    ty_ = tm_ = td_ = 0;
    thh_ = tmm_ = -1;
    if (value.is<const char *>()) {
      const char *text = value.as<const char *>();
      int y = 0, m = 0, d = 0, hh = -1, mm = -1;
      const int n = text ? std::sscanf(text, "%4d-%2d-%2dT%2d:%2d", &y, &m, &d, &hh, &mm) : 0;
      if (n < 3) return;
      ty_ = y, tm_ = m, td_ = d;
      if (n == 5) thh_ = hh, tmm_ = mm;
    } else if (value.is<double>() && value.as<double>() > 0) {
      const tessera::LocalTime t = tessera::local_time((uint32_t) value.as<double>());
      ty_ = t.year, tm_ = t.month, td_ = t.day, thh_ = t.hour, tmm_ = t.minute;
    }
    if (ty_ < 1900 || tm_ < 1 || tm_ > 12 || td_ < 1 || td_ > 31) ty_ = 0;
  }

  std::string month(int m, bool full) const {
    char key[12];
    std::snprintf(key, sizeof(key), full ? "month_%d" : "mon_%d", m);
    return plugin_->text(key);
  }
  std::string taken_date(bool full) const {
    if (!ty_) return "";
    std::string out = tessera::fill(plugin_->text(full ? "long_date" : "short_date"), "month", month(tm_, full));
    out = tessera::fill(out.c_str(), "day", std::to_string(td_));
    return tessera::fill(out.c_str(), "year", std::to_string(ty_));
  }
  // The time of day the photo was taken, 12 or 24 hours as the screen writes its own clock.
  std::string taken_time(bool twelve) const {
    if (!ty_ || thh_ < 0 || !show_time_) return "";
    char out[24];
    if (twelve)
      std::snprintf(out, sizeof(out), "%d:%02d %s", thh_ % 12 ? thh_ % 12 : 12, tmm_, plugin_->text(thh_ < 12 ? "am" : "pm"));
    else
      std::snprintf(out, sizeof(out), "%02d:%02d", thh_, tmm_);
    return out;
  }
  // How long ago, on the screen's calendar: Today, 3 days ago, 4 months ago, 2 years ago; On this day, 2 years ago.
  std::string ago(const tessera::LocalTime &now) const {
    if (!ty_ || !show_ago_ || !now.year) return "";
    const long days = days_of(now.year, now.month, now.day) - days_of(ty_, tm_, td_);
    if (days < 0) return "";
    if (days == 0) return plugin_->text("ago_today");
    int months = (now.year - ty_) * 12 + now.month - tm_ - (now.day < td_ ? 1 : 0);
    if (months < 1) return tessera::format(plugin_->text("ago_days"), days);
    if (months < 12) return tessera::format(plugin_->text("ago_months"), months);
    const std::string years = tessera::format(plugin_->text("ago_years"), months / 12);
    if (now.month == tm_ && now.day == td_) return tessera::fill(plugin_->text("on_this_day"), "ago", years);
    return years;
  }
  static long days_of(int y, int m, int d) {  // days since 1970 of a civil date
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
  }
  std::string join(const std::string &a, const std::string &b) const {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return a + plugin_->text("dot") + b;
  }

  // Everything placed again: once a minute and whenever the data changed.
  void layout(uint32_t epoch) {
    const bool shown = place_ && !entity_.empty();
    if (!shown) {
      hide(now_), hide(photo_);
      return;
    }
    const tessera::LocalTime now = epoch ? tessera::local_time(epoch) : tessera::LocalTime{};
    const std::string clock = epoch ? tessera::clock_text(epoch) : "";
    const bool twelve = clock.find_first_of("AaPp") != std::string::npos;
    const std::string in = roomy::plain(in_text_), out = roomy::plain(out_text_), where = roomy::plain(loc_text_);
    const int m = ui::px(14), pad = ui::px(12);
    const int lb = ui::line_height(Font::BODY), ll = ui::line_height(Font::BODY_LARGE), li = ui::line_height(Font::ICON_SMALL);
    const int lh = ui::line_height(Font::HEADLINE);
    const int wi = width(ui::icon(ICON_HOME), Font::ICON_SMALL);

    // The caption at the bottom left: how long ago large, then where, then the day and time. Without how long ago,
    // the day is the headline and the time below it; a line with nothing to say leaves no gap.
    const std::string ago_text = ago(now);
    const std::string head = ago_text.empty() ? taken_date(true) : ago_text;
    const std::string sub = ago_text.empty() ? taken_time(twelve) : join(taken_date(true), taken_time(twelve));
    if (head.empty() && where.empty()) {
      hide(photo_);
    } else {
      const int x = pad + wi + ui::px(8), maxw = w_ - 2 * m - x - pad;
      int y = pad / 2;
      put(photo_icon_, ui::icon(ICON_MEMORY), Font::ICON_SMALL, pad, y + ((head.empty() ? ll : lh) - li) / 2);
      put(head_, head, Font::HEADLINE, x, y);
      if (!head.empty()) y += lh;
      put(loc_, where, Font::BODY_LARGE, x, y);
      if (!where.empty()) y += ll;
      put(sub_, sub, Font::BODY, x, y);
      if (!sub.empty()) y += lb;
      int tw = std::max({width(head, Font::HEADLINE), width(where, Font::BODY_LARGE), width(sub, Font::BODY)});
      tw = std::min(tw, maxw);
      // A line wider than the page ends in dots rather than running off it.
      lv_obj_set_width(head_, std::max(1, std::min(width(head, Font::HEADLINE), maxw)));
      lv_obj_set_width(loc_, std::max(1, std::min(width(where, Font::BODY_LARGE), maxw)));
      lv_obj_set_width(sub_, std::max(1, std::min(width(sub, Font::BODY), maxw)));
      const int ph = y + pad / 2 + ui::px(2);
      box(photo_, m, h_ - m - ph, x + tw + pad, ph);
    }

    // Now at the top right: 10:38 PM  ⌂ 71°  ☾ 46°; a temperature without a value leaves out its icon too.
    int x = pad;
    const int sh = ll + ui::px(10), y = (sh - ll) / 2;
    put(time_, clock, Font::BODY_LARGE, x, y);
    x += width(clock, Font::BODY_LARGE);
    auto temp = [&](lv_obj_t *icon, lv_obj_t *value, uint32_t glyph, const std::string &v) {
      if (v.empty()) { hide(icon), hide(value); return; }
      x += ui::px(16);
      put(icon, ui::icon(glyph), Font::ICON_SMALL, x, (sh - li) / 2);
      x += wi + ui::px(4);
      put(value, v, Font::BODY_LARGE, x, y);
      x += width(v, Font::BODY_LARGE);
    };
    temp(in_icon_, in_, ICON_HOME, in);
    temp(out_icon_, out_, out_glyph_, out);
    if (clock.empty() && in.empty() && out.empty()) hide(now_);
    else box(now_, w_ - m - (x + pad), m, x + pad, sh);
  }

  const tessera::Plugin *plugin_;
  int w_ = 0, h_ = 0;
  bool show_time_ = true, show_ago_ = true;
  Tap tap_ = TAP_PAGE;
  Step step_ = PROXY;
  int tap_page_ = 2;
  int ty_ = 0, tm_ = 0, td_ = 0, thh_ = -1, tmm_ = -1;
  uint32_t minute_ = 0, out_glyph_ = ICON_THERMOMETER;
  tessera::Text entity_, attribute_, tap_entity_, in_text_, out_text_, loc_text_, address_;   // PSRAM first
  lv_obj_t *place_{}, *note_{};
  lv_obj_t *now_{}, *time_{}, *in_icon_{}, *in_{}, *out_icon_{}, *out_{};
  lv_obj_t *photo_{}, *photo_icon_{}, *head_{}, *loc_{}, *sub_{};
};

void PhotoClockTile::setup() {
  add_tile("photo", [this]() { return new PhotoTile(this); });
}

}  // namespace esphome::photo_clock_tile
