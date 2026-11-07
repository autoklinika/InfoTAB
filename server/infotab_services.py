#!/usr/bin/env python3
"""InfoTAB service status gateway. Read-only, fixed allowlist; no secrets or controls.

Runs as harrypotter (systemd --user), not root. Does not restart/alter any service.
GET /services -> newline-separated NAME=running|stopped|unknown (text/plain).
"""
from __future__ import annotations

import argparse
import ipaddress
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BIND_HOST = "192.168.1.100"
PORT = 18092
ALLOWED_LAN = ipaddress.ip_network("192.168.1.0/24")
# Stable machine-readable identifiers; keep firmware InfoTABServices.h in sync.
SERVICE_SPECS = (
    ("bridge", "system", "ai-bridge.service"),
    ("gateway", "system", "ai-gateway.service"),
    ("ollama", "system", "ollama.service"),
    ("ollama_rtx", "system", "ollama-rtx-small.service"),
    ("postgres", "system", "postgresql@18-main.service"),
    ("qdrant", "docker", "ai-qdrant"),
    ("comfyui", "system", "comfyui.service"),
    ("radio", "system", "ai-platform-radio.service"),
    ("ups_guardian", "system", "ai-ups-protect.service"),
    ("ups_rm", "system", "ai-ups-rm-coordinator.service"),
    ("hermes", "user", "hermes-gateway.service"),
    ("magazyn_web", "user", "magazyn-webgui.service"),
    ("magazyn_bot", "user", "magazyn-telegram.service"),
)


def interpret_systemctl(value: str) -> str:
    v = value.strip()
    if v == "active":
        return "running"
    if v in ("inactive", "failed", "deactivating", "activating"):
        return "stopped"
    return "unknown"


def probe(kind: str, name: str) -> str:
    if kind == "docker":
        args = ["/usr/bin/docker", "inspect", "--format", "{{.State.Running}}", name]
    else:
        args = ["/usr/bin/systemctl"]
        if kind == "user":
            args += ["--user"]
        args += ["is-active", name]
    try:
        cp = subprocess.run(args, capture_output=True, text=True, timeout=1.6, check=False)
        value = cp.stdout.strip().lower()
        if kind == "docker":
            return "running" if value == "true" else ("stopped" if value == "false" else "unknown")
        return interpret_systemctl(value)
    except (OSError, subprocess.TimeoutExpired):
        return "unknown"


def snapshot() -> bytes:
    return ("".join(f"{key}={probe(kind, name)}\n"
                    for key, kind, name in SERVICE_SPECS)).encode("ascii")


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        try:
            source_ip = ipaddress.ip_address(self.client_address[0])
        except ValueError:
            source_ip = None
        if source_ip is None or source_ip not in ALLOWED_LAN:
            self.send_error(403)
            return
        if self.path != "/services":
            self.send_error(404)
            return
        body = snapshot()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=ascii")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt: str, *args: object) -> None:
        # No request logging or user secrets exposed in the journal.
        return


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--snapshot", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        assert interpret_systemctl("active\n") == "running"
        assert interpret_systemctl("failed\n") == "stopped"
        assert interpret_systemctl("inactive\n") == "stopped"
        assert interpret_systemctl("") == "unknown"
        assert len(SERVICE_SPECS) == len({s[0] for s in SERVICE_SPECS})
        assert all(k.isascii() and k.replace("_", "").isalnum() for k, _, _ in SERVICE_SPECS)
        print("INFOTAB_SERVICES_API_SELFTEST_PASS")
        return
    if args.snapshot:
        print(snapshot().decode("ascii"), end="")
        return
    server = ThreadingHTTPServer((BIND_HOST, PORT), Handler)
    server.daemon_threads = True
    server.serve_forever(poll_interval=0.5)


if __name__ == "__main__":
    main()
