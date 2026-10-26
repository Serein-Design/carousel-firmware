# SPDX-License-Identifier: AGPL-3.0-only
"""End-to-end test of the coding agent bridge, against the real panel.

    python3 tools/agent-bridge/test/test_network.py              (carousel.local)
    python3 tools/agent-bridge/test/test_network.py 192.168.1.42
    CAROUSEL_CODE=123456 python3 tools/agent-bridge/test/test_network.py 192.168.1.42

Install the hooks on this computer first, with the command on the Panel page
(Advanced, coding agent hooks): the test runs the installed hook exactly as an
agent does, and reads the token from the followed panel's "header" file under
~/.config/carousel (XDG_CONFIG_HOME if set). It never prints the token.

The sessions are read on Stage and the command on Panel: both pages need a
signed-in browser, and no machine answer lists the sessions. So the test
signs in like a browser, with the 6-digit code the panel shows once the test
asks for it: typed at the prompt, or given in CAROUSEL_CODE (CAROUSEL_COOKIE="name=value"
reuses a browser session instead). It never signs
out, as that would sign out every other browser too.
It checks the English routes (/agent) and the French names (/claude), a call
without the token refused and the next request answered at once, event
ordering, bursts, the old http hooks with the header, the installer route and
the Panel page, and the browser guards: a cross-site call refused (403) before
its token is looked at, the hooks fold's buttons by POST only, signed in and
from the panel's own pages. That step turns "Accept hooks without a token" on
for a moment, and always back off, even when the test fails; its signed-out
POST puts a sign-in code on the panel. A few calls are refused on purpose: the
panel counts them, and its Panel page then says so. Test sessions are named "e2e-..." and closed at
the end: real sessions are left alone.
No dependency: standard library only, plus a POSIX sh and the installed hook.
"""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HOST = sys.argv[1] if len(sys.argv) > 1 else "carousel.local"
BASE = f"http://{HOST}"
CONFIG = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") / "carousel"
HOOK = CONFIG / "hook.sh"
SH = shutil.which("sh") or "/bin/sh"
failures = 0
cookie = ""
token = ""          # the hook token, read from the installed header file
panel_name = ""     # the panel's unique name, from /id

# Stage shows each session's state as a label in the panel's language: back
# to the state name the hook sends (firmware/src/i18n.cpp stgStates).
STATES = ("idle", "thinking", "tool", "attention", "error", "done", "failed")
LABELS = {label: word for labels in (
    ("waiting", "thinking", "using a tool", "needs you", "error", "finished", "failed"),
    ("en attente", "réfléchit", "utilise un outil", "t'attend", "erreur", "terminé", "échec"),
) for label, word in zip(labels, STATES)}


def check(cond, text):
    global failures
    print(f"  {'ok    ' if cond else 'FAILED'}  {text}")
    if not cond:
        failures += 1


def request(path, body=None, timeout=5, form=None, headers=None, hook=True, send=None):
    """Returns (HTTP code, text, duration in ms). Does not follow redirects.
    Sends the sign-in cookie once there is one, and the hook token (hook=False
    leaves it out, a string sends that value instead); send adds request
    headers; headers, if given, gets the answer's."""
    if form is not None:
        data, kind = urllib.parse.urlencode(form).encode(), "application/x-www-form-urlencoded"
    else:
        data, kind = (None if body is None else json.dumps(body).encode()), "application/json"
    req = urllib.request.Request(BASE + path, data=data, method="POST" if data else "GET")
    if data:
        req.add_header("Content-Type", kind)
    if cookie:
        req.add_header("Cookie", cookie)
    if hook is not False and (hook is not True or token):
        req.add_header("X-Hook-Token", token if hook is True else hook)
    for name, value in (send or {}).items():
        req.add_header(name, value)

    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *args, **kwargs):
            return None

    t0 = time.perf_counter()
    try:
        with urllib.request.build_opener(NoRedirect).open(req, timeout=timeout) as r:
            text, code, got = r.read().decode("utf-8", "replace"), r.status, r.headers
    except urllib.error.HTTPError as e:
        text, code, got = e.read().decode("utf-8", "replace"), e.code, e.headers
    except Exception as e:                       # connection dropped, timeout...
        text, code, got = f"{type(e).__name__}: {e}", 0, {}
    if headers is not None:
        headers.update(got or {})
    return code, text, (time.perf_counter() - t0) * 1000


