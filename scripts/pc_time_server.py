#!/usr/bin/env python3
# PC time server for board one-click sync (LAN only, no internet needed).
# Run on Windows (same PC as Mosquitto):
#   python pc_time_server.py
# Board app button "一键同步电脑时间" will GET http://<PC_IP>:8765/time
#
# Firewall: allow inbound TCP 8765 (Private network).

from http.server import BaseHTTPRequestHandler, HTTPServer
from datetime import datetime
import json
import time

PORT = 8765


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.split("?")[0] not in ("/time", "/"):
            self.send_response(404)
            self.end_headers()
            return
        now = datetime.now()
        payload = {
            "time": now.strftime("%Y-%m-%d %H:%M:%S"),
            "epoch": int(time.time()),
        }
        data = json.dumps(payload).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)
        print("served time", payload["time"], "to", self.client_address[0])

    def log_message(self, fmt, *args):
        return


if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", PORT), Handler)
    print("PC time server on 0.0.0.0:%d" % PORT)
    print("Board should use: http://<this-PC-LAN-IP>:%d/time" % PORT)
    server.serve_forever()
