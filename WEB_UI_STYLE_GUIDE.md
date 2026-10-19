# Web UI Style Guide (ESP32 projects)

How the ROR Controller's web interface was made consistent on 2026-10-17 and 2026-10-18, written so the same treatment can be applied to other ESP32 projects, starting with `../SkyConditions_90640`.

All code here lives in `main/src/webserver/html_templates.h` (page templates, shared CSS and JS) and `main/src/webserver/web_ui_handler.cpp` (routes and JSON). SkyConditions uses the same layout (`src/webserver/html_templates.h` with `getCommonStyles()`, `getPageHeader()` and `getNavBar()`, plus `web_ui_handler.cpp`), so most sections port directly.

---

## 1. What changed

### 2026-10-17

| Commit | Change |
|---|---|
| `59a600b` | Setup page split into **tabs**. The selected tab is remembered (URL hash + localStorage). Saving a form **redirects back to the same tab** with a "Settings saved" banner instead of showing a bare text page. The page is **streamed** tab by tab instead of built as one ~43 KB `String`. Every page declares UTF-8 and the template strings are ASCII-only. |
| `9f1f25b` | **Input widths capped** (text 420 px, numbers 200 px). **Status tables capped** at 720 px. Status-dot colours made **global**: they had only been defined on two pages, so setup-page dots were invisible. Dots sit in a **fixed gutter** so every value lines up. |
| `c6abe86` | Home page cells **refresh live** every 2 s instead of only at page load. The timing toggles are left-aligned. A **grey** dot colour was added; "No Sensor" dots had been invisible. |
| `c854187` | **Consistency pass:** one toggle-label form everywhere (`Label: Enabled/Disabled`). One dot size (12 px). Blue for normal actions, red only for destructive ones. Removed duplicated rows and text echoes. The same labels and states are used for the same thing on every page. A safety sensor that has never reported shows grey "Not seen", not green "Safe". |
| `ef04a1a` | **One card style on every page.** Home, Roof Control and WiFi Config dropped their own grey, centred styles for the shared navy cards, left-aligned header and table styling. Page-specific styles moved into the shared stylesheet. |
| `e91783a` | **Status bar on every page**, built into the nav. It shows the error reason when the roof is in ERROR, which replaced the big status banners. The page is a centred **1040 px column** and generic tables are capped at **860 px**. |

### 2026-10-18 (`280b5c4`, v4.0.1)

- **Live sensor tables:** the setup page's park and safety sensor tables refresh their status, temperature and last-seen cells in place. Both tables share one layout and column order. A hint appears when a new sensor is discovered.
- **Live clocks:** every time and date display ticks once a second, synced to the controller's millisecond time instead of jumping on each 2 s poll.
- **Update page integrated:** the nav's **Update** link opens `/firmware`. This page has our header, nav and status bar, and frames ElegantOTA's `/update` inside a card, switched to its dark theme with its background cleared.
- **Form fix:** the MQTT enable toggle had no `name`, so its own form's Save button never sent it.

---

## 2. Design tokens

Use exactly these values everywhere. Nothing page-specific should introduce a new colour.

| Role | Value | Used for |
|---|---|---|
| Page background | `#1a1a2e` | `body` |
| Card | `#16213e` | `.card`, `.status-card`, inactive tab buttons |
| Accent surface | `#0f3460` | nav buttons, table headers, inputs, active tab |
| Border | `#2d3561` | tables, inputs |
| Text | `#e0e0e0` | body text |
| Muted text | `#b0b0b0` / `#8899aa` | labels, help text, version line |
| Heading | `#a0c4ff` | `h1`, `h2` |
| Sub-heading | `#81c784` | `h3` |
| Link / primary button | `#74b9ff` (hover `#a0c4ff`) | links, `button`, submit |
| Nav "warn" button | `#7f4f00` (hover `#a06200`) | the **Update** link |
| Danger | `#e57373` | destructive buttons, `.danger` toggles |
| Good / green | `#81c784` | status dot |
| Bad / red | `#e57373` | status dot |
| Info / blue | `#64b5f6` | status dot (e.g. roof Open) |
| Warning / orange | `#ffb74d` | status dot, notes |
| Unknown / grey | `#78909c` | status dot (never seen, no sensor) |

Font: `Arial, sans-serif`. Card radius 8 px, nav and input radius 4 px.

> SkyConditions already uses this palette (the ROR stylesheet comment says "matches SkyConditions_90640"). Its `.card` padding is 16 px against ROR's 20 px; pick one value and use it in both projects.

---

