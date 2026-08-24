#!/usr/bin/env python3
"""
AetherPlane Web Telemetry, Deep Packet Inspection & Control Server
FAANG-Grade Interactive Network Telemetry Platform with Live Hex Inspector,
Multi-Core RSS Thread Distribution, and FQ-CoDel Bufferbloat Mitigation Controls.
"""

import http.server
import socketserver
import json
import os
import time
import random
import threading
import binascii
from urllib.parse import urlparse, parse_qs

PORT = 8080
STATIC_DIR = os.path.join(os.path.dirname(__file__), "static")

# Shared telemetry & simulation state
state = {
    "total_rx": 4820000,
    "total_tx": 4818200,
    "total_drops": 1800,
    "pps": 1250000.0,
    "gbps": 9.85,
    "avg_latency_us": 0.42,
    "p90_latency_us": 0.58,
    "p99_latency_us": 0.74,
    "bufferbloat": False,
    "aqm_mode": "FQ-CoDel (Active)",
    "wireless_link_rate_mbps": 100.0,
    "cores": [
        {"core_id": 0, "load_pct": 42.5, "rx_pps": 312000, "ring_depth": 14},
        {"core_id": 1, "load_pct": 39.8, "rx_pps": 305000, "ring_depth": 11},
        {"core_id": 2, "load_pct": 45.2, "rx_pps": 328000, "ring_depth": 18},
        {"core_id": 3, "load_pct": 38.1, "rx_pps": 305000, "ring_depth": 9},
    ],
    "queues": [
        {"id": 0, "name": "Q0: Strict Voice & Control", "occupancy": 2, "max": 256, "dropped": 0, "delay_ms": 0.08, "class": "VOICE_CONTROL"},
        {"id": 1, "name": "Q1: Interactive Gaming (UDP)", "occupancy": 6, "max": 256, "dropped": 0, "delay_ms": 0.25, "class": "GAMING_LOW_LAT"},
        {"id": 2, "name": "Q2: Adaptive Video Stream", "occupancy": 18, "max": 256, "dropped": 4, "delay_ms": 1.40, "class": "STREAMING_VIDEO"},
        {"id": 3, "name": "Q3: Best Effort Web (HTTP)", "occupancy": 32, "max": 256, "dropped": 18, "delay_ms": 2.80, "class": "BEST_EFFORT"},
        {"id": 4, "name": "Q4: Bulk Transfer & Sync", "occupancy": 64, "max": 256, "dropped": 1778, "delay_ms": 7.50, "class": "BULK_BACKGROUND"},
    ],
    "flows": [
        {"id": 101, "src": "192.168.1.45:5060", "dst": "10.0.0.1:5060", "proto": "UDP", "class": "VOICE_CONTROL", "confidence": 0.99, "pps": 50, "bandwidth_mbps": 0.08, "action": "FAST_FORWARD_Q0"},
        {"id": 102, "src": "192.168.1.102:7777", "dst": "104.22.5.89:7777", "proto": "UDP", "class": "GAMING_LOW_LAT", "confidence": 0.96, "pps": 128, "bandwidth_mbps": 0.25, "action": "FAST_FORWARD_Q1"},
        {"id": 103, "src": "192.168.1.88:443", "dst": "142.250.180.206:443", "proto": "TCP", "class": "STREAMING_VIDEO", "confidence": 0.93, "pps": 2400, "bandwidth_mbps": 24.5, "action": "DRR_SCHEDULE_Q2"},
        {"id": 104, "src": "192.168.1.15:443", "dst": "151.101.1.140:443", "proto": "TCP", "class": "BEST_EFFORT", "confidence": 0.90, "pps": 650, "bandwidth_mbps": 6.2, "action": "DRR_SCHEDULE_Q3"},
        {"id": 105, "src": "192.168.1.200:51413", "dst": "185.12.34.56:51413", "proto": "TCP", "class": "BULK_BACKGROUND", "confidence": 0.94, "pps": 9800, "bandwidth_mbps": 88.0, "action": "CODEL_SHAPE_Q4"},
    ],
    "recent_packets": []
}

