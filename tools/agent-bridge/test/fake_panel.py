# SPDX-License-Identifier: AGPL-3.0-only
"""Fake panel: answers like the real one on /id, /agent/<state> and
/claude/<state>, and logs every hook request received to a file, one line each:

    time state session t name token-ok path-has-32-hex

token-ok is 1 when the X-Hook-Token header holds the token given here (0 else,
and the request is answered 401); path-has-32-hex is 1 when the address holds
32 hexadecimal characters in a row, which a token in the query would.

/id?c=<nonce> adds "proof", as the panel does, made with the token given
here over the nonce and the address the request reached; with "noproof" last,
it never does, like a host that only copies the panel's /id; with
"as=<address>" last, the proof is made for that address instead, like a relay
that passes the nonce on to the real panel there; with "busy" last, every
other /id leaves the proof out, the first one included, as the panel does past
4 proofs a second; with "refuse" last, it proves the token but answers every
hook call 401, as a panel whose token was renewed in between.

    python3 fake_panel.py <port> <log file> [token] [name] [noproof | as=<address> | busy | refuse]
"""
import hashlib
import hmac
import re
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

port, log_path = int(sys.argv[1]), sys.argv[2]
token = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
name = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "carousel-695c"
last = sys.argv[5] if len(sys.argv) > 5 else ""
proof = token is not None and last != "noproof"
relayed = last[3:] if last.startswith("as=") else None
asked = 0
HEX32 = re.compile(r"[0-9a-fA-F]{32}")
NONCE = re.compile(r"[0-9a-fA-F]{16,32}")


class Panel(BaseHTTPRequestHandler):
    def answer(self, code, body=b""):
        self.send_response(code)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urlsplit(self.path)
        query = parse_qs(url.query)
        global asked
        if url.path == "/id":
            body = '{"device":"carousel","name":"%s","mac":"24:6F:28:A1:69:5C"' % name
            c = query.get("c", [""])[0]
            asked += 1 if c else 0
            if proof and NONCE.fullmatch(c) and not (last == "busy" and asked % 2):
                ip = relayed or self.connection.getsockname()[0]
                mac = hmac.new(token.encode(), f"{c}:{ip}".encode(), hashlib.sha256).hexdigest()
                body += ',"proof":"%s"' % mac[:32]
            self.answer(200, (body + "}").encode())
            return
        route, _, state = url.path.rpartition("/")
        if route not in ("/agent", "/claude"):
            self.answer(404)
            return
        if not query.get("s"):
            self.answer(400)
            return
        token_ok = last != "refuse" and (token is None or self.headers.get("X-Hook-Token") == token)
        with open(log_path, "a", encoding="utf-8") as f:
            f.write(f"{time.time():.3f} {state} {query['s'][0]} "
                    f"{query.get('t', ['0'])[0]} {query.get('n', [''])[0]} "
                    f"{int(token_ok)} {int(bool(HEX32.search(self.path)))}\n")
        self.answer(200 if token_ok else 401)

    def log_message(self, *args):
        pass


ThreadingHTTPServer(("127.0.0.1", port), Panel).serve_forever()