## 3. Page skeleton

Every page is built the same way:

```cpp
String html = getPageHeader("Page Title - Device");   // <!DOCTYPE>, <meta charset='utf-8'>, viewport, shared CSS
html += "<h1>Page Title</h1>\n";
html += "<p class='page-sub'>Version: " + String(DEVICE_VERSION) + "</p>\n";
html += getNavBar();                                   // nav links + live status bar
html += "<div class='card'>...</div>";                 // content, one card per topic
html += "</body></html>";
```

Rules:
- **Left-aligned header** (title, version line, nav). No centred page titles.
- **Centred column, capped width:** `body { max-width: 1040px; margin: 20px auto; padding: 0 16px; }`. Nothing should stretch across a wide monitor.
- **Content goes in cards.** No page defines its own background, card colour or table style. A style needed by one page still goes into `getCommonStyles()`.
- Every page includes `<meta charset='utf-8'>`, and the **C++ template strings are ASCII-only**. Use HTML entities (`&mdash;`, `&deg;`, `&middot;`) instead of literal non-ASCII characters.

---

## 4. Nav and live status bar

`getNavBar()` returns the nav links **plus** `getStatusCard()`, so every page gets the status bar without any extra work:

```cpp
inline String getNavBar() {
  String navbar =
    "<div class='nav'>\n"
    "<a href='/'>Home</a>\n"
    "<a href='/control'>Roof Control</a>\n"
    "<a href='/setup'>Setup</a>\n"
    "<a href='/console'>Console</a>\n"
    "<a href='/firmware' class='warn'>Update</a>\n"   // warn colour: it restarts the device
    "</div>\n";
  return navbar + getStatusCard();
}
```

The status bar is a compact card with one line of `label value` items. It polls `/api/status` every 2 s:

```cpp
auto item = [](const char* label, const char* id) {
  return String("<span><span style='color:#8899aa'>") + label + "</span> <strong id='" + id + "'>-</strong></span>";
};
// flex row: display:flex; flex-wrap:wrap; gap:6px 20px; font-size:0.9em
html += item("Roof", "ssRoof"); html += item("Telescope", "ssPark"); /* ... */
```

- **Keep it to one line.** Merge related items where you can: the two limit switches became a single "Limit: Open / Closed / Neither / Both".
- **Errors show here**, in a red line under the items. This replaced the large per-page status banners.
- The status bar's script is also the **shared poll** for the page. The live clocks and the live sensor tables (sections 9 and 10) are driven from the same `fetch('/api/status')`, so a page doesn't start extra polls.

For SkyConditions, the status bar would carry sky state (Clear / Cloudy / Rain), the safety verdict, ambient temperature, network and MQTT.

---

## 5. Cards, tables and status dots

### Tables

```css
table { border-collapse: collapse; width: 100%; max-width: 860px; }
table, th, td { border: 1px solid #2d3561; }
th, td { padding: 8px; text-align: left; }
th { background-color: #0f3460; }
table.status-table, table.net-table { max-width: 720px; }
.status-table th { width: 40%; }
```

- **Two-column "status tables"** (label | value) use `class='status-table'` and are capped at 720 px.
- **Wide data tables** with many columns, such as the 8-column safety sensor table, override the cap with `style='max-width:none'`.

### Status dots

One dot size (12 px), and the colour classes are defined **globally**. They were once defined on only two pages, which made the dots invisible everywhere else.

```css
.status-indicator { display:inline-block; width:12px; height:12px; border-radius:50%; margin-right:6px; vertical-align:middle; }
.status-indicator.green  { background:#81c784; }
.status-indicator.red    { background:#e57373; }
.status-indicator.blue   { background:#64b5f6; }
.status-indicator.orange { background:#ffb74d; }
.status-indicator.gray   { background:#78909c; }
.status-indicator.blink  { animation: blink 1s infinite alternate; }
```

**Dot gutter:** in status tables the dot is absolutely positioned in the cell's left padding, so values with and without a dot start at the same x position:

```css
table.status-table td { position: relative; padding-left: 30px; }
table.status-table .status-indicator { position:absolute; left:10px; top:50%; margin:-6px 0 0 0; }
```

**Colour meanings, used the same way everywhere:**

| Colour | Meaning |
|---|---|
| green | OK / safe / connected / at the expected position |
| red | bad / unsafe / not parked / off |
| red + `blink` | needs attention now: offline, error, signal lost |
| orange | degraded or overridden: bypassed, moving, holdover, waiting |
| blue | informational state (roof Open) |
| grey | unknown: never seen, no sensor configured |

