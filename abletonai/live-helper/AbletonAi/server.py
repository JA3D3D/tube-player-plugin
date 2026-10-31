# AbletonAi Live helper: a small local server the AbletonAi plugin talks to.
#
#  - listens on 127.0.0.1 only, on the first free port from 47410
#  - writes {port, token} to Application Support/AbletonAi/helper.json (readable only by you);
#    every request must carry that token
#  - one JSON object per line in, one per line out
#  - requests are queued here and run on Live's main thread by process() (Live's API is not
#    thread-safe), which the control surface calls from update_display

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import socket
import threading
import time
import binascii

try:
    import queue
except ImportError:  # pragma: no cover
    import Queue as queue

FIRST_PORT = 47410
PORT_TRIES = 10
MAX_LINE = 4 * 1024 * 1024


def helper_dir():
    home = os.path.expanduser("~")
    mac = os.path.join(home, "Library", "Application Support")
    base = mac if os.path.isdir(mac) else home
    return os.path.join(base, "AbletonAi")


class Server(object):
    def __init__(self, commands, log=None, info_dir=None, first_port=FIRST_PORT):
        self.commands = commands
        self.log = log or (lambda m: None)
        self.info_dir = info_dir or helper_dir()
        self.first_port = first_port
        self.token = binascii.hexlify(os.urandom(16)).decode("ascii")
        self.port = 0
        self._queue = queue.Queue()
        self._sock = None
        self._running = False
        self._thread = None

    # ------------------------------------------------------------------ lifecycle
    def start(self):
        for port in range(self.first_port, self.first_port + PORT_TRIES):
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                s.bind(("127.0.0.1", port))
                s.listen(8)
            except socket.error:
                s.close()
                continue
            s.settimeout(0.5)
            self._sock, self.port = s, port
            break
        if not self._sock:
            self.log("AbletonAi: no free port between {0} and {1}".format(self.first_port, self.first_port + PORT_TRIES - 1))
            return False
        self._running = True
        self._write_info()
        self._thread = threading.Thread(target=self._accept_loop, name="AbletonAi helper")
        self._thread.daemon = True
        self._thread.start()
        self.log("AbletonAi helper listening on 127.0.0.1:{0}".format(self.port))
        return True

    def stop(self):
        self._running = False
        try:
            if self._sock:
                self._sock.close()
        except Exception:
            pass
        try:
            p = os.path.join(self.info_dir, "helper.json")
            with open(p) as f:
                if json.load(f).get("token") == self.token:
                    os.remove(p)
        except Exception:
            pass

    def _write_info(self):
        try:
            if not os.path.isdir(self.info_dir):
                os.makedirs(self.info_dir)
            p = os.path.join(self.info_dir, "helper.json")
            tmp = p + ".tmp"
            fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
            with os.fdopen(fd, "w") as f:
                json.dump({"port": self.port, "token": self.token, "pid": os.getpid(), "started": time.time()}, f)
            os.chmod(tmp, 0o600)
            os.rename(tmp, p)
        except Exception as e:
            self.log("AbletonAi: could not write helper.json: {0}".format(e))

    # ------------------------------------------------------------------ network (background threads)
    def _accept_loop(self):
        while self._running:
            try:
                conn, _ = self._sock.accept()
            except socket.timeout:
                continue
            except Exception:
                if self._running:
                    time.sleep(0.2)
                continue
            t = threading.Thread(target=self._client_loop, args=(conn,))
            t.daemon = True
            t.start()

    def _client_loop(self, conn):
        lock = threading.Lock()
        buf = b""
        conn.settimeout(1.0)
        try:
            while self._running:
                try:
                    chunk = conn.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                buf += chunk
                if len(buf) > MAX_LINE:
                    break
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    if line.strip():
                        self._accept_line(conn, lock, line)
        except Exception:
            pass
        finally:
            try:
                conn.close()
            except Exception:
                pass

    def _accept_line(self, conn, lock, line):
        try:
            req = json.loads(line.decode("utf-8"))
        except Exception:
            self._send(conn, lock, {"id": None, "ok": False, "error": "bad request"})
            return
        if not isinstance(req, dict) or req.get("token") != self.token:
            self._send(conn, lock, {"id": req.get("id") if isinstance(req, dict) else None, "ok": False, "error": "wrong token"})
            return
        self._queue.put((conn, lock, req))

    @staticmethod
    def _send(conn, lock, obj):
        data = (json.dumps(obj) + "\n").encode("utf-8")
        with lock:
            try:
                conn.sendall(data)
            except Exception:
                pass

    # ------------------------------------------------------------------ Live's main thread
    def process(self, budget_s=0.25):
        """Run queued requests; called from the control surface's update_display."""
        t0 = time.time()
        while time.time() - t0 < budget_s:
            try:
                conn, lock, req = self._queue.get_nowait()
            except queue.Empty:
                return
            rid = req.get("id")
            try:
                result = self.commands.run(req.get("cmd"), req.get("args") or {})
                resp = {"id": rid, "ok": True, "result": result}
            except Exception as e:
                resp = {"id": rid, "ok": False, "error": str(e) or e.__class__.__name__}
                self.log("AbletonAi: {0} failed: {1}".format(req.get("cmd"), e))
            self._send(conn, lock, resp)