def payload(session, project, event, output="", extra=None):
    """A hook payload, as Claude Code sends it (compact JSON); extra goes first."""
    return {
        **(extra or {}),
        "session_id": session,
        "prompt_id": "550e8400-e29b-41d4-a716-446655440000",
        "transcript_path": "/Users/test/.claude/projects/x/" + session + ".jsonl",
        "cwd": "/Users/test/" + project,
        "permission_mode": "default",
        "hook_event_name": event,
        "tool_name": "Read",
        "tool_input": {"file_path": "/x.txt"},
        "tool_response": output,
    }


def shown_sessions():
    """{name: state name} from Stage's "Coding sessions" block."""
    _, page, _ = request("/")
    rows = re.findall(r"<div class=st><span class=sn><i [^>]*></i>([^<]*)</span><b>([^<]*?) <span", page)
    return {html.unescape(n): LABELS.get(html.unescape(s), s) for n, s in rows}


def sign_in():
    """Signs in like a browser; the panel shows the code once a page is asked for signed out."""
    global cookie
    # A browser's session cookie (name=value) skips the code, for unattended runs
    if os.environ.get("CAROUSEL_COOKIE"):
        cookie = os.environ["CAROUSEL_COOKIE"]
        return True
    code, page, _ = request("/")
    check(code == 200 and 'name=code' in page, f"signed out, GET / asks for the code (got {code})")
    typed = os.environ.get("CAROUSEL_CODE", "")
    if not typed and sys.stdin.isatty():
        typed = input("  code   shown on the panel: ").strip()
    if not typed:               # an empty try still counts against the panel's attempts
        check(False, "a sign-in code, typed or in CAROUSEL_CODE")
        return False
    got = {}
    code, _, _ = request("/login", form={"code": typed, "next": "/"}, headers=got)
    cookie = (got.get("Set-Cookie") or "").split(";")[0]
    check(code == 303 and cookie, f"signed in with the panel's code (got {code})")
    return bool(cookie)


def read_token():
    """The hook token the installed hook sends to this panel, from its header file."""
    global token, panel_name
    code, ident, _ = request("/id", hook=False)
    found = re.search(r'"name":"(carousel-[a-z0-9-]+)"', ident)
    if code != 200 or not found:
        return False
    panel_name = found.group(1)
    header = CONFIG / "panels" / panel_name / "header"
    try:
        match = re.search(r"^X-Hook-Token: *([0-9a-f]{32})", header.read_text(), re.M)
    except OSError:
        return False
    token = match.group(1) if match else ""
    return bool(token)


def run_hook(agent, state, body, env=None):
    """Runs the installed hook exactly like an agent: sh, the agent and the state
    as arguments, the payload on stdin. CAROUSEL_HOST pins it to this panel."""
    t0 = time.perf_counter()
    subprocess.run([SH, str(HOOK), agent, state], input=json.dumps(body, separators=(",", ":")).encode(),
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15,
                   env={**os.environ, "CAROUSEL_HOST": panel_name, **(env or {})})
    return (time.perf_counter() - t0) * 1000


def await_state(name, state, timeout=8):
    """Sending happens in the background: give the panel time to receive it."""
    end = time.time() + timeout
    while time.time() < end:
        if shown_sessions().get(name) == state:
            return True
        time.sleep(0.5)
    return False


print(f"Panel {BASE}")

print("- Installed hook and token")
if not HOOK.exists() or not read_token():
    print(f"\n  install the hooks first, with the command on the Panel page (Advanced):\n"
          f"  {HOOK} and a token for this panel are needed")
    sys.exit(1)
check(True, f"hook installed, token read for {panel_name}")

