// SPDX-License-Identifier: AGPL-3.0-only
#include "i18n.h"

// Designated initializers, in declaration order: a renamed or reordered field
// fails to compile instead of silently shifting every string after it.
const Strings STRINGS[LANG_COUNT] = {
  // ===========================================================================
  // English
  // ===========================================================================
  {
    .code = "en",
    .name = "English",

    .scrMascot = "NIMBUS", .scrCode = "CODE", .scrDance = "DANCE", .scrSleep = "SLEEP",
    .scrRun = "RUN", .scrLife = "LIFE", .scrBall = "BALL", .scrMatrix = "MATRIX",
    .scrPlasma = "PLASMA", .scrFire = "FIRE", .scrStars = "STARS", .scrText = "TEXT",
    .scrRays = "RAYS", .scrBars = "BARS", .scrBlackHole = "BLACKHOLE", .scrCat = "CAT",
    .scrIss = "ISS DEMO", .scrClock = "CLOCK", .scrWeather = "WEATHER", .scrDate = "DATE",
    .scrSystem = "SYSTEM", .scrMoney = "MONEY", .scrSocial = "SOCIAL",
    .scrImage = "IMAGE",
    .mascotHi = "Hi!", .runBug = "BUG!", .runBest = "BEST",

    .codeThinking = "Thinking…", .codeWorking = "Working", .codeNeedsYou = "Needs you",
    .codeDone = "Done", .codeIdle = "Idle", .codeFailed = "Failed", .codeToolError = "Tool error",
    .codeWaiting = "Waiting", .codeNoWifi = "No Wi-Fi",
    .codeMin = "%u MIN", .codeMinShort = "%uM",
    .clkDays = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"},
    .clkMonths = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"},
    .clkWeek = "WEEK",
    .clkOutside = "OUTSIDE",
    .clkAm = "AM",
    .clkPm = "PM",
    .weatherLoading = "WEATHER...",
    .staleWords = {"Offline", "No key", "Server"},
    .wxCond = {"Clear", "Mainly clear", "Some clouds", "Overcast", "Fog", "Drizzle", "Icy rain",
               "Rain", "Showers", "Snow", "Storm"},
    .wxHour = "%uH",
    .wxHourAm = "%uA",
    .wxHourPm = "%uP",
    .working = "WORKING",
    .offHours = "OFF HOURS",
    .workingShort = "LIVE",
    .pausedShort = "PAUSE",
    .month = "MONTH",
    .decimalMark = ".",
    .moneySpace = " ",
    .socialNames = {"YOUTUBE", "INSTAGRAM", "X", "GITHUB"},
    .socialNamesTall = {"YOUTUBE", "INSTA", "X", "GITHUB"},
    .socialSubscribers = "SUBSCRIBERS",
    .socialFollowers = "FOLLOWERS",
    .socialMillions = "%sM",
    .weekPrefix = "W",
    .dayPrefix = "D",
    .dateWeek = "WEEK %u",
    .dateDay = "DAY %u",
    .sysRam = "FREE RAM",
    .sysRamTall = "RAM",
    .sysKb = "KB",
    .sysFps = "%u FPS",
    .sysUp = "UP %s",
    .sysUpTall = "UP",
    .sysUpMin = "%u MIN",
    .sysUpDay = "D",
    .near = "NEAR",
    .land = "LAND",
    .ocean = "OCEAN",
    .setup = "SETUP",
    .factoryReset = "RESET",
    .saved = "Saved",
    .phoneConnected = "PHONE JOINED",
    .wrongPassword = "WRONG PASSWORD",
    .setupLabel = "WI-FI SETUP",
    .setupJoin = "Join Wi-Fi",
    .setupOpen = "Or open",
    .setupScan = {"SCAN", "TO JOIN"},
    .setupTall = "WI-FI",
    .reachLabel = "OPEN THE PAGES",
    .reachScan = {"SCAN", "TO OPEN"},
    .signInPanel = "SIGN-IN CODE",
    .signInPanelTall = "CODE",
    .bootWord = "Carousel",
    .bootWordTall = {"Carou", "sel"},
    .days = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"},
    .months = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
               "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"},
    .imgOther = {"NO IMAGE", "FOR 64X32"},
    .imgNoStore = {"NO STORAGE", "SEE WEB"},
    .imgOtherTall = {"NO", "IMAGE", "FOR", "32X64"},
    .imgNoStoreTall = {"NO", "STORE", "SEE", "WEB"},
    .noImage = "No image",
    .noImageAsk = "ADD ONE AT",
    .noImageTall = {"No", "image", "ADD ONE", "IN THE", "PAGES"},
    .locked = {"LOCKED", "RESET NOT DONE", "RESTART IT"},
    .lockedTall = {"LOCKED", "RESTART", "IT"},

    .webSaved = "Settings saved.",
    .webOutOfMemory = "Not enough memory, reload the page.",
    .webNotSaved = "<b>Not saved:</b> the panel's memory refused the settings. Nothing "
                   "changed. Restart the panel and try again.",
    .webSavedLater = "Settings saved. The panel was busy: the screen list applies at its next "
                     "restart.",
    .webWaitingHint = "The panel is waiting. Install the hooks, then restart Claude Code.",
    .webClearSessions = "Clear the session list",
    .webHookAddress = "Hook address:",
    .webStates = "States:",
    .webSignal = "Wi-Fi signal",
    .webSignalHint = "Good above -67 dBm. Below -75, hooks arrive late: move the panel "
                     "closer to the router.",
    .webChangeWifi = "Change Wi-Fi",
    .webFreeMemory = "Free memory",
    .webMemoryMin = "min",
    .webLastUpdate = "Last update",
    .webMinutesAgo = "%lu min ago",
    .webNotYet = "not yet",
    .webThousands = ",",
    .webLanguage = "Language",
    .webSalary = "Net monthly salary (%s)",
    .webDayStart = "Day starts",
    .webDayEnd = "Day ends",
    .webDaysPerMonth = "Days / month",
    .webIncomeHint = "The counter goes up live during working hours.",
    .webYtKey = "API key (Google Cloud, YouTube Data v3)",
    .webYtChannel = "Channel ID",
    .webYtChannelHint = "Shown in YouTube Studio, Settings, Advanced.",
    .webGhUser = "Username",
    .webGhHint = "No key needed.",
    .webManual = "Manual entry",
    .webInstagram = "Instagram followers",
    .webX = "X followers",
    .webManualHint = "These two APIs are closed or paid: update the numbers here.",
    .webSave = "Save",
    .webUnsaved = "Unsaved changes",
    .webDiscard = "Discard",
    .webCity = "City",
    .webCityFound = "Found",
    .webCityNotFound = "not found, check the spelling",
    .webCitySearching = "searching...",
    .webCityHint = "Weather, time zone and the ISS distance follow it. Set at Wi-Fi setup from "
                   "your phone's time zone; type your own city to be precise.",
    .webCityMatches = "Places found: %s. Use the up and down arrows.",
    .webCityNone = "No place found.",
    .webLastRestart = "Last restart",
    .webFirmware = "Firmware",
    .updChecking = "checking...",
    .updUpToDate = "up to date",
    .updAvailable = "available",
    .updInstalling = "installing...",
    .updCheck = "Check for updates",
    .updInstall = "Install",
    .updAuto = "Install signed updates by itself at night",
    .webMessage = "Scrolling text",
    .webNightFrom = "Night from",
    .webNightTo = "until",
    .webNightLevel = "Night brightness",
    .webNightOff = "off",
    .webNightHint = "Hours from 0 to 23. Same hours: no night mode. At night, pressing the knob "
                    "brings back the day brightness for a minute.",
    .webCredits = "Weather: %s \xC2\xB7 Places: %s",
    .webDisclaimer = "Unofficial project, not affiliated with or endorsed by Anthropic. "
                     "Claude and Claude Code are trademarks of Anthropic, PBC.",
    .webSource = "Source code",
    .webMqttOff = "off",
    .webMqttOn = "connected",
    .webMqttDown = "not connected",
    .webMqttHost = "Broker",
    .webMqttPort = "Port",
    .webMqttUser = "User",
    .webMqttPass = "Password",
    .webMqttHint = "Home Assistant then finds the panel by itself (MQTT discovery): screen, "
                   "brightness, message, notifications. Empty broker: MQTT off.",
    .webApiHint = "For scripts and home automation: send it as \"Authorization: Bearer "
                  "<token>\" to /api/... (see docs/api.md). Signing out every other browser "
                  "also replaces it.",
    .webResetFailed = "The factory reset could not finish. Until it does, the panel uses no "
                      "saved setting, network or image. Restart the panel: it tries the reset "
                      "again at each start. If this message stays, reinstall the firmware over "
                      "USB as the README's \"Build and flash\" section explains.",
    .webResetFailedKnob = "The reset did not finish and will not start again by itself. Hold "
                          "the knob at the next start to run it again, and do not hand the "
                          "panel over until it restarts on the Wi-Fi setup screen.",
    .resetFailedTitle = "Factory reset unfinished",

    // \u00A0: no-break space between a number and its unit
    .webImages = "Images",
    .webImgSpace = "Space",
    .webImgUsedOf = "%lu\u00A0KB used of %lu\u00A0KB",
    .webKb = "KB",
    .webImgNone = "No image yet.",
    .webImgAdd = "Add a GIF",
    .webImgAddHint = "Still or animated, 64×32 (landscape) or 32×64 (portrait), up to "
                     "96\u00A0KB. Names over 40 characters are shortened.",
    .webImgUpload = "Upload",
    .webImgOnPanel = "On the panel (%d)",
    .webImgLandscape = "Landscape",
    .webImgPortrait = "Portrait",
    .webImgDelete = "Delete",
    .webImgShows = "Panel shows",
    .webImgAll = "All in turn",
    .webImgShowHint = "An image shows only in its own orientation. Turned the other way, the "
                      "panel shows that orientation's images in turn.",
    .webImgShow = "Show",
    .webImgAdded = "<b>%s</b> added.",
    .webImgReplaced = "<b>%s</b> replaced.",
    .webImgNotAdded = "Not added:",
    .webImgNotGif = "%s is a %s, not a GIF. Export it as GIF and upload it again.",
    .webImgNotGifOther = "%s is not a GIF. Export it as GIF and upload it again.",
    .webImgWrongSize = "%s is %u×%u pixels. The panel shows 64×32 (landscape) or "
                       "32×64 (portrait).",
    .webImgNoRoom = "%s needs %lu\u00A0KB and %lu\u00A0KB are left. Delete an image to make "
                    "room.",
    // 24 and 96 KB below are MAX_IMAGES and MAX_IMAGE_BYTES (content_logic.h):
    // change them together
    .webImgTooMany = "%s would be image 25, and the panel holds 24. Delete an image to make "
                     "room.",
    .webImgTooBig = "The file is over 96\u00A0KB, the most an image can take. Use fewer frames "
                    "or colours and upload it again.",
    .webImgFailed = "The panel could not save the file. Upload it again; if it fails twice, "
                    "restart the panel.",
    .webImgNoFile = "No file was chosen. Choose a GIF, then press Upload.",
    .webImgNa = "Not available",
    .webImgNaText = "Images need a storage area that this panel's first install did not create. "
                    "Reinstalling the firmware once over USB adds it. Everything else works as "
                    "before.",
    .webImgNaLink = "How to reinstall over USB",

    .webChangeScreen = "Change screen",
    .webRotate = {"With the knob only", "Every 10\u00A0s", "Every 20\u00A0s", "Every 30\u00A0s",
                  "Every minute", "Every 2\u00A0minutes", "Every 5\u00A0minutes"},
    .webRotateHint = "Automatic changes pause when you touch the knob and resume 30\u00A0s "
                     "after your last touch.",
    .webInOrder = "In order, %u of %u on",
    .webOrderHint = "Untick a screen to skip it. Drag it, or use its arrows, to move it; "
                    "Save keeps the order.",
    .webLandscapeOnly = "Landscape only",
    .webPortraitOnly = "Portrait only",
    .webMoveUp = "Move %s up",
    .webMoveDown = "Move %s down",
    .webRefusedEmpty = "<b>Not saved.</b> Turn on at least one screen.",
    .webRefusedPortrait = "<b>Not saved.</b> Turn on a screen not marked “Landscape only”.",
    .webRefusedLandscape = "<b>Not saved.</b> Turn on a screen not marked “Portrait only”.",
    .webTitleValue = "%s: %s",
    .webImgOff = "IMAGE is off in Running order, so the panel does not show it.",
    .webStyle = "Style",
    .codeStyles = {"Auto", "Cloud", "List"},
    .codeStyleHints = {"The cloud for one or two sessions, the list from three.",
                       "The mascot, the state in a word, the project and the time.",
                       "One row per session, up to four; the one that needs you in orange."},
    .clockStyles = {"Stone", "Dial", "Alternate"},
    .clockStyleHints = {"Big digits, the date above, the seconds as a line.",
                        "A dial with hands, the date beside it.",
                        "Big digits, one line below that changes every 4 s."},
    .weatherStyles = {"Glance", "Scene", "Day"},
    .weatherStyleHints = {"Icon, temperature, conditions, low and high.",
                          "Today's illustrated scene, the temperature small.",
                          "Now and the next three slots: hour, icon, temperature."},
    .webClockShows = "Shows",
    .clockShows = {"Date", "Seconds line", "Week number", "Outside temperature"},
    .clockShowsShort = {"date", "seconds", "week", "outside"},

    .signInTitle = "Sign in",
    .signInHint = "Type the 6-digit code the panel shows now. This browser will then be "
                  "remembered.",
    .signInCode = "Code",
    .signInButton = "Sign in",
    .signInWrong = "Wrong or expired code. Reload the page for a new one (after 5 mistakes, "
                   "wait 30\u00A0s).",
    .signOutAll = "Sign out every other browser",

    .wifiTitle = "Panel Wi-Fi",
    .wifiSubtitle = "The panel remembers this network, even after a power cut.",
    .wifiSaved = "Saved network",
    .wifiState = "State",
    .wifiStateSetup = "setup access point",
    .wifiStateConnected = "connected,",
    .wifiStateOffline = "not connected",
    .wifiRefresh = "Refresh the list",
    .wifiPassword = "Password",
    .wifiPasswordHint = "Leave it empty for an open network.",
    .wifiFooter = "The panel restarts and joins this network. Claude Code hooks find it "
                  "again by themselves.",
    .wifiBand = "The panel only sees 2.4\u00A0GHz networks. If your box gives the 5\u00A0GHz "
                "band its own name, pick the other one.",
    .wifiLastTry = "Last attempt",
    .wifiRefused = "password refused",
    .wifiNotFound = "network not found",
    .wifiMsgRetry = "The panel restarts and retries the saved network. You can disconnect.",
    .wifiMsgBadSsid = "The network name must be 1 to 32 characters long.",
    .wifiMsgBadPassword = "A Wi-Fi password is 8 to 63 characters long.",

    // --- Web: Shell (theatre pages) ---
    .webTabs = {"Stage", "Running order", "Images", "Panel"},
    .webSkip = "Skip to content",
    .webNavName = "Pages",
    .webSetupBrand = "Carousel setup",
    .webTitle = "%s \u00B7 Carousel",

    // --- Web: Wi-Fi (theatre pages) ---
    .wifiAnother = "Another network, by name",
    .wifiName = "Network name",
    .wifiMsgSaved = "Saved. The panel restarts and joins %s. You can disconnect.",
    .wifiNoneYet = "No network found yet.",
    .wifiKnown = "Known network",
    .wifiOthers = "Other networks",
    .wifiNetworks = "Networks",
    .wifiAway = "%s is not in range right now.",
    .wifiSignal = {"weak", "fair", "good", "excellent"},
    .wifiSignalLocked = "%s signal, secured",
    .wifiSignalOpen = "%s signal, open",
    .wifiContinue = "Continue",

    // --- Web: Sign-in and locked (theatre pages) ---
    .signInLive = "code on the panel now",
    .signInCaption = "The digits stay on the panel; this page never shows them.",
    .signInFrame = "The panel shows CODE, a 6-digit code (hidden here) and a bar for the time left",
    .signInReload = "No code on the panel? Reload this page: opening it puts a new code on the "
                    "panel for 2\u00A0minutes.",
    .lockedLive = "on the panel now",
    .signInDigit = "Digit %d of 6",

    // --- Web: Error pages (theatre pages) ---
    .errTitle = {"Page not found", "Request refused", "Too long to send", "Form not received",
                 "Short of memory"},
    .errText = {"There is no page at this address: start again from Stage.",
                "It came from another site or address: open the panel at its own address.",
                "It is longer than the panel takes: shorten it and send it again.",
                "It arrived incomplete or in another format: send it again from the page.",
                "The panel ran short of memory: wait a few seconds and try again."},
    .errBack = "Back to %s",

    // --- Web: Stage (theatre pages) ---
    .stgLive = "on stage",
    .stgPlace = "screen %u of %u",
    .stgNowSr = ", on stage now",
    .stgNext = "Next: %s in %lu\u00A0s",
    .stgEvery = {"a new screen every 10\u00A0s", "a new screen every 20\u00A0s",
                 "a new screen every 30\u00A0s", "a new screen every minute",
                 "a new screen every 2\u00A0minutes", "a new screen every 5\u00A0minutes"},
    .stgKnobOnly = "Changes with the knob only",
    .stgNoOther = "No other screen to change to in this orientation",
    .stgPreviewAlt = "What the panel shows now: %s",
    .stgStill = "A still from when this page opened. Reload to see it again.",
    .stgAllScreens = "All %u screens and their order",
    .stgSessions = "Coding sessions",
    .stgSessionsHint = "Claude Code sessions the panel hears from right now. They show on the "
                       "CODE screen.",
    .stgStates = {"waiting", "thinking", "using a tool", "needs you", "error", "finished",
                  "failed"},
    .stgAgoSec = "%lu\u00A0s ago",
    .stgAgoMin = "%lu\u00A0min ago",
    .stgAgoHour = "%lu\u00A0h ago",
    .stgSetUpHooks = "Set up the hooks",
    .stgBring = "Bring on a screen",
    .stgScreen = "Screen",
    .stgLights = "Brightness",
    .stgDimmer = "Dimmer",
    .stgBrighter = "Brighter",
    .stgSteps = "Brightness %d of %d",
    .stgNightDims = "Level %d of %d. At night, from %u:00 to %u:00, the panel dims to %s.",
    .stgNightDark = "Level %d of %d. At night, from %u:00 to %u:00, the panel goes dark.",
    .stgNightNone = "Level %d of %d. No night mode.",
    .webNightMode = "Night mode",
    .stgSay = "Say something",
    .stgSend = "Send",
    .stgSayHint = "The TEXT screen scrolls it, and it comes on stage now.",
    .stgTextOff = "Saved. TEXT is off in Running order, so the panel does not show it.",
    .stgTextPortrait = "Saved. TEXT shows in landscape only: turn the panel to see it.",
    .stgTextFailed = "Not saved: the panel's memory refused the message. Try again; if it fails "
                     "twice, restart the panel.",
    .stgShowOff = "Not shown: %s is off in Running order. Turn it on there first.",
    .stgShowOther = "Not shown: %s is drawn for the other orientation.",
    .stgShowUnknown = "Not shown: the panel has no screen by that name.",
    .stgSent = "Sent: the panel shows it now.",
    .stgFailed = "That did not go through: reload the page and try again.",

    // --- Web: Images (theatre pages) ---
    .imgLead = "Your own GIFs, shown in IMAGE's turn of the running order.",
    .imgFileLabel = "GIF file",
    .imgShowsThis = "Panel shows this",
    .imgDeleteName = "Delete %s",
    .imgSelectLabel = "Image",
    .imgEmptyHint = "Until you add one, the IMAGE screen shows the frame above. Add a GIF: the "
                    "panel shows it at once, then in IMAGE's turn.",

    // --- Web: Running order (theatre pages) ---
    .ordLead = "What the panel plays, in this order. The knob and Home Assistant follow this "
               "list.",
    .ordCount = "%u of %u on",
    .ordImageShows = "Shows %s. Add, delete or choose the image on the Images page.",
    .ordImageAll = "Shows every image in turn. Add, delete or choose the image on the Images "
                   "page.",
    .ordOpenImages = "Open Images",
    .ordSettingsOf = "%s settings",
    .ordHandle = "Move %s, position %s of %s",
    .ordMoneySum = "%s\u00A0%s · %u–%u\u00A0h",
    .ordImagesAll = "%d images, all in turn",

    // --- Web: Panel (theatre pages) ---
    .pnlLead = "The object itself: its language, its region, its nights, its network and what it talks to.",
    .pnlUpdates = "Updates",
    .pnlWifi = "Wi-Fi",
    .pnlAdvanced = "Advanced",
    .pnlLanguageLabel = "Language of the pages and the panel",
    .pnlSaveScope = "Saves language, region, night mode, feedback, updates and Home Assistant.",
    .pnlRegion = "Region",
    .pnlTemperature = "Temperature",
    .pnlCurrency = "Currency",
    .pnlClock = "Clock",
    .pnlRegionHint = "How the panel writes temperatures, money and the time.",
    .pnlFeedback = "Feedback",
    .pnlTransition = "Transition",
    .pnlBrightnessChange = "Brightness change",
    .pnlKnobTurn = "Knob turn",
    .pnlRequests = "Requests",
    .pnlTransitions = {"Push", "Dissolve", "Wipe", "Cut"},
    .pnlTransitionHints = {"The next screen pushes the last one out.",
                           "One screen melts into the next.",
                           "A line sweeps the next screen in.",
                           "The next screen simply replaces the last."},
    .pnlBrightnessFeedbacks = {"Dots", "Gauge", "Number", "None"},
    .pnlBrightnessHints = {"A band at the bottom: a sun and six steps.",
                           "A gauge down the side.",
                           "One big number.",
                           "Nothing: the brightness just changes."},
    .pnlKnobFeedbacks = {"Band", "Neighbours", "Dots", "None"},
    .pnlKnobHints = {"A band with the screen name and its place.",
                     "The screens before and after.",
                     "One dot per screen, the current one lit.",
                     "Nothing: the screen just changes."},
    .pnlRequestKinds = {"Messages", "Images", "Commands", "Coding agent"},
    .pnlRequestShows = {"Show", "Discreet", "Off"},
    .pnlRequestShowHints = {"In its own way: a band, the full screen or a chip.",
                            "A small chip in a corner.",
                            "Only the change itself."},
    .pnlBoot = "Boot screen",
    .pnlBoots = {"Auto", "Simple", "Detailed"},
    .pnlBootHint = "Simple shows the Carousel name and its loading line. "
                   "Detailed adds the IP address under the line. "
                   "Auto: detailed on a development build, simple on a release.",
    .pnlSignal = {"good", "fair", "weak"},
    .pnlMqttTitle = "Home Assistant (MQTT): %s",
    .pnlHooksNone = "Coding agent hooks: no session",
    .pnlHooksOne = "Coding agent hooks: 1 session",
    .pnlHooksMany = "Coding agent hooks: %s sessions",
    .pnlHooksHint = "A session closed without its end hook stays listed for 15\u00A0minutes.",
    .pnlHookInstall = "Install on a computer",
    .pnlHookInstallHint = "Run it in a terminal on each computer on this network. It finds "
                          "Claude Code, Codex, Cursor and Gemini CLI and sets up their hooks.",
    .pnlHookUninstall = "To remove them:",
    .pnlCopy = "Copy",
    .pnlCopied = "Copied",
    .pnlCopyByHand = "Select the command and copy it",
    .pnlHookRefused = "%s hook calls refused since the panel started: no token, or an old one. "
                      "Run the command again on each computer.",
    .pnlHookOpenOff = "Hooks without a token: refused",
    .pnlHookOpenOn = "Hooks without a token: accepted (for development)",
    .pnlHookAccept = "Accept hooks without a token",
    .pnlHookRefuse = "Refuse hooks without a token",
    .pnlHookOpenWarn = "Any device on this network can then change what CODE shows.",
    .pnlHookRenew = "Renew the token",
    .pnlHookRenewHint = "The command changes. Computers set up with the old one stop until you "
                        "run the new one there.",
    .pnlHookLegacy = "French names and /claude/ still work.",
    .pnlApiTitle = "Local API token and browsers",
    .pnlToken = "Token",
    .pnlMemoryTitle = "Memory and restarts",
    .pnlRestartHint = "To restart the panel, unplug it for a second; it comes back on the screen "
                      "of your last knob turn.",
  },

  // ===========================================================================
  // Français
  // ===========================================================================
  {
    .code = "fr",
    .name = "Français",

    .scrMascot = "NIMBUS", .scrCode = "CODE", .scrDance = "DANSE", .scrSleep = "DODO",
    .scrRun = "COURSE", .scrLife = "VIE", .scrBall = "BALLE", .scrMatrix = "MATRIX",
    .scrPlasma = "PLASMA", .scrFire = "FEU", .scrStars = "ETOILES", .scrText = "TEXTE",
    .scrRays = "RAYONS", .scrBars = "BARRES", .scrBlackHole = "TROU NOIR", .scrCat = "CHAT",
    .scrIss = "ISS DEMO", .scrClock = "HORLOGE", .scrWeather = "METEO", .scrDate = "DATE",
    .scrSystem = "SYSTEME", .scrMoney = "ARGENT", .scrSocial = "SOCIAL",
    .scrImage = "IMAGE",
    .mascotHi = "Salut", .runBug = "BUG\u202F!", .runBest = "RECORD",

    .codeThinking = "Réfléchit…", .codeWorking = "Travaille",
    .codeNeedsYou = "Vous attend", .codeDone = "Terminé", .codeIdle = "Au repos",
    .codeFailed = "Échec", .codeToolError = "Erreur outil",
    .codeWaiting = "En attente", .codeNoWifi = "Sans Wi-Fi",
    .codeMin = "%u MIN", .codeMinShort = "%uM",
    .clkDays = {"DIM.", "LUN.", "MAR.", "MER.", "JEU.", "VEN.", "SAM."},
    .clkMonths = {"JANV.", "F\xC3\x89VR.", "MARS", "AVR.", "MAI", "JUIN",
                  "JUIL.", "AO\xC3\x9BT", "SEPT.", "OCT.", "NOV.", "D\xC3\x89" "C."},
    .clkWeek = "SEMAINE",
    .clkOutside = "DEHORS",
    .clkAm = "AM",
    .clkPm = "PM",
    .weatherLoading = "METEO...",
    .staleWords = {"Hors ligne", "Sans clé", "Serveur"},
    .wxCond = {"Dégagé", "Beau temps", "Éclaircies", "Couvert", "Brouillard", "Bruine", "Verglas",
               "Pluie", "Averses", "Neige", "Orage"},
    .wxHour = "%uH",
    .wxHourAm = "%uA",
    .wxHourPm = "%uP",
    .working = "EN COURS",
    .offHours = "REPOS",
    .workingShort = "ACTIF",
    .pausedShort = "PAUSE",
    .month = "MOIS",
    .decimalMark = ",",
    .moneySpace = "\u202F",
    .socialNames = {"YOUTUBE", "INSTAGRAM", "X", "GITHUB"},
    .socialNamesTall = {"YOUTUBE", "INSTA", "X", "GITHUB"},
    .socialSubscribers = "ABONNÉS",
    .socialFollowers = "ABONNÉS",
    .socialMillions = "%s\u202FM",
    .weekPrefix = "S",
    .dayPrefix = "J",
    .dateWeek = "SEM. %u",
    .dateDay = "JOUR %u",
    .sysRam = "RAM LIBRE",
    .sysRamTall = "RAM",
    .sysKb = "KO",
    .sysFps = "%u\u202FIPS",
    .sysUp = "ALLUMÉ %s",
    .sysUpTall = "ALLUMÉ",
    .sysUpMin = "%u MIN",
    .sysUpDay = "J",
    .near = "PRES",
    .land = "TERRE",
    .ocean = "OCEAN",
    .setup = "REGLAGE",
    .factoryReset = "RAZ",
    .saved = "Enregistré",
    .phoneConnected = "TÉLÉPHONE RELIÉ",
    .wrongPassword = "MDP REFUSÉ",
    .setupLabel = "INSTALLATION",
    .setupJoin = "Rejoignez",
    .setupOpen = "Ou ouvrez",
    .setupScan = {"SCANNEZ", "LE CODE"},
    .setupTall = "WI-FI",
    .reachLabel = "VERS LES PAGES",
    .reachScan = {"SCANNEZ", "LE CODE"},
    .signInPanel = "CODE D’ACCÈS",
    .signInPanelTall = "CODE",
    .bootWord = "Carousel",                           // the product's name
    .bootWordTall = {"Carou", "sel"},
    .days = {"DIMANCHE", "LUNDI", "MARDI", "MERCREDI", "JEUDI", "VENDREDI", "SAMEDI"},
    .months = {"JANVIER", "FÉVRIER", "MARS", "AVRIL", "MAI", "JUIN",
               "JUILLET", "AOÛT", "SEPTEMBRE", "OCTOBRE", "NOVEMBRE", "DÉCEMBRE"},
    .imgOther = {"SANS IMAGE", "EN 64X32"},
    .imgNoStore = {"PAS DISPO", "VOIR WEB"},
    .imgOtherTall = {"SANS", "IMAGE", "EN", "32X64"},
    .imgNoStoreTall = {"PAS", "DISPO", "VOIR", "WEB"},
    .noImage = "Pas d’image",
    .noImageAsk = "AJOUTEZ-EN SUR",
    .noImageTall = {"Sans", "image", "AJOUTEZ", "UNE", "IMAGE"},
    .locked = {"VERROUILLÉ", "RAZ INACHEVÉE", "REDÉMARREZ"},
    .lockedTall = {"BLOQUÉ", "REDÉ-", "MARREZ"},

    .webSaved = "Réglages enregistrés.",
    .webOutOfMemory = "Mémoire insuffisante, rechargez la page.",
    .webNotSaved = "<b>Pas enregistré\u202F:</b> la mémoire du panneau a refusé les "
                   "réglages. Rien n'a changé. Redémarrez le panneau et réessayez.",
    .webSavedLater = "Réglages enregistrés. Le panneau était occupé\u202F: la liste des écrans "
                     "s'applique à son prochain redémarrage.",
    .webWaitingHint = "Le panneau attend. Installez les hooks, puis relancez Claude Code.",
    .webClearSessions = "Vider la liste des sessions",
    .webHookAddress = "Adresse des hooks\u202F:",
    .webStates = "États\u202F:",
    .webSignal = "Signal Wi-Fi",
    .webSignalHint = "Bon au-dessus de -67 dBm. En dessous de -75, les hooks arrivent en "
                     "retard : rapprochez le panneau de la box.",
    .webChangeWifi = "Changer de Wi-Fi",
    .webFreeMemory = "Mémoire libre",
    .webMemoryMin = "plancher",
    .webLastUpdate = "Dernière mise à jour",
    .webMinutesAgo = "il y a %lu min",
    .webNotYet = "pas encore",
    .webThousands = "\u202F",
    .webLanguage = "Langue",
    .webSalary = "Salaire net mensuel (%s)",
    .webDayStart = "Début de journée",
    .webDayEnd = "Fin de journée",
    .webDaysPerMonth = "Jours / mois",
    .webIncomeHint = "Le compteur monte en direct pendant les heures de travail.",
    .webYtKey = "Clé API (Google Cloud, YouTube Data v3)",
    .webYtChannel = "Identifiant de chaîne",
    .webYtChannelHint = "Visible dans YouTube Studio, Paramètres, Avancé.",
    .webGhUser = "Pseudo",
    .webGhHint = "Aucune clé requise.",
    .webManual = "Saisie manuelle",
    .webInstagram = "Abonnés Instagram",
    .webX = "Abonnés X",
    .webManualHint = "Ces deux API sont fermées ou payantes : mettez le chiffre à jour ici.",
    .webSave = "Enregistrer",
    .webUnsaved = "Modifications non enregistrées",
    .webDiscard = "Ignorer",
    .webCity = "Ville",
    .webCityFound = "Trouvé",
    .webCityNotFound = "introuvable, vérifiez l'orthographe",
    .webCitySearching = "recherche...",
    .webCityHint = "La météo, le fuseau horaire et la distance de l'ISS la suivent. Réglée à la "
                   "configuration du Wi-Fi d'après le fuseau de votre téléphone ; tapez votre ville "
                   "pour être précis.",
    .webCityMatches = "Lieux trouvés : %s. Utilisez les flèches haut et bas.",
    .webCityNone = "Aucun lieu trouvé.",
    .webLastRestart = "Dernier redémarrage",
    .webFirmware = "Firmware",
    .updChecking = "vérification...",
    .updUpToDate = "à jour",
    .updAvailable = "disponible",
    .updInstalling = "installation...",
    .updCheck = "Vérifier les mises à jour",
    .updInstall = "Installer",
    .updAuto = "Installer seul les mises à jour signées, la nuit",
    .webMessage = "Texte défilant",
    .webNightFrom = "Nuit de",
    .webNightTo = "à",
    .webNightLevel = "Luminosité de nuit",
    .webNightOff = "éteint",
    .webNightHint = "Heures de 0 à 23. Mêmes heures\u202F: pas de mode nuit. La nuit, appuyer sur "
                    "le bouton rend la luminosité de jour pendant une minute.",
    .webCredits = "M\xC3\xA9t\xC3\xA9o\u202F: %s \xC2\xB7 Lieux\u202F: %s",
    .webDisclaimer = "Projet non officiel, ni affilié à Anthropic ni approuvé par Anthropic. "
                     "Claude et Claude Code sont des marques d'Anthropic, PBC.",
    .webSource = "Code source",
    .webMqttOff = "désactivé",
    .webMqttOn = "connecté",
    .webMqttDown = "non connecté",
    .webMqttHost = "Broker",
    .webMqttPort = "Port",
    .webMqttUser = "Utilisateur",
    .webMqttPass = "Mot de passe",
    .webMqttHint = "Home Assistant trouve alors le panneau tout seul (découverte MQTT) : écran, "
                   "luminosité, message, notifications. Broker vide : MQTT désactivé.",
    .webApiHint = "Pour les scripts et la domotique\u202F: envoyez-le en «\u202FAuthorization: "
                  "Bearer <token>\u202F» à /api/... (voir docs/api.md). Déconnecter tous les "
                  "autres navigateurs le remplace aussi.",
    .webResetFailed = "La remise à zéro n'a pas pu aller au bout. En attendant, le panneau "
                      "n'utilise aucun réglage, réseau ou image enregistré. Redémarrez le "
                      "panneau\u202F: il retente la remise à zéro à chaque démarrage. Si ce "
                      "message reste, réinstallez le firmware par USB comme l'explique la "
                      "section «\u202FBuild and flash\u202F» du README.",
    .webResetFailedKnob = "La remise à zéro n'a pas abouti et ne repartira pas seule. Maintenez "
                          "le bouton au prochain démarrage pour la relancer, et ne donnez pas le "
                          "panneau tant qu'il ne redémarre pas sur l'écran de configuration "
                          "Wi-Fi.",
    .resetFailedTitle = "Remise à zéro inachevée",

    // \u202F: narrow no-break space before : ; ? ! -- \u00A0: before a unit
    .webImages = "Images",
    .webImgSpace = "Espace",
    .webImgUsedOf = "%lu\u00A0Ko utilisés sur %lu\u00A0Ko",
    .webKb = "Ko",
    .webImgNone = "Aucune image pour l'instant.",
    .webImgAdd = "Ajouter un GIF",
    .webImgAddHint = "Fixe ou animé, 64×32 (paysage) ou 32×64 (portrait), 96\u00A0Ko au "
                     "plus. Un nom de plus de 40 caractères est raccourci.",
    .webImgUpload = "Envoyer",
    .webImgOnPanel = "Sur le panneau (%d)",
    .webImgLandscape = "Paysage",
    .webImgPortrait = "Portrait",
    .webImgDelete = "Supprimer",
    .webImgShows = "Le panneau montre",
    .webImgAll = "Toutes à tour de rôle",
    .webImgShowHint = "Une image ne s'affiche que dans son orientation. Tourné dans l'autre "
                      "sens, le panneau montre à tour de rôle les images de cette orientation.",
    .webImgShow = "Afficher",
    .webImgAdded = "<b>%s</b> ajoutée.",
    .webImgReplaced = "<b>%s</b> remplacée.",
    .webImgNotAdded = "Pas ajoutée\u202F:",
    .webImgNotGif = "%s est un %s, pas un GIF. Exportez-la en GIF et envoyez-la à nouveau.",
    .webImgNotGifOther = "%s n'est pas un GIF. Exportez-la en GIF et envoyez-la à nouveau.",
    .webImgWrongSize = "%s fait %u×%u pixels. Le panneau affiche du 64×32 (paysage) ou du "
                       "32×64 (portrait).",
    .webImgNoRoom = "%s demande %lu\u00A0Ko et il en reste %lu. Supprimez une image pour faire "
                    "de la place.",
    // 24 and 96 Ko: MAX_IMAGES and MAX_IMAGE_BYTES, as in English
    .webImgTooMany = "%s serait la 25\u1D49 image, et le panneau en garde 24. Supprimez une image pour "
                     "faire de la place.",
    .webImgTooBig = "Le fichier dépasse 96\u00A0Ko, le maximum pour une image. Réduisez le nombre "
                    "d'images ou de couleurs et envoyez-le à nouveau.",
    .webImgFailed = "Le panneau n'a pas pu enregistrer le fichier. Envoyez-le à nouveau\u202F; si "
                    "ça recommence, redémarrez le panneau.",
    .webImgNoFile = "Aucun fichier choisi. Choisissez un GIF, puis appuyez sur Envoyer.",
    .webImgNa = "Indisponible",
    .webImgNaText = "Les images ont besoin d'un espace de stockage que la première installation "
                    "de ce panneau n'a pas créé. Réinstallez le firmware une fois par USB pour "
                    "l'ajouter. Tout le reste marche comme avant.",
    .webImgNaLink = "Réinstaller par USB",

    .webChangeScreen = "Changer d'écran",
    .webRotate = {"Au bouton seulement", "Toutes les 10\u00A0s", "Toutes les 20\u00A0s",
                  "Toutes les 30\u00A0s", "Toutes les minutes", "Toutes les 2\u00A0minutes",
                  "Toutes les 5\u00A0minutes"},
    .webRotateHint = "Les changements automatiques se mettent en pause quand vous touchez le "
                     "bouton et reprennent 30\u00A0s après votre dernière touche.",
    .webInOrder = "Dans l'ordre, %u sur %u allumés",
    .webOrderHint = "Décochez un écran pour le sauter. Faites-le glisser, ou utilisez ses flèches, "
                    "pour le déplacer\u202F; Enregistrer garde l'ordre.",
    .webLandscapeOnly = "Paysage seulement",
    .webPortraitOnly = "Portrait seulement",
    .webMoveUp = "Monter %s",
    .webMoveDown = "Descendre %s",
    .webRefusedEmpty = "<b>Pas enregistré.</b> Allumez au moins un écran.",
    .webRefusedPortrait = "<b>Pas enregistré.</b> Allumez un écran non marqué "
                          "«\u202FPaysage seulement\u202F».",
    .webRefusedLandscape = "<b>Pas enregistré.</b> Allumez un écran non marqué "
                           "«\u202FPortrait seulement\u202F».",
    .webTitleValue = "%s\u202F: %s",
    .webImgOff = "IMAGE est éteint dans l'ordre de passage, donc le panneau ne la montre "
                 "pas.",
    .webStyle = "Style",
    .codeStyles = {"Auto", "Nuage", "Liste"},
    .codeStyleHints = {"Le nuage pour une ou deux sessions, la liste dès trois.",
                       "La mascotte, l'état en un mot, le projet et la durée.",
                       "Une ligne par session, jusqu'à quatre ; celle qui vous attend en "
                       "orange."},
    .clockStyles = {"Pierre", "Cadran", "Alternance"},
    .clockStyleHints = {"Grands chiffres, la date au-dessus, les secondes en trait.",
                        "Un cadran à aiguilles, la date à côté.",
                        "Grands chiffres, une ligne dessous qui change toutes les 4 s."},
    .weatherStyles = {"Coup d'œil", "Scène", "Journée"},
    .weatherStyleHints = {"Icône, température, conditions, minimum et maximum.",
                          "La scène illustrée du jour, la température en petit.",
                          "Maintenant et les trois créneaux suivants : heure, icône, "
                          "température."},
    .webClockShows = "Affiche",
    .clockShows = {"Date", "Ligne des secondes", "Numéro de semaine", "Température extérieure"},
    .clockShowsShort = {"date", "secondes", "semaine", "extérieur"},

    .signInTitle = "Connexion",
    .signInHint = "Tapez le code à 6 chiffres affiché en ce moment sur le panneau. Ce "
                  "navigateur sera ensuite mémorisé.",
    .signInCode = "Code",
    .signInButton = "Se connecter",
    .signInWrong = "Code faux ou expiré. Rechargez la page pour en avoir un nouveau (après 5 "
                   "erreurs, attendez 30\u00A0s).",
    .signOutAll = "Déconnecter tous les autres navigateurs",

    .wifiTitle = "Wi-Fi du panneau",
    .wifiSubtitle = "Le panneau retient ce réseau, même après une coupure.",
    .wifiSaved = "Réseau enregistré",
    .wifiState = "État",
    .wifiStateSetup = "point d'accès de réglage",
    .wifiStateConnected = "connecté,",
    .wifiStateOffline = "non connecté",
    .wifiRefresh = "Rafraîchir la liste",
    .wifiPassword = "Mot de passe",
    .wifiPasswordHint = "Laissez vide pour un réseau ouvert.",
    .wifiFooter = "Le panneau redémarre et rejoint ce réseau. Les hooks Claude Code le "
                  "retrouvent tout seuls.",
    .wifiBand = "Le panneau ne voit que les réseaux 2,4\u00A0GHz. Si votre box donne un nom "
                "à part à la bande 5\u00A0GHz, choisissez l'autre.",
    .wifiLastTry = "Dernier essai",
    .wifiRefused = "mot de passe refusé",
    .wifiNotFound = "réseau introuvable",
    .wifiMsgRetry = "Le panneau redémarre et retente le réseau enregistré. Vous pouvez vous "
                    "déconnecter.",
    .wifiMsgBadSsid = "Le nom du réseau doit faire de 1 à 32 caractères.",
    .wifiMsgBadPassword = "Un mot de passe Wi-Fi fait de 8 à 63 caractères.",

    // --- Web: Shell (theatre pages) ---
    .webTabs = {"Scène", "Ordre de passage", "Images", "Panneau"},
    .webSkip = "Aller au contenu",
    .webNavName = "Pages",
    .webSetupBrand = "Réglage de Carousel",
    .webTitle = "%s \u00B7 Carousel",

    // --- Web: Wi-Fi (theatre pages) ---
    .wifiAnother = "Un autre réseau, par son nom",
    .wifiName = "Nom du réseau",
    .wifiMsgSaved = "Enregistré. Le panneau redémarre et rejoint %s. Vous pouvez vous déconnecter.",
    .wifiNoneYet = "Aucun réseau trouvé pour l'instant.",
    .wifiKnown = "Réseau connu",
    .wifiOthers = "Autres réseaux",
    .wifiNetworks = "Réseaux",
    .wifiAway = "%s n'est pas à portée en ce moment.",
    .wifiSignal = {"faible", "moyen", "bon", "excellent"},
    .wifiSignalLocked = "signal %s, sécurisé",
    .wifiSignalOpen = "signal %s, ouvert",
    .wifiContinue = "Continuer",

    // --- Web: Sign-in and locked (theatre pages) ---
    .signInLive = "code sur le panneau en ce moment",
    .signInCaption = "Les chiffres restent sur le panneau\u202F; cette page ne les montre jamais.",
    .signInFrame = "Le panneau montre CODE, un code à 6 chiffres (caché ici) et une barre pour le "
                   "temps restant",
    .signInReload = "Pas de code sur le panneau\u202F? Rechargez cette page\u202F: l'ouvrir affiche "
                    "un nouveau code pendant 2\u00A0minutes.",
    .lockedLive = "sur le panneau",
    .signInDigit = "Chiffre %d sur 6",

    // --- Web: Error pages (theatre pages) ---
    .errTitle = {"Page introuvable", "Demande refusée", "Trop long à envoyer", "Formulaire non reçu",
                 "Mémoire insuffisante"},
    .errText = {"Il n'y a pas de page à cette adresse\u202F: repartez de la Scène.",
                "Elle vient d'un autre site ou d'une autre adresse\u202F: ouvrez le panneau à sa "
                "propre adresse.",
                "C'est plus long que ce que le panneau accepte\u202F: raccourcissez et renvoyez.",
                "C'est arrivé incomplet ou dans un autre format\u202F: renvoyez depuis la page.",
                "Le panneau manque de mémoire\u202F: attendez quelques secondes et réessayez."},
    .errBack = "Retour à %s",

    // --- Web: Stage (theatre pages) ---
    .stgLive = "en scène",
    .stgPlace = "écran %u sur %u",
    .stgNowSr = ", en scène maintenant",
    .stgNext = "Ensuite\u202F: %s dans %lu\u00A0s",
    .stgEvery = {"un nouvel écran toutes les 10\u00A0s", "un nouvel écran toutes les 20\u00A0s",
                 "un nouvel écran toutes les 30\u00A0s", "un nouvel écran chaque minute",
                 "un nouvel écran toutes les 2\u00A0minutes",
                 "un nouvel écran toutes les 5\u00A0minutes"},
    .stgKnobOnly = "Change au bouton seulement",
    .stgNoOther = "Aucun autre écran à montrer dans cette orientation",
    .stgPreviewAlt = "Ce que le panneau montre en ce moment\u202F: %s",
    .stgStill = "Une image prise à l'ouverture de la page. Rechargez-la pour la revoir.",
    .stgAllScreens = "Les %u écrans et leur ordre",
    .stgSessions = "Sessions de code",
    .stgSessionsHint = "Les sessions Claude Code que le panneau entend en ce moment. Elles "
                       "passent sur l'écran CODE.",
    .stgStates = {"en attente", "réfléchit", "utilise un outil", "vous attend", "erreur", "terminé",
                  "échec"},
    .stgAgoSec = "il y a %lu\u00A0s",
    .stgAgoMin = "il y a %lu\u00A0min",
    .stgAgoHour = "il y a %lu\u00A0h",
    .stgSetUpHooks = "Installer les hooks",
    .stgBring = "Faire entrer un écran",
    .stgScreen = "Écran",
    .stgLights = "Luminosité",
    .stgDimmer = "Moins fort",
    .stgBrighter = "Plus fort",
    .stgSteps = "Luminosité %d sur %d",
    .stgNightDims = "Niveau %d sur %d. La nuit, de %u:00 à %u:00, le panneau baisse à %s.",
    .stgNightDark = "Niveau %d sur %d. La nuit, de %u:00 à %u:00, le panneau s'éteint.",
    .stgNightNone = "Niveau %d sur %d. Pas de mode nuit.",
    .webNightMode = "Mode nuit",
    .stgSay = "Dire quelque chose",
    .stgSend = "Envoyer",
    .stgSayHint = "L'écran TEXTE le fait défiler, et il entre en scène tout de suite.",
    .stgTextOff = "Enregistré. TEXTE est éteint dans l'ordre de passage, donc le panneau ne le "
                  "montre pas.",
    .stgTextPortrait = "Enregistré. TEXTE ne s'affiche qu'en paysage\u202F: tournez le panneau "
                       "pour le voir.",
    .stgTextFailed = "Pas enregistré\u202F: la mémoire du panneau a refusé le message. "
                     "Réessayez\u202F; si ça échoue deux fois, redémarrez le panneau.",
    .stgShowOff = "Pas affiché\u202F: %s est éteint dans l'ordre de passage. Allumez-le d'abord.",
    .stgShowOther = "Pas affiché\u202F: %s est dessiné pour l'autre orientation.",
    .stgShowUnknown = "Pas affiché\u202F: le panneau n'a pas d'écran de ce nom.",
    .stgSent = "Envoyé\u202F: le panneau l'affiche.",
    .stgFailed = "Ça n'est pas passé\u202F: rechargez la page et réessayez.",

    // --- Web: Images (theatre pages) ---
    .imgLead = "Vos propres GIF, montrés au tour d'IMAGE dans l'ordre de passage.",
    .imgFileLabel = "Fichier GIF",
    .imgShowsThis = "Le panneau montre celle-ci",
    .imgDeleteName = "Supprimer %s",
    .imgSelectLabel = "Image",
    .imgEmptyHint = "Tant que vous n'en ajoutez pas, l'écran IMAGE montre l'image ci-dessus. Ajoutez "
                    "un GIF\u202F: le panneau le montre tout de suite, puis au tour d'IMAGE.",

    // --- Web: Running order (theatre pages) ---
    .ordLead = "Ce que joue le panneau, dans cet ordre. Le bouton et Home Assistant suivent cette "
               "liste.",
    .ordCount = "%u sur %u allumés",
    .ordImageShows = "Montre %s. Ajoutez, supprimez ou choisissez l'image sur la page Images.",
    .ordImageAll = "Montre chaque image à tour de rôle. Ajoutez, supprimez ou choisissez l'image sur la "
                   "page Images.",
    .ordOpenImages = "Ouvrir Images",
    .ordSettingsOf = "Réglages de %s",
    .ordHandle = "Déplacer %s, position %s sur %s",
    .ordMoneySum = "%s\u00A0%s · %u\u00A0h–%u\u00A0h",
    .ordImagesAll = "%d images, à tour de rôle",

    // --- Web: Panel (theatre pages) ---
    .pnlLead = "L'objet lui-même\u202F: sa langue, sa région, ses nuits, son réseau et ce avec quoi il parle.",
    .pnlUpdates = "Mises à jour",
    .pnlWifi = "Wi-Fi",
    .pnlAdvanced = "Avancé",
    .pnlLanguageLabel = "Langue des pages et du panneau",
    .pnlSaveScope = "Enregistre la langue, la région, le mode nuit, les retours, les mises à jour et Home Assistant.",
    .pnlRegion = "Région",
    .pnlTemperature = "Température",
    .pnlCurrency = "Devise",
    .pnlClock = "Horloge",
    .pnlRegionHint = "La façon dont le panneau écrit les températures, l’argent et l’heure.",
    .pnlFeedback = "Retours",
    .pnlTransition = "Transition",
    .pnlBrightnessChange = "Changement de luminosité",
    .pnlKnobTurn = "Tour de molette",
    .pnlRequests = "Demandes",
    .pnlTransitions = {"Poussée", "Fondu", "Balayage", "Coupe"},
    .pnlTransitionHints = {"L’écran suivant pousse le précédent dehors.",
                           "Un écran se fond dans le suivant.",
                           "Une ligne fait entrer l’écran suivant.",
                           "L’écran suivant remplace simplement le précédent."},
    .pnlBrightnessFeedbacks = {"Points", "Jauge", "Nombre", "Aucun"},
    .pnlBrightnessHints = {"Un bandeau en bas\u202F: un soleil et six crans.",
                           "Une jauge sur le côté.",
                           "Un grand nombre.",
                           "Rien\u202F: la luminosité change, c’est tout."},
    .pnlKnobFeedbacks = {"Bandeau", "Voisins", "Points", "Aucun"},
    .pnlKnobHints = {"Un bandeau avec le nom de l’écran et sa place.",
                     "Les écrans d’avant et d’après.",
                     "Un point par écran, l’actuel allumé.",
                     "Rien\u202F: l’écran change, c’est tout."},
    .pnlRequestKinds = {"Messages", "Images", "Commandes", "Agent de code"},
    .pnlRequestShows = {"Afficher", "Discret", "Désactivé"},
    .pnlRequestShowHints = {"À sa façon\u202F: bandeau, plein écran ou pastille.",
                            "Une petite pastille dans un coin.",
                            "Seulement le changement lui-même."},
    .pnlBoot = "Écran de démarrage",
    .pnlBoots = {"Auto", "Simple", "Détaillé"},
    .pnlBootHint = "Simple affiche le nom Carousel et sa ligne de chargement. "
                   "Détaillé ajoute l’adresse IP sous la ligne. "
                   "Auto\u202F: détaillé sur une version de développement, simple sur une version publiée.",
    .pnlSignal = {"bon", "moyen", "faible"},
    .pnlMqttTitle = "Home Assistant (MQTT)\u202F: %s",
    .pnlHooksNone = "Hooks des agents de code\u202F: aucune session",
    .pnlHooksOne = "Hooks des agents de code\u202F: 1 session",
    .pnlHooksMany = "Hooks des agents de code\u202F: %s sessions",
    .pnlHooksHint = "Une session fermée sans son hook de fin reste listée 15\u00A0minutes.",
    .pnlHookInstall = "Installer sur un ordinateur",
    .pnlHookInstallHint = "Lancez-la dans un terminal sur chaque ordinateur de ce réseau. Elle "
                          "trouve Claude Code, Codex, Cursor et Gemini CLI et installe leurs hooks.",
    .pnlHookUninstall = "Pour les retirer\u202F:",
    .pnlCopy = "Copier",
    .pnlCopied = "Copié",
    .pnlCopyByHand = "Sélectionnez la commande et copiez-la",
    .pnlHookRefused = "%s appels de hooks refusés depuis le démarrage du panneau\u202F: sans "
                      "jeton, ou avec un ancien. Relancez la commande sur chaque ordinateur.",
    .pnlHookOpenOff = "Hooks sans jeton\u202F: refusés",
    .pnlHookOpenOn = "Hooks sans jeton\u202F: acceptés (pour le développement)",
    .pnlHookAccept = "Accepter les hooks sans jeton",
    .pnlHookRefuse = "Refuser les hooks sans jeton",
    .pnlHookOpenWarn = "Tout appareil de ce réseau peut alors changer ce qu’affiche CODE.",
    .pnlHookRenew = "Renouveler le jeton",
    .pnlHookRenewHint = "La commande change. Les ordinateurs installés avec l’ancienne "
                        "s’arrêtent jusqu’à ce que vous y lanciez la nouvelle.",
    .pnlHookLegacy = "Les noms français et /claude/ marchent toujours.",
    .pnlApiTitle = "Jeton de l'API locale et navigateurs",
    .pnlToken = "Jeton",
    .pnlMemoryTitle = "Mémoire et redémarrages",
    .pnlRestartHint = "Pour redémarrer le panneau, débranchez-le une seconde\u202F; il revient sur "
                      "l'écran de votre dernier tour de bouton.",
  },
};