A sensor that has **never** reported is grey "Not seen", never green. Don't show "Safe" before you have data.

---

## 6. Text and labels

- **Toggle labels use one form:** `Label: Enabled` / `Label: Disabled`. Don't mix in "(ENABLED)", "ON", "Enable X" and so on. Write "GPS Module: Enabled", not "Enable GPS Module: ENABLED".
- **Booleans in tables** use one word pair per meaning and keep it on every page: Yes/No, Triggered/Not Triggered, Parked/Not Parked, ON/OFF.
- **Use the same name for the same thing on every page.** If Home says "Power Relay (K1)", Roof Control says the same. If Home's weather row says Safe / UNSAFE / No Sensor / BYPASSED, so does every other page.
- **Capitals only for alarms:** "UNSAFE" and "BYPASSED" are capitalised because they are warnings. Everything else is in normal case.
- **Help text** goes under the control in `<small>`, in muted colour, and describes what the setting does when it is **off** as well as on. Example: "When off, only the movement timeout catches a roof that doesn't move."
- **Don't repeat information.** Remove "Current Status" rows that duplicate the status bar, and "Current Configuration" blocks that just echo the form above them.
- **Version line** on every page, right under the `h1`: `<p class='page-sub'>Version: x.y.z</p>`.

---

## 7. Forms, toggles and buttons

### Inputs

```css
input[type=text], input[type=password], input[type=number] { display:block; width:100%; max-width:420px; ... }
input[type=number] { max-width: 200px; }
select { max-width: 420px; }
```

Full-width inputs on a wide card look broken. Cap them.

### Toggles

Use the shared `.switch` / `.slider` / `.switch-label`, inside a `.switch-container` (flex, left-aligned). Add `class='danger'` to a checkbox when turning it **on** is the risky direction, such as a bypass; it then shows red when on. **Left-align toggles and radio groups.** No centred or floated controls.

### Buttons

- Blue (`.button-primary`, or the default) for normal actions, including **Apply** and **Save**.
- Red (`.button-danger`) **only** for destructive actions: Restart, Remove, Factory Reset.
- Group buttons in `.button-row` (flex, wrap, 10 px gap).

### Saving forms: Post / Redirect / Get

A settings form POSTs, the handler saves, then **303-redirects back** to the page:

```cpp
webUiServer.sendHeader("Location", changed ? "/setup?saved=1" : "/setup", true);
webUiServer.send(303, "text/plain", "");
```

The page shows a green `.saved-banner` when `?saved=1` is present, and the tab script reopens the same tab (section 8). Never answer a form POST with a bare text page.

### Gotcha: checkboxes need a `name`, and an unchecked box sends nothing

A checkbox inside a `<form>` is only submitted if it has a `name`, and only **when checked**. The MQTT toggle had an `id` but no `name`, so "Save MQTT Settings" silently ignored it. The fix:

```html
<input type='checkbox' id='mqttEnabled' name='mqttEnabled' value='true' ...>
```

```cpp
// The enable toggle is a checkbox: it's only submitted when checked
bool newMqttEnabled = webUiServer.hasArg("mqttEnabled");
```

Only read "absent means off" inside the handler for that specific form. Check that the form's other fields are present first, so a POST from a different form doesn't turn the setting off.

---

## 8. Tabs (long settings pages)

Split a long setup page into tabs, one card or topic per tab:

```css
.tabs { display:flex; flex-wrap:wrap; gap:4px; border-bottom:2px solid #0f3460; margin-bottom:16px; }
.tabs button { margin:0; border-radius:6px 6px 0 0; background:#16213e; color:#b0b0b0; padding:10px 14px; font-weight:bold; }
.tabs button.active { background:#0f3460; color:#a0c4ff; }
.tab { display:none; } .tab.active { display:block; }
```

```js
function showTab(id){
  // show .tab#tab-<id>, mark its button active, then remember the choice:
  try { localStorage.setItem('setupTab', id); } catch(e) {}
  if (location.hash !== '#'+id) history.replaceState(null, '', '#'+id);
}
// On load: open the tab from the URL hash, else from localStorage, else the first tab.
```

All tabs are in the page at once. Switching tabs is instant and doesn't hit the device.

### Streaming large pages

Building a 40 KB page as one `String` can fragment the heap. Send it in pieces instead:

```cpp
webUiServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
webUiServer.send(200, "text/html", "");
buildSetupPage([](const String& part) { webUiServer.sendContent(part); });  // one tab per part
webUiServer.sendContent("");   // end of chunked response
```