print("- Sign-in")
if not sign_in():
    print("\n  every check below reads Stage: give the code the panel shows (CAROUSEL_CODE)")
    sys.exit(1)

print("- Web pages")
code, page, _ = request("/")
check(code == 200, f"GET / (Stage) answers 200 (got {code})")
check(page.rstrip().endswith("</html>"), f"page complete, {len(page)} bytes, not truncated")
check(">Coding sessions<" in page or ">Sessions de code<" in page, "it has the Coding sessions block")
code, page, _ = request("/panel")
check(code == 200 and re.search(r"/agent/(&lt;|<)state(&gt;|>)", page) is not None,
      f"Panel, Advanced shows the /agent/<state> hook address (got {code})")
check(f"/install?k={token}" in page and "curl -sS --compressed --noproxy '*' \"http://" in html.unescape(page),
      "the Panel page shows the install command, --noproxy and the current token (the one the hook uses)")
code, ident, _ = request("/id", hook=False)
check(code == 200 and '"device":"carousel"' in ident, f"/id identifies the panel ({ident[:80]})")

print("- Installer route")
code, text, _ = request("/install", hook=False, send={"Accept-Encoding": "gzip"})
check(code == 403 and "not valid any more" in text and token not in text,
      f"no link token: 403 and a script that says why, without the token (got {code})")
code, text, _ = request("/install?k=" + "0" * 32, hook=False, send={"Accept-Encoding": "gzip"})
check(code == 403 and "not valid any more" in text, f"wrong link token: 403 (got {code})")
got = {}
code, _, _ = request(f"/install?k={token}", hook=False, send={"Accept-Encoding": "gzip"}, headers=got)
check(code == 200 and got.get("Content-Encoding") == "gzip" and got.get("Cache-Control") == "no-store",
      f"right token: 200, gzip, no-store (got {code})")
check(got.get("X-Content-Type-Options") == "nosniff" and got.get("Content-Type") == "text/plain; charset=utf-8",
      f"as text/plain; charset=utf-8, nosniff (got {got.get('Content-Type')!r}, {got.get('X-Content-Type-Options')!r})")
code, _, _ = request(f"/install?k={token}", hook=False)
check(code == 406, f"right token without gzip accepted: 406, never raw bytes for sh (got {code})")

print("- Clean refusals")
code, text, _ = request("/agent/thinking")
check(code == 400, f"no identity: 400 (got {code} {text[:50]!r})")
code, text, _ = request("/agent/nonsense?s=e2e")
check(code == 400, f"unknown state: 400 (got {code} {text[:50]!r})")

print("- No token, a wrong token: refused, and the next request is answered at once")
REFUSED = "e2e-refused"
code, text, _ = request(f"/agent/thinking?s={REFUSED}&n={REFUSED}", hook=False)
check(code == 401, f"no token: 401 (got {code} {text[:50]!r})")
code, text, _ = request(f"/agent/thinking?s={REFUSED}&n={REFUSED}", hook="0" * 32)
check(code == 401, f"wrong token: 401 (got {code} {text[:50]!r})")
code, _, _ = request("/claude/thinking", payload(REFUSED, REFUSED, "UserPromptSubmit"), hook=False)
check(code == 401, f"POST with a body and no token: 401 (got {code})")
code, _, ms = request("/id", hook=False)
check(code == 200 and ms < 1000, f"the request right after answers in {ms:.0f} ms (got {code})")
time.sleep(0.4)
check(REFUSED not in shown_sessions(), "a refused call changed nothing on Stage")

print("- Browser guards")
EVIL = {"Origin": "http://evil.example"}
code, _, _ = request("/agent/attention?s=e2e-x&n=e2e-x", hook=False, send=EVIL)
check(code == 403, f"another site's Origin, no token: 403, before the token's 401 (got {code})")
code, _, _ = request("/agent/attention?s=e2e-x&n=e2e-x", hook=False, send={"Sec-Fetch-Site": "cross-site"})
check(code == 403, f"Sec-Fetch-Site: cross-site, no token: 403 (got {code})")
OPEN = "e2e-open"


