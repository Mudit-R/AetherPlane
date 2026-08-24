let telemetryChart;
let recentPackets = [];
let selectedPacketId = null;

const chartSeries = {
    labels: [],
    throughput: [],
    latency: []
};

// Client-side fallback state for GitHub Pages static hosting
const clientSimState = {
    gbps: 9.85,
    pps: 1250000,
    avg_latency_us: 0.42,
    p90_latency_us: 0.58,
    p99_latency_us: 0.74,
    bufferbloat: false,
    cores: [
        { core_id: 0, load_pct: 42.5, rx_pps: 312000, ring_depth: 14 },
        { core_id: 1, load_pct: 39.8, rx_pps: 305000, ring_depth: 11 },
        { core_id: 2, load_pct: 45.2, rx_pps: 328000, ring_depth: 18 },
        { core_id: 3, load_pct: 38.1, rx_pps: 305000, ring_depth: 9 }
    ],
    queues: [
        { id: 0, name: "Q0: Strict Voice & Control", occupancy: 2, max: 256, dropped: 0, delay_ms: 0.08, class: "VOICE_CONTROL" },
        { id: 1, name: "Q1: Interactive Gaming (UDP)", occupancy: 6, max: 256, dropped: 0, delay_ms: 0.25, class: "GAMING_LOW_LAT" },
        { id: 2, name: "Q2: Adaptive Video Stream", occupancy: 18, max: 256, dropped: 4, delay_ms: 1.40, class: "STREAMING_VIDEO" },
        { id: 3, name: "Q3: Best Effort Web (HTTP)", occupancy: 32, max: 256, dropped: 18, delay_ms: 2.80, class: "BEST_EFFORT" },
        { id: 4, name: "Q4: Bulk Transfer & Sync", occupancy: 64, max: 256, dropped: 1778, delay_ms: 7.50, class: "BULK_BACKGROUND" }
    ],
    flows: [
        { id: 101, src: "192.168.1.45:5060", dst: "10.0.0.1:5060", proto: "UDP", class: "VOICE_CONTROL", confidence: 0.99, pps: 50, bandwidth_mbps: 0.08, action: "FAST_FORWARD_Q0" },
        { id: 102, src: "192.168.1.102:7777", dst: "104.22.5.89:7777", proto: "UDP", class: "GAMING_LOW_LAT", confidence: 0.96, pps: 128, bandwidth_mbps: 0.25, action: "FAST_FORWARD_Q1" },
        { id: 103, src: "192.168.1.88:443", dst: "142.250.180.206:443", proto: "TCP", class: "STREAMING_VIDEO", confidence: 0.93, pps: 2400, bandwidth_mbps: 24.5, action: "DRR_SCHEDULE_Q2" },
        { id: 104, src: "192.168.1.15:443", dst: "151.101.1.140:443", proto: "TCP", class: "BEST_EFFORT", confidence: 0.90, pps: 650, bandwidth_mbps: 6.2, action: "DRR_SCHEDULE_Q3" },
        { id: 105, src: "192.168.1.200:51413", dst: "185.12.34.56:51413", proto: "TCP", class: "BULK_BACKGROUND", confidence: 0.94, pps: 9800, bandwidth_mbps: 88.0, action: "CODEL_SHAPE_Q4" }
    ]
};