---

## 9. Live updates

Pages render complete HTML at load, so they work without JS. Values that change are then refreshed from **one JSON endpoint** (`/api/status`) every 2 s.

Rules:
1. **One poll per page.** Hang new live content off the existing status-bar poll instead of adding another `setInterval(fetch...)`.
2. **The server formats the values.** For table cells, the JSON carries the cell's finished HTML (dot + label, "12s ago", "4.5 &deg;C"). The **same C++ helper** renders the cell at page load and fills the JSON, so the first render and the live updates can never disagree:

   ```cpp
   inline String sensorAgoText(unsigned long lastSeen);      // "Never" / "12s ago" / "3m ago" / "2h ago"
   inline String sensorTempText(float t);                    // "&mdash;" or "4.5 &deg;C"
   inline String safetySensorStatusHtml(const SafetySensor&); // dot + "Safe" / "UNSAFE" / "Offline" / "Not seen" / "Bypassed"
   ```

3. **Mark updatable cells, not whole rows.** Rows carry `data-sensor='<kind>:<id>'` and cells carry `data-f='status|temp|seen'`. The script updates only those cells. **Don't redraw toggles or buttons**: re-rendering a row while someone is clicking its switch undoes the click.

   ```js
   function updateSensorRows(list){
     var rows={};
     document.querySelectorAll('tr[data-sensor]').forEach(function(r){ rows[r.getAttribute('data-sensor')]=r; });
     list.forEach(function(s){
       var r=rows[s.id]; if (!r) { /* new sensor: show the "refresh to manage it" hint */ return; }
       r.querySelector("[data-f='status']").innerHTML=s.status;
       r.querySelector("[data-f='seen']").innerHTML='<small>'+s.seen+'</small>';
     });
   }
   ```

4. **New items** that appear while the page is open aren't added. A small hint ("New sensor discovered: refresh the page to manage it") is shown instead.
5. **Size the JSON document** for the worst case. `/api/status` went from 3 KB to 8 KB to hold 10 park and 10 safety sensor rows.

---

## 10. Live clocks

A clock updated by a 2 s poll jumps two seconds at a time. Instead, the browser runs the clock from its own time and **corrects the offset** from each poll:

- The device returns `unix_ms`, its time **in milliseconds**. The ROR takes this from PPS when locked. A device with an SNTP clock can use `gettimeofday()`. It also returns `total_offset`, the local offset in minutes.
- Any element with `data-clock='utc-time|utc-date|local-time|local-date'` ticks, on any page. The C++ only has to add the attribute:

  ```cpp
  html += "<td data-clock='utc-time'>" + getTimeString() + "</td>";   // server value until JS takes over
  ```

- Offset estimate: `offset = unix_ms + rtt/2 - receiveTime`, keeping the sample with the shortest round trip (accept a new one if its RTT is within 20 ms of the best, or the best is over 60 s old).
- Each tick schedules the next for just after the following **second boundary** (`setTimeout(tick, 1000 - ms % 1000 + 5)`), so the seconds change on time instead of drifting through the second.
- If `unix_ms` is 0 (no time source), the server-rendered value is left as it is.

Measured against an NTP-synced PC, the displayed clock is within a few milliseconds.

---

## 11. Integrating the ElegantOTA page

ElegantOTA serves its own full page at `/update`, which matches nothing else on the site. Rather than restyling the library, add a page in the site layout that **frames** it:

```cpp
inline String getFirmwarePage() {
  String html = getPageHeader("Firmware Update - Device");
  html += "<h1>Firmware Update</h1>\n";
  html += "<p class='page-sub'>Version: " + String(DEVICE_VERSION) + "</p>\n";
  html += getNavBar();
  html += R"rawhtml(
<div class='card'>
<p style='margin-top:0;color:#b0b0b0;font-size:0.9em'>Upload <code>firmware_ota.bin</code>. The device restarts when the upload finishes.</p>
<iframe id='ota' src='/update' title='Firmware upload'
        style='width:100%;height:520px;border:0;background:transparent;display:block' onload='otaLoaded()'></iframe>
</div>
<script>
try { localStorage.setItem('theme', 'dark'); } catch (e) {}       // ElegantOTA reads this
function otaLoaded() {
  var f = document.getElementById('ota');
  try {
    var doc = f.contentDocument;                                   // same origin, so allowed
    doc.documentElement.classList.add('dark');
    var st = doc.createElement('style');
    st.textContent = 'html,body,body>div{background:transparent!important}';
    doc.head.appendChild(st);                                      // let our card colour show through
  } catch (e) {}
}
setInterval(function () {                                          // grow/shrink with the upload UI
  var f = document.getElementById('ota');
  try {
    var h = f.contentDocument.documentElement.scrollHeight;
    if (h > 100 && Math.abs(h - f.offsetHeight) > 4) f.style.height = h + 'px';
  } catch (e) {}
}, 500);
</script>
</body></html>
)rawhtml";
  return html;
}
```