def tokenless():
    """The code a hook call without a token gets: 401 while such calls are refused."""
    return request(f"/agent/idle?s={OPEN}&n={OPEN}", hook=False)[0]


try:
    code, _, _ = request("/hooks?do=accept", hook=False)
    check(tokenless() == 401, f"GET /hooks?do=accept (got {code}) changes nothing: a call without a token still 401")
    signed_in, cookie = cookie, ""
    code, page, _ = request("/hooks", form={"do": "accept"}, hook=False)
    cookie = signed_in
    check(code == 200 and "name=code" in page and tokenless() == 401,
          f"POST /hooks without the cookie: the sign-in page, nothing changed (got {code})")
    code, _, _ = request("/hooks", form={"do": "accept"}, hook=False, send=EVIL)
    check(code == 403 and tokenless() == 401, f"POST /hooks from another site: 403, nothing changed (got {code})")
    code, _, _ = request("/hooks", form={"do": "x"}, hook=False)
    check(code == 400 and tokenless() == 401, f"an unknown action: 400 (got {code})")
    got = {}
    code, _, _ = request("/hooks", form={"do": "accept"}, hook=False, headers=got)
    check(code == 303 and got.get("Location") == "/panel#hooks" and tokenless() == 200,
          f"accept: 303 to /panel#hooks, a call without a token then 200 (got {code})")
    code, _, _ = request("/hooks", form={"do": "refuse"}, hook=False)
    check(code == 303 and tokenless() == 401, f"refuse: 303, a call without a token 401 again (got {code})")
finally:
    request("/hooks", form={"do": "refuse"}, hook=False)
    request(f"/agent/end?s={OPEN}")

print("- Hook script (the agent's real path)")
SCRIPT = "e2e-script"
ms = run_hook("claude", "thinking", payload(SCRIPT + "-0001", SCRIPT, "UserPromptSubmit", output="x" * 150_000))
check(ms < 400, f"the agent gets control back in {ms:.0f} ms, 150 KB payload included")
check(await_state(SCRIPT, "thinking"), "the e2e-script session shows up as thinking")
for state in ("tool", "thinking", "tool"):
    run_hook("claude", state, payload(SCRIPT + "-0001", SCRIPT, "PreToolUse"))
check(await_state(SCRIPT, "tool"), "three hooks in a row: the last one wins (tool)")

print("- Hook script: other agents")
CURSOR = "e2e-cursor"
cursor = {"conversation_id": CURSOR + "-0001", "workspace_roots": ["/Users/test/" + CURSOR]}
run_hook("cursor", "thinking", {**cursor, "hook_event_name": "beforeSubmitPrompt"})
check(await_state(CURSOR, "thinking"), "a Cursor prompt shows its session as thinking")
run_hook("cursor", "stop", {**cursor, "hook_event_name": "stop", "status": "completed"})
check(await_state(CURSOR, "done"), "a Cursor stop with status completed shows done")
TWICE = "e2e-twice"
run_hook("claude", "thinking", payload(TWICE + "-0001", TWICE, "UserPromptSubmit",
                                       extra={"conversation_id": "c-0001", "cursor_version": "1.0"}))
time.sleep(3)
check(TWICE not in shown_sessions(), "a Claude Code hook that Cursor runs is ignored (Cursor reports it itself)")

# A name of its own per run: the panel keeps a session's last timestamp for a
# while, so a rerun within minutes would see its older events refused
ORDER = f"e2e-o{int(time.time()) % 10000}"   # short: the page cuts long names
print("- Event order (timestamped background sends)")
code, text, _ = request(f"/agent/tool?s={ORDER}&n={ORDER}&t=2000000")
check(code == 200 and text == "", f"200 with an empty body (got {code} {text[:30]!r})")
request(f"/agent/thinking?s={ORDER}&n={ORDER}&t=1000000")        # older
time.sleep(0.4)
check(shown_sessions().get(ORDER) == "tool", "the older event is ignored")
request(f"/agent/thinking?s={ORDER}&n={ORDER}&t=3000000")
time.sleep(0.4)
check(shown_sessions().get(ORDER) == "thinking", "the newest one goes through")