const fallbackPackets = [
    {
        id: 1,
        timestamp: "10:14:22.408",
        protocol: "UDP",
        src: "192.168.1.45:5060",
        dst: "10.0.0.1:5060",
        length: 128,
        traffic_class: "VOICE_CONTROL",
        hex_dump: "00 1a 2b 3c 4d 5e 00 11 22 33 44 55 08 00 45 00\n00 80 12 34 40 00 40 11 00 00 c0 a8 01 64 0a 00\n00 01 13 c4 13 c4 00 00 00 00 49 4e 56 49 54 45\n20 73 69 70 3a 61 6c 69 63 65 40 31 30 2e 30 2e",
        payload_preview: "INVITE sip:alice@10.0.0.1 SIP/2.0"
    },
    {
        id: 2,
        timestamp: "10:14:22.412",
        protocol: "UDP",
        src: "192.168.1.102:7777",
        dst: "104.22.5.89:7777",
        length: 84,
        traffic_class: "GAMING_LOW_LAT",
        hex_dump: "00 1a 2b 3c 4d 5e 00 11 22 33 44 55 08 00 45 00\n00 54 12 34 40 00 40 11 00 00 c0 a8 01 64 0a 00\n00 01 1e 61 1e 61 00 00 00 00 01 04 fa 12 47 41\n4d 45 5f 54 49 43 4b 5f 58 3a 31 34 35 2e 32 2c",
        payload_preview: "GAME_TICK_X:145.2,Y:388.1,P:12ms"
    },
    {
        id: 3,
        timestamp: "10:14:22.419",
        protocol: "TCP",
        src: "192.168.1.88:52144",
        dst: "142.250.180.206:443",
        length: 1420,
        traffic_class: "STREAMING_VIDEO",
        hex_dump: "00 1a 2b 3c 4d 5e 00 11 22 33 44 55 08 00 45 00\n05 8c 12 34 40 00 40 06 00 00 c0 a8 01 64 0a 00\n00 01 cb b0 01 bb 00 00 00 00 17 03 03 05 80 54\n4c 53 5f 41 50 50 4c 49 43 41 54 49 4f 4e 5f 44",
        payload_preview: "TLS_APPLICATION_DATA_FRAME_CHUNK"
    },
    {
        id: 4,
        timestamp: "10:14:22.425",
        protocol: "TCP",
        src: "192.168.1.15:49211",
        dst: "151.101.1.140:443",
        length: 540,
        traffic_class: "BEST_EFFORT",
        hex_dump: "00 1a 2b 3c 4d 5e 00 11 22 33 44 55 08 00 45 00\n02 1c 12 34 40 00 40 06 00 00 c0 a8 01 64 0a 00\n00 01 c0 3b 01 bb 00 00 00 00 47 45 54 20 2f 61\n70 69 2f 76 31 2f 66 65 65 64 20 48 54 54 50 2f",
        payload_preview: "GET /api/v1/feed HTTP/1.1"
    },
    {
        id: 5,
        timestamp: "10:14:22.431",
        protocol: "TCP",
        src: "192.168.1.200:51413",
        dst: "185.12.34.56:51413",
        length: 1500,
        traffic_class: "BULK_BACKGROUND",
        hex_dump: "00 1a 2b 3c 4d 5e 00 11 22 33 44 55 08 00 45 00\n05 dc 12 34 40 00 40 06 00 00 c0 a8 01 64 0a 00\n00 01 c8 d5 c8 d5 00 00 00 00 00 00 00 09 07 42\n54 5f 42 49 54 54 4f 52 52 45 4e 54 5f 50 49 45",
        payload_preview: "BT_BITTORRENT_PIECE_INDEX:48922"
    }
];

function initTelemetryChart() {
    const ctx = document.getElementById('telemetryChart').getContext('2d');
    telemetryChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: chartSeries.labels,
            datasets: [
                {
                    label: 'Throughput (Gbps)',
                    data: chartSeries.throughput,
                    borderColor: '#0f172a',
                    backgroundColor: 'rgba(253, 224, 71, 0.4)',
                    borderWidth: 2.5,
                    tension: 0.1,
                    fill: true,
                    yAxisID: 'y'
                },
                {
                    label: 'Mean Latency (us)',
                    data: chartSeries.latency,
                    borderColor: '#f97316',
                    backgroundColor: 'transparent',
                    borderWidth: 2.5,
                    borderDash: [5, 5],
                    tension: 0.1,
                    yAxisID: 'y1'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 300 },
            scales: {
                x: {
                    grid: { color: '#e2e8f0', lineWidth: 1 },
                    ticks: { color: '#0f172a', font: { family: 'JetBrains Mono', weight: 'bold', size: 10 } }
                },
                y: {
                    type: 'linear',
                    position: 'left',
                    grid: { color: '#e2e8f0', lineWidth: 1 },
                    ticks: { color: '#0f172a', font: { family: 'JetBrains Mono', weight: 'bold' } },
                    title: { display: true, text: 'Throughput (Gbps)', color: '#0f172a', font: { family: 'JetBrains Mono', weight: 'bold' } },
                    min: 0,
                    max: 12
                },
                y1: {
                    type: 'linear',
                    position: 'right',
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#f97316', font: { family: 'JetBrains Mono', weight: 'bold' } },
                    title: { display: true, text: 'Latency (us)', color: '#f97316', font: { family: 'JetBrains Mono', weight: 'bold' } },
                    min: 0,
                    max: 2.0
                }
            },
            plugins: {
                legend: {
                    labels: { color: '#0f172a', font: { family: 'JetBrains Mono', weight: 'bold', size: 11 } }
                }
            }
        }
    });
}