static uint8_t current = LANG_EN;

void setLanguage(uint8_t lang) { current = lang < LANG_COUNT ? lang : (uint8_t)LANG_EN; }

const Strings &tr() { return STRINGS[current]; }

const char *resetFailedText(const Strings &t, bool knobLock) {
  return knobLock ? t.webResetFailedKnob : t.webResetFailed;
}

// The French letters the body face composes (type.cpp): kept by toMessageText
static bool messageLetter(uint32_t cp) {
  static const uint16_t KEPT[] = {
    0xC0, 0xC2, 0xC4, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCE, 0xCF, 0xD4, 0xD6, 0xD9, 0xDB, 0xDC,
    0xE0, 0xE2, 0xE4, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEE, 0xEF, 0xF4, 0xF6, 0xF9, 0xFB, 0xFC,
    0xFF, 0x152, 0x153, 0x178,
  };
  for (uint16_t k : KEPT) if (k == cp) return true;
  return false;
}

// One character of untrusted UTF-8 (upload names, MQTT payloads, the API): its
// code point and its length. Only continuation bytes are taken, so a lead byte
// without them is one unknown character (0) and never swallows the next one,
// and the NUL stops it, so a character cut short never reads past the string.
// Overlong forms, 4-byte ones (emoji and the like) and cut ones are unknown.
static int nextChar(const uint8_t *p, uint32_t *cp) {
  uint32_t c = *p;
  const int want = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
  int len = 1;
  while (len < want && (p[len] & 0xC0) == 0x80) len++;
  if      (len < want || want == 4) c = 0;
  else if (want == 3) c = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
  else if (want == 2) c = ((c & 0x1F) << 6) | (p[1] & 0x3F);
  if ((want == 2 && c < 0x80) || (want == 3 && c < 0x800)) c = 0;
  *cp = c;
  return len;
}

