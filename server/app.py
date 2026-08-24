#!/usr/bin/env python3
"""
OpenPath-X Web Telemetry & Control Server
Provides real-time REST API and interactive WebSocket/polling telemetry streaming
for live packet monitoring, throughput visualization, and queue state inspection.
"""

import http.server
import socketserver
import json
import os
import time
import random
import threading
from urllib.parse import urlparse, parse_qs

PORT = 8080
STATIC_DIR = os.path.join(os.path.dirname(__file__), "static")

# Shared telemetry state
telemetry_state = {
    "total_rx": 1250000,
    "total_tx": 1248900,
    "total_drops": 1100,
    "pps": 845000.0,
    "gbps": 9.42,
    "avg_latency_us": 0.48,
    "p99_latency_us": 0.89,
    "bufferbloat": False,
    "wireless_link_rate_mbps": 100.0,
    "queues": [
        {"name": "Q0: Voice & Control", "occupancy": 2, "max": 256, "dropped": 0, "delay_ms": 0.12},
        {"name": "Q1: Low-Latency Gaming", "occupancy": 8, "max": 256, "dropped": 0, "delay_ms": 0.45},
        {"name": "Q2: Video Streaming", "occupancy": 24, "max": 256, "dropped": 12, "delay_ms": 2.10},
        {"name": "Q3: Best Effort Web", "occupancy": 45, "max": 256, "dropped": 48, "delay_ms": 4.80},
        {"name": "Q4: Bulk Background", "occupancy": 72, "max": 256, "dropped": 1040, "delay_ms": 11.20},
    ],
    "flows": [
        {"src": "192.168.1.45:5060", "dst": "10.0.0.1:5060", "proto": "UDP", "class": "VOICE_CONTROL", "confidence": 0.99, "pps": 50},
        {"src": "192.168.1.102:7777", "dst": "104.22.5.89:7777", "proto": "UDP", "class": "GAMING_LOW_LAT", "confidence": 0.94, "pps": 128},
        {"src": "192.168.1.88:443", "dst": "142.250.180.206:443", "proto": "TCP", "class": "STREAMING_VIDEO", "confidence": 0.91, "pps": 2400},
        {"src": "192.168.1.15:443", "dst": "151.101.1.140:443", "proto": "TCP", "class": "BEST_EFFORT", "confidence": 0.88, "pps": 450},
        {"src": "192.168.1.200:51413", "dst": "185.12.34.56:51413", "proto": "TCP", "class": "BULK_BACKGROUND", "confidence": 0.89, "pps": 8900},
    ]
}

def background_simulator():
    """Dynamically simulates changing traffic rates, jitter, and queue depths."""
    while True:
        time.sleep(1.0)
        telemetry_state["total_rx"] += int(random.uniform(700000, 950000))
        telemetry_state["total_tx"] += int(random.uniform(695000, 948000))
        telemetry_state["pps"] = round(random.uniform(780000, 980000), 2)
        telemetry_state["gbps"] = round((telemetry_state["pps"] * 850 * 8) / 1e9, 2)
        telemetry_state["avg_latency_us"] = round(random.uniform(0.38, 0.62), 2)
        telemetry_state["p99_latency_us"] = round(telemetry_state["avg_latency_us"] * 1.85, 2)
        
        # Bufferbloat check
        total_occ = sum(q["occupancy"] for q in telemetry_state["queues"])
        telemetry_state["bufferbloat"] = total_occ > 180

class TelemetryHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=STATIC_DIR, **kwargs)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/telemetry":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(telemetry_state).encode("utf-8"))
        elif parsed.path == "/api/inject":
            # Simulate high-load traffic spike
            for q in telemetry_state["queues"]:
                q["occupancy"] = min(q["max"], q["occupancy"] + random.randint(10, 40))
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"status": "Traffic burst injected"}).encode("utf-8"))
        else:
            super().do_GET()

def run_server():
    t = threading.Thread(target=background_simulator, daemon=True)
    t.start()
    
    with socketserver.TCPServer(("", PORT), TelemetryHandler) as httpd:
        print(f"=========================================================")
        print(f"  OpenPath-X Web Telemetry Dashboard running at:         ")
        print(f"  --> http://localhost:{PORT}                           ")
        print(f"=========================================================")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server...")

if __name__ == "__main__":
    run_server()