async function fetchTelemetry() {
    try {
        const res = await fetch('/api/telemetry');
        if (!res.ok) throw new Error('API unavailable');
        const data = await res.json();
        renderTelemetry(data);
    } catch (e) {
        simulateClientTick();
        renderTelemetry(clientSimState);
    }
}

async function fetchPackets() {
    try {
        const res = await fetch('/api/packets');
        if (!res.ok) throw new Error('API unavailable');
        recentPackets = await res.json();
        renderPacketList();
    } catch (e) {
        recentPackets = fallbackPackets;
        renderPacketList();
    }
}

function simulateClientTick() {
    const pps = 1180000 + Math.random() * 200000;
    clientSimState.pps = Math.round(pps);
    clientSimState.gbps = Number(((pps * 850 * 8) / 1e9).toFixed(2));
    clientSimState.avg_latency_us = Number((0.39 + Math.random() * 0.09).toFixed(2));
    clientSimState.p90_latency_us = Number((clientSimState.avg_latency_us * 1.38).toFixed(2));
    clientSimState.p99_latency_us = Number((clientSimState.avg_latency_us * 1.76).toFixed(2));

    clientSimState.cores.forEach(c => {
        c.load_pct = Number((36 + Math.random() * 16).toFixed(1));
        c.rx_pps = Math.round(pps / 4 + (Math.random() * 20000 - 10000));
        c.ring_depth = Math.floor(Math.random() * 18 + 6);
    });
}

function renderTelemetry(data) {
    // 1. Executive KPIs
    document.getElementById('valThroughput').textContent = data.gbps.toFixed(2);
    document.getElementById('valPPS').textContent = data.pps.toLocaleString();
    document.getElementById('valLatency').textContent = data.avg_latency_us.toFixed(2);
    document.getElementById('valP90').textContent = `${data.p90_latency_us.toFixed(2)} us`;
    document.getElementById('valP99').textContent = `${data.p99_latency_us.toFixed(2)} us`;

    const aqmVal = document.getElementById('valAQM');
    const aqmSub = document.getElementById('valAQMSub');
    if (data.bufferbloat) {
        aqmVal.textContent = 'HIGH JITTER (ACTIVE AQM)';
        aqmVal.className = 'kpi-value text-alert';
        aqmSub.textContent = 'CoDel Dropping Bulk Tail Packets to Preserve Latency';
        aqmSub.className = 'kpi-footer text-alert';
    } else {
        aqmVal.textContent = 'OPTIMAL';
        aqmVal.className = 'kpi-value text-ok';
        aqmSub.textContent = 'FQ-CoDel Active Queue Management Active';
        aqmSub.className = 'kpi-footer text-ok';
    }

    // 2. Multi-Core RSS Worker Threads
    const coreGrid = document.getElementById('coreGrid');
    coreGrid.innerHTML = data.cores.map(c => `
        <div class="core-box">
            <div class="core-header">
                <span>Core #${c.core_id} (RSS Worker)</span>
                <span>${c.load_pct}% Load</span>
            </div>
            <div class="core-bar-bg">
                <div class="core-bar-fill" style="width: ${c.load_pct}%;"></div>
            </div>
            <div class="core-meta">
                <span>${(c.rx_pps / 1000).toFixed(0)}k PPS</span>
                <span>Ring Depth: ${c.ring_depth}/1024</span>
            </div>
        </div>
    `).join('');

    // 3. Telemetry Chart Updates
    const timestamp = new Date().toLocaleTimeString();
    chartSeries.labels.push(timestamp);
    chartSeries.throughput.push(data.gbps);
    chartSeries.latency.push(data.avg_latency_us);

    if (chartSeries.labels.length > 18) {
        chartSeries.labels.shift();
        chartSeries.throughput.shift();
        chartSeries.latency.shift();
    }
    telemetryChart.update();

    // 4. Dynamic Smart QoS Queues
    const qContainer = document.getElementById('queueContainer');
    qContainer.innerHTML = data.queues.map(q => {
        const pct = Math.round((q.occupancy / q.max) * 100);
        return `
            <div class="queue-card">
                <div class="queue-card-head">
                    <strong>${q.name}</strong>
                    <span>${q.occupancy} / ${q.max} (${pct}%)</span>
                </div>
                <div class="queue-bar-bg">
                    <div class="queue-bar-fill" style="width: ${pct}%;"></div>
                </div>
                <div class="queue-card-footer">
                    <span>Queue Delay: ${q.delay_ms.toFixed(2)} ms</span>
                    <span>AQM Drops: ${q.dropped}</span>
                </div>
            </div>
        `;
    }).join('');

    // 5. 5-Tuple Flows Table
    const flowTbody = document.getElementById('flowTbody');
    flowTbody.innerHTML = data.flows.map(f => {
        let tagClass = 'tag-web';
        if (f.class === 'VOICE_CONTROL') tagClass = 'tag-voice';
        if (f.class === 'GAMING_LOW_LAT') tagClass = 'tag-gaming';
        if (f.class === 'STREAMING_VIDEO') tagClass = 'tag-video';
        if (f.class === 'BULK_BACKGROUND') tagClass = 'tag-bulk';

        return `
            <tr>
                <td><strong>${f.src}</strong> &lt;-&gt; <strong>${f.dst}</strong></td>
                <td><span class="tag-badge">${f.proto}</span></td>
                <td><span class="tag-badge ${tagClass}">${f.class}</span></td>
                <td>${(f.confidence * 100).toFixed(0)}%</td>
                <td>${f.pps.toLocaleString()}</td>
                <td>${f.bandwidth_mbps} Mbps</td>
                <td><strong style="color: #0f172a;">${f.action}</strong></td>
            </tr>
        `;
    }).join('');
}