// Bytes a character takes folded: three dots for the ellipsis, one otherwise
static size_t foldedBytes(uint32_t cp) { return cp == 0x2026 ? 3 : 1; }

// UTF-8 -> what the panel font draws. Accented Latin letters lose their
// accent, anything else outside ASCII becomes '?'. With keepFrench, the
// letters the body face draws stay as they are (UTF-8, never cut) while the
// rest of the text still fits folded: every character is kept first, and the
// last accents that no longer fit lose their mark.
static void foldText(const char *in, char *out, size_t size, bool keepFrench) {
  // Latin-1 U+00C0..U+00FF, in order
  static const char FOLD[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPs"
                             "aaaaaaaceeeeiiiidnooooo/ouuuuypy";
  uint32_t cp;
  size_t rest = 0;                                    // bytes the text still to read takes folded
  if (keepFrench)
    for (const uint8_t *p = (const uint8_t *)in; *p; ) { p += nextChar(p, &cp); rest += foldedBytes(cp); }
  size_t n = 0;
  for (const uint8_t *p = (const uint8_t *)in; *p && n + 1 < size; ) {
    const int len = nextChar(p, &cp);
    if (keepFrench) rest -= foldedBytes(cp);
    if (keepFrench && len == 2 && messageLetter(cp)) {
      if (n + 2 + rest < size) {
        out[n++] = (char)p[0];
        out[n++] = (char)p[1];
        p += len;
        continue;
      }
      if (cp > 0xFF) cp = cp == 0x152 ? 'O' : cp == 0x153 ? 'o' : 'Y';   // no room: its letter alone
    }
    p += len;
    if (cp >= 0x20 && cp < 0x7F)        out[n++] = (char)cp;
    else if (cp >= 0xC0 && cp <= 0xFF)  out[n++] = FOLD[cp - 0xC0];
    // Phone keyboards type these on their own: ’ “ ” – — …
    else if (cp == 0x2018 || cp == 0x2019) out[n++] = '\'';
    else if (cp == 0x201C || cp == 0x201D || cp == 0xAB || cp == 0xBB) out[n++] = '"';
    else if (cp == 0x2013 || cp == 0x2014) out[n++] = '-';
    else if (cp == 0x2026) { for (int k = 0; k < 3 && n + 1 < size; k++) out[n++] = '.'; }
    else if (cp == 0xA0 || cp == 0x202F) out[n++] = ' ';     // non-breaking spaces (French)
    else if (cp == '\t' || cp == '\n' || cp == '\r') out[n++] = ' ';
    else                                out[n++] = '?';
  }
  out[n] = '\0';
}

void toPanelText(const char *in, char *out, size_t size) { foldText(in, out, size, false); }
void toMessageText(const char *in, char *out, size_t size) { foldText(in, out, size, true); }