```cpp
webUiServer.on("/firmware", HTTP_GET, handleFirmware);   // and point the nav's Update link at /firmware
```

Why this works:
- ElegantOTA's page is built with Tailwind using **class-based dark mode**. It adds `dark` to `<html>` when `localStorage.theme === 'dark'`. The frame is **same-origin**, so the parent can set that value and can also reach into the frame's document.
- Clearing the frame's backgrounds makes ElegantOTA's dark controls sit directly on our card colour.
- `/update` still works on its own, so scripts and tools that POST to `/ota/start` and `/ota/upload` are unaffected.
- Leave ElegantOTA's own logo and link in place. They're part of the library's free-tier UI.

---

## 12. Gotchas

| Symptom | Cause | Fix |
|---|---|---|
| Status dots invisible on some pages | Colour classes defined in one page's `<style>` | All dot colours in `getCommonStyles()` |
| Values in a status table don't line up | Some cells start with a dot, some don't | Dot gutter (section 5) |
| A form "saves" but the toggle comes back | Checkbox has no `name`, or unchecked boxes aren't submitted | `name=` + `hasArg()` (section 7) |
| A live script silently does nothing | A global variable shadows a browser global (`var history` hid `window.history` in the console page) | Don't name globals `history`, `location`, `status`, `name` and so on; wrap code in a function |
| Clicking a toggle in a live table gets undone | Poll re-renders the whole row | Update only `data-f` cells (section 9) |
| Page truncated or heap errors on a large page | One huge `String` | Stream with `sendContent()` (section 8) |
| Odd characters in text | Non-ASCII in C++ string literals | ASCII source + HTML entities + `<meta charset='utf-8'>` |
| A shared script can't update every copy of a display | IDs are unique per page, and the same ID was reused for different displays (`localTimeDisplay` on Home and Setup) | Use `data-*` attributes for anything a shared script updates |
| Long JS in C++ is hard to read | `"...\n"` line-by-line strings | Use raw string literals: `R"rawjs( ... )rawjs"` |

---

## 13. Porting checklist (SkyConditions_90640 and others)

1. **Stylesheet:** bring `getCommonStyles()` and `getToggleSwitchStyles()` in line with section 2 and the CSS in sections 5 and 7. Settle on one card padding.
2. **Header:** every page uses `getPageHeader()`, then `h1`, then `.page-sub` version line, then `getNavBar()`. Remove per-page `<style>` blocks; move anything still needed into the shared stylesheet.
3. **Width:** `body` max 1040 px centred; tables max 860 px; status tables max 720 px; inputs max 420 px (numbers 200 px).
4. **Status bar:** add `getStatusCard()` to `getNavBar()` with that device's key states and an error line. Create `/api/status` if there isn't one.
5. **Dots and labels:** one dot size, global colour classes, the colour meanings in section 5, "Not seen" in grey for no data, and `Label: Enabled/Disabled` for toggles.
6. **Buttons:** blue for Save/Apply, red only for destructive actions.
7. **Forms:** Post/Redirect/Get with a saved banner. Check every checkbox in a `<form>` has a `name`.
8. **Tabs:** if the setup page is long, split it into tabs (section 8) and stream it. SkyConditions' `getSetupPage()` is a candidate.
9. **Live data:** reuse the status-bar poll and use server-formatted cells marked with `data-*`. SkyConditions already polls in several places (`setInterval` on the home, trends, sky history and console pages). Fold the home-page polls into the shared one. The chart pages' slower polls (30 s / 5 min) can stay as they are.
10. **Clocks:** add `unix_ms` and `total_offset` to `/api/status` and mark time displays with `data-clock`. SkyConditions' home page has a hand-rolled one-second counter (`setInterval(... ++sec ..., 1000)` in `getHomePage()`). If it displays time of day, switch it to `data-clock`. If it counts uptime or seconds since an update, it can stay.
11. **Update page:** add `/firmware` (section 11) and point the nav's **Update** link at it.
12. **Read through every page** for duplicated rows, inconsistent names for the same thing, centred blocks and orphaned colours.