function renderPacketList() {
    const listEl = document.getElementById('packetList');
    listEl.innerHTML = recentPackets.map(pkt => `
        <div class="packet-item ${pkt.id === selectedPacketId ? 'active' : ''}" onclick="selectPacket(${pkt.id})">
            <div class="packet-item-top">
                <span>#${pkt.id} [${pkt.protocol}]</span>
                <span>${pkt.length} Bytes</span>
            </div>
            <div class="packet-item-bottom">
                ${pkt.src} -&gt; ${pkt.dst}
            </div>
        </div>
    `).join('');

    if (selectedPacketId === null && recentPackets.length > 0) {
        selectPacket(recentPackets[0].id);
    }
}

function selectPacket(id) {
    selectedPacketId = id;
    renderPacketList();

    const pkt = recentPackets.find(p => p.id === id);
    if (!pkt) return;

    const detailsEl = document.getElementById('packetDetailsCard');
    detailsEl.innerHTML = `
        <div class="dissection-header">Dissection Analysis: Packet #${pkt.id} [${pkt.protocol}]</div>
        <div class="dissection-fields">
            <div><strong>• Ingress Timestamp:</strong> ${pkt.timestamp}</div>
            <div><strong>• L2 Frame:</strong> Ethernet II (Type: 0x0800 IPv4) | Checksum: VALID</div>
            <div><strong>• L3 Network:</strong> IPv4 (TTL: 64, IHL: 20B) | Src: ${pkt.src.split(':')[0]} -&gt; Dst: ${pkt.dst.split(':')[0]}</div>
            <div><strong>• L4 Transport:</strong> ${pkt.protocol} (Ports: ${pkt.src.split(':')[1]} -&gt; ${pkt.dst.split(':')[1]})</div>
            <div><strong>• Flow Classification:</strong> <span class="tag-badge tag-gaming">${pkt.traffic_class}</span></div>
            <div><strong>• Payload Preview:</strong> <span>"${pkt.payload_preview}"</span></div>
        </div>
        <div style="font-size: 11px; font-weight: 800; color: #475569; margin-bottom: 6px;">RAW BINARY HEX DUMP (Zero-Copy Ring Buffer Slice):</div>
        <div class="hex-box">${pkt.hex_dump}</div>
    `;
}

async function injectTraffic(type) {
    try {
        const res = await fetch(`/api/inject/${type}`);
        if (res.ok) {
            const data = await res.json();
            alert(`Simulation Triggered: ${data.status}`);
            fetchTelemetry();
            return;
        }
    } catch (e) {}

    if (type === 'burst') {
        clientSimState.gbps = 10.0;
        clientSimState.pps = 1450000;
        clientSimState.queues.forEach(q => q.occupancy = Math.min(q.max, q.occupancy + 25));
        alert('Simulation: 10Gbps Line-Rate Burst Injected');
    } else if (type === 'ddos') {
        alert('Simulation: SYN Flood Mitigated by eBPF/XDP Fast-Path Hook (0% CPU impact)');
    } else if (type === 'bufferbloat') {
        clientSimState.queues[4].occupancy = 240;
        clientSimState.bufferbloat = true;
        alert('Simulation: Bottleneck Link Saturated - FQ-CoDel Coordinated Drops Active');
    }
    renderTelemetry(clientSimState);
}

window.addEventListener('DOMContentLoaded', () => {
    initTelemetryChart();
    fetchTelemetry();
    fetchPackets();
    setInterval(fetchTelemetry, 1000);
});
