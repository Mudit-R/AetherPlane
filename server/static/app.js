let telemetryChart;
let recentPackets = [];
let selectedPacketId = null;

const chartSeries = {
    labels: [],
    throughput: [],
    latency: []
};

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
                    borderColor: '#38bdf8',
                    backgroundColor: 'rgba(56, 189, 248, 0.12)',
                    borderWidth: 2.5,
                    tension: 0.35,
                    fill: true,
                    yAxisID: 'y'
                },
                {
                    label: 'Mean Latency (μs)',
                    data: chartSeries.latency,
                    borderColor: '#a855f7',
                    backgroundColor: 'transparent',
                    borderWidth: 2,
                    borderDash: [4, 4],
                    tension: 0.35,
                    yAxisID: 'y1'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 400 },
            scales: {
                x: {
                    grid: { color: 'rgba(255, 255, 255, 0.04)' },
                    ticks: { color: '#64748b', font: { family: 'JetBrains Mono', size: 10 } }
                },
                y: {
                    type: 'linear',
                    position: 'left',
                    grid: { color: 'rgba(255, 255, 255, 0.04)' },
                    ticks: { color: '#38bdf8', font: { family: 'JetBrains Mono' } },
                    title: { display: true, text: 'Throughput (Gbps)', color: '#38bdf8', font: { family: 'Inter', weight: 'bold' } },
                    min: 0,
                    max: 12
                },
                y1: {
                    type: 'linear',
                    position: 'right',
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#c084fc', font: { family: 'JetBrains Mono' } },
                    title: { display: true, text: 'Latency (μs)', color: '#c084fc', font: { family: 'Inter', weight: 'bold' } },
                    min: 0,
                    max: 2.0
                }
            },
            plugins: {
                legend: {
                    labels: { color: '#f8fafc', font: { family: 'Inter', size: 12 } }
                }
            }
        }
    });
}

async function fetchTelemetry() {
    try {
        const res = await fetch('/api/telemetry');
        const data = await res.json();
        renderTelemetry(data);
    } catch (e) {
        console.error('Failed to fetch telemetry:', e);
    }
}

async function fetchPackets() {
    try {
        const res = await fetch('/api/packets');
        recentPackets = await res.json();
        renderPacketList();
    } catch (e) {
        console.error('Failed to fetch packets:', e);
    }
}

function renderTelemetry(data) {
    // 1. Executive KPIs
    document.getElementById('valThroughput').textContent = data.gbps.toFixed(2);
    document.getElementById('valPPS').textContent = data.pps.toLocaleString();
    document.getElementById('valLatency').textContent = data.avg_latency_us.toFixed(2);
    document.getElementById('valP90').textContent = `${data.p90_latency_us.toFixed(2)} μs`;
    document.getElementById('valP99').textContent = `${data.p99_latency_us.toFixed(2)} μs`;

    const aqmVal = document.getElementById('valAQM');
    const aqmSub = document.getElementById('valAQMSub');
    if (data.bufferbloat) {
        aqmVal.textContent = 'HIGH JITTER (ACTIVE AQM)';
        aqmVal.className = 'kpi-value text-danger';
        aqmSub.textContent = 'CoDel Dropping Bulk Tail Packets to Save Latency';
        aqmSub.className = 'kpi-footer text-danger';
    } else {
        aqmVal.textContent = 'OPTIMAL (MITIGATED)';
        aqmVal.className = 'kpi-value text-success';
        aqmSub.textContent = 'FQ-CoDel Active Queue Management Active';
        aqmSub.className = 'kpi-footer text-success';
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
                <td><strong>${f.src}</strong> ↔ <strong>${f.dst}</strong></td>
                <td><span class="tag-badge badge-subtle">${f.proto}</span></td>
                <td><span class="tag-badge ${tagClass}">${f.class}</span></td>
                <td>${(f.confidence * 100).toFixed(0)}%</td>
                <td>${f.pps.toLocaleString()}</td>
                <td>${f.bandwidth_mbps} Mbps</td>
                <td><strong style="color: #38bdf8;">${f.action}</strong></td>
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
                ${pkt.src} → ${pkt.dst}
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
        <div class="dissection-header">🔍 Dissection Analysis: Packet #${pkt.id} [${pkt.protocol}]</div>
        <div class="dissection-fields">
            <div><strong>• Ingress Timestamp:</strong> ${pkt.timestamp}</div>
            <div><strong>• L2 Frame:</strong> Ethernet II (Type: 0x0800 IPv4) | Checksum: VALID</div>
            <div><strong>• L3 Network:</strong> IPv4 (TTL: 64, IHL: 20B) | Src: ${pkt.src.split(':')[0]} → Dst: ${pkt.dst.split(':')[0]}</div>
            <div><strong>• L4 Transport:</strong> ${pkt.protocol} (Ports: ${pkt.src.split(':')[1]} → ${pkt.dst.split(':')[1]})</div>
            <div><strong>• AI Classification:</strong> <span class="tag-badge tag-gaming">${pkt.traffic_class}</span></div>
            <div><strong>• Payload Preview:</strong> <span style="color: #34d399;">"${pkt.payload_preview}"</span></div>
        </div>
        <div style="font-size: 11px; font-weight: 700; color: #94a3b8; margin-bottom: 6px;">RAW BINARY HEX DUMP (Zero-Copy Ring Buffer Slice):</div>
        <div class="hex-box">${pkt.hex_dump}</div>
    `;
}

async function injectTraffic(type) {
    try {
        const res = await fetch(`/api/inject/${type}`);
        const data = await res.json();
        alert(`⚡ Simulation Triggered: ${data.status}`);
        fetchTelemetry();
    } catch (e) {
        console.error(e);
    }
}

window.addEventListener('DOMContentLoaded', () => {
    initTelemetryChart();
    fetchTelemetry();
    fetchPackets();
    setInterval(fetchTelemetry, 1000);
});