def generate_sample_packets():
    """Generates synthetic dissected packets with authentic binary hex dumps."""
    sample_types = [
        {"proto": "UDP", "src": "192.168.1.45", "dst": "10.0.0.1", "sport": 5060, "dport": 5060, "len": 128, "cls": "VOICE_CONTROL", "payload": "INVITE sip:alice@10.0.0.1 SIP/2.0\r\nVia: SIP/2.0/UDP"},
        {"proto": "UDP", "src": "192.168.1.102", "dst": "104.22.5.89", "sport": 7777, "dport": 7777, "len": 84, "cls": "GAMING_LOW_LAT", "payload": "\x01\x04\xFA\x12GAME_TICK_X:145.2,Y:388.1,P:12ms"},
        {"proto": "TCP", "src": "192.168.1.88", "dst": "142.250.180.206", "sport": 52144, "dport": 443, "len": 1420, "cls": "STREAMING_VIDEO", "payload": "\x17\x03\x03\x05\x80TLS_APPLICATION_DATA_FRAME_CHUNK"},
        {"proto": "TCP", "src": "192.168.1.15", "dst": "151.101.1.140", "sport": 49211, "dport": 443, "len": 540, "cls": "BEST_EFFORT", "payload": "GET /api/v1/feed HTTP/1.1\r\nHost: api.service.io\r\n"},
        {"proto": "TCP", "src": "192.168.1.200", "dst": "185.12.34.56", "sport": 51413, "dport": 51413, "len": 1500, "cls": "BULK_BACKGROUND", "payload": "\x00\x00\x00\x09\x07BT_BITTORRENT_PIECE_INDEX:48922"},
    ]
    
    pkts = []
    for i, s in enumerate(sample_types):
        raw_hex = f"001a2b3c4d5e00112233445508004500{s['len']:04x}1234400040{17 if s['proto']=='UDP' else 6:02x}0000"
        raw_hex += "c0a801640a000001" + f"{s['sport']:04x}{s['dport']:04x}00000000"
        raw_hex += binascii.hexlify(s["payload"].encode("latin1", errors="ignore")).decode("ascii")
        
        # Format hex dump in 16-byte blocks
        chunks = [raw_hex[j:j+32] for j in range(0, min(len(raw_hex), 128), 32)]
        formatted_hex = "\n".join(" ".join(c[k:k+2] for k in range(0, len(c), 2)) for c in chunks)
        
        pkts.append({
            "id": i + 1,
            "timestamp": time.strftime("%H:%M:%S") + f".{random.randint(100, 999)}",
            "protocol": s["proto"],
            "src": f"{s['src']}:{s['sport']}",
            "dst": f"{s['dst']}:{s['dport']}",
            "length": s["len"],
            "traffic_class": s["cls"],
            "hex_dump": formatted_hex,
            "payload_preview": s["payload"][:40]
        })
    return pkts

state["recent_packets"] = generate_sample_packets()

def telemetry_daemon():
    """Background simulator driving real-time PPS, jitter, and core loads."""
    while True:
        time.sleep(1.0)
        pps_delta = random.uniform(1150000, 1380000)
        state["total_rx"] += int(pps_delta)
        state["total_tx"] += int(pps_delta * 0.998)
        state["pps"] = round(pps_delta, 2)
        state["gbps"] = round((state["pps"] * 850 * 8) / 1e9, 2)
        state["avg_latency_us"] = round(random.uniform(0.38, 0.49), 2)
        state["p90_latency_us"] = round(state["avg_latency_us"] * 1.38, 2)
        state["p99_latency_us"] = round(state["avg_latency_us"] * 1.76, 2)

        # Distribute across multi-core RSS workers
        for c in state["cores"]:
            c["load_pct"] = round(random.uniform(36.0, 52.0), 1)
            c["rx_pps"] = int(pps_delta / len(state["cores"]) + random.uniform(-15000, 15000))
            c["ring_depth"] = random.randint(6, 24)

        # Bufferbloat status calculation
        total_occ = sum(q["occupancy"] for q in state["queues"])
        state["bufferbloat"] = total_occ > 190

class AetherPlaneHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=STATIC_DIR, **kwargs)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/telemetry":
            self.send_json_response(state)
        elif parsed.path == "/api/flows":
            self.send_json_response(state["flows"])
        elif parsed.path == "/api/packets":
            self.send_json_response(state["recent_packets"])
        elif parsed.path == "/api/inject/burst":
            # Simulate 10G burst
            state["gbps"] = 10.0
            state["pps"] = 1450000.0
            for q in state["queues"]:
                q["occupancy"] = min(q["max"], q["occupancy"] + random.randint(15, 35))
            self.send_json_response({"status": "10Gbps Line-Rate Burst Injected", "gbps": state["gbps"]})
        elif parsed.path == "/api/inject/ddos":
            # Simulate eBPF/XDP DDoS drop
            state["total_drops"] += 50000
            self.send_json_response({"status": "SYN Flood Mitigated by eBPF/XDP Fast-Path Hook (0% CPU impact)", "drops": 50000})
        elif parsed.path == "/api/inject/bufferbloat":
            state["queues"][4]["occupancy"] = 240 # Saturate bulk queue
            state["bufferbloat"] = True
            self.send_json_response({"status": "Simulated Wireless Link Bottleneck Saturated", "bufferbloat": True})
        else:
            super().do_GET()

    def send_json_response(self, data):
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode("utf-8"))

def run_server():
    t = threading.Thread(target=telemetry_daemon, daemon=True)
    t.start()
    with socketserver.TCPServer(("", PORT), AetherPlaneHandler) as httpd:
        print(f"=========================================================")
        print(f"  AetherPlane Interactive Web Dashboard is Live!         ")
        print(f"  --> URL: http://localhost:{PORT}                       ")
        print(f"=========================================================")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server...")

if __name__ == "__main__":
    run_server()