print("- Burst of 6 hooks in parallel (tools run together)")
with ThreadPoolExecutor(6) as ex:
    results = list(ex.map(
        lambda k: request(f"/agent/{'tool' if k % 2 else 'thinking'}?s={ORDER}&n={ORDER}&t={4000000 + k}"),
        range(6)))
check(all(c == 200 for c, _, _ in results), f"codes: {[c for c, _, _ in results]}")
slowest = max(ms for _, _, ms in results)
check(slowest < 2000, f"the slowest answered in {slowest:.0f} ms (< 2000)")
time.sleep(0.4)
check(shown_sessions().get(ORDER) == "tool", "final state = the newest (tool)")

print("- Stale arguments: a ?s= test must not capture a following http hook")
request("/agent/idle?s=e2e-trap&n=e2e-trap")
request("/agent/thinking", payload("e2e-session-2", "e2e-two", "UserPromptSubmit"))
time.sleep(0.4)
s = shown_sessions()
check(s.get("e2e-trap") == "idle", "e2e-trap stays idle")
check(s.get("e2e-two") == "thinking", "e2e-two received its own hook")

print("- Compatibility: /claude routes and French names")
code, text, _ = request("/claude/outil?s=e2e-session-2&n=e2e-two&t=9000000000000000")
check(code == 200 and text == "", f"GET /claude/outil: 200 with an empty body (got {code} {text[:30]!r})")
time.sleep(0.4)
check(shown_sessions().get("e2e-two") == "tool", "outil acts as tool")
code, _, _ = request("/agent/echec?s=e2e-session-2&n=e2e-two&t=9000000000000001")
check(code == 200, f"a French name on /agent: 200 (got {code})")
time.sleep(0.4)
check(shown_sessions().get("e2e-two") == "failed", "echec acts as failed")

print("- Compatibility: http hooks with the header")
# The agents' http hooks post the whole payload; the header comes from their
# configuration ("headers"), here from the request.
code, text, ms = request("/agent/tool", payload("e2e-session-2", "e2e-two", "PostToolUse", "y" * 20_000))
check(code == 200 and text == "", f"20 KB POST: 200 with an empty body in {ms:.0f} ms (got {code})")
# The old /claude?e= routes are gone (CHANGELOG: their POST loaded the whole
# body in memory); /agent/<state> and /claude/<state> replaced them.
code, _, _ = request("/claude?e=done", payload("e2e-session-2", "e2e-two", "Stop"))
check(code == 404, f"retired /claude?e=done format: 404 (got {code})")
code, _, _ = request("/claude/fini", payload("e2e-session-2", "e2e-two", "Stop"))
check(code == 200, f"POST /claude/fini: 200 (got {code})")
time.sleep(0.4)
check(shown_sessions().get("e2e-two") == "done", "e2e-two goes to done")

print("- SessionEnd")
code, _, _ = request("/agent/end", payload("e2e-session-2", "e2e-two", "SessionEnd"))
check(code == 200, f"POST /agent/end: 200 (got {code})")
time.sleep(0.4)
check("e2e-two" not in shown_sessions(), "e2e-two left the list")
run_hook("claude", "end", payload(SCRIPT + "-0001", SCRIPT, "SessionEnd"))
time.sleep(1.5)
check(SCRIPT not in shown_sessions(), "the hook's end leaves the list too")

print("- Cleaning up test sessions")
for sid in [ORDER, "e2e-trap", CURSOR + "-0001", SCRIPT + "-0001"]:
    request(f"/agent/end?s={sid}")
time.sleep(0.4)
code, _, _ = request("/")
left = [n for n in shown_sessions() if n.startswith("e2e-")]
check(code == 200 and not left, f"no e2e- session left (left: {left})")

print(f"\n{failures} failure(s)")
sys.exit(1 if failures else 0)
