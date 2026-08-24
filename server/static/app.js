let throughputChart;
const chartData = {
    labels: [],
    throughput: [],
    latency: []
};

function initChart() {
    const ctx = document.getElementById('throughputChart').getContext('2d');
    throughputChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: chartData.labels,
            datasets: [
                {
                    label: 'Throughput (Gbps)',
                    data: chartData.throughput,
                    borderColor: '#00d2ff',
                    backgroundColor: 'rgba(0, 210, 255, 0.1)',
                    borderWidth: 2,
                    tension: 0.35,
                    fill: true,
                    yAxisID: 'y'
                },
                {
                    label: 'Latency (μs)',
                    data: chartData.latency,
                    borderColor: '#7928ca',
                    backgroundColor: 'transparent',
                    borderWidth: 2,
                    borderDash: [5, 5],
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
                    grid: { color: 'rgba(255, 255, 255, 0.05)' },
                    ticks: { color: '#64748b' }
                },
                y: {
                    type: 'linear',
                    position: 'left',
                    grid: { color: 'rgba(255, 255, 255, 0.05)' },
                    ticks: { color: '#00d2ff' },
                    title: { display: true, text: 'Throughput (Gbps)', color: '#00d2ff' },
                    min: 0,
                    max: 12
                },
                y1: {
                    type: 'linear',
                    position: 'right',
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#c084fc' },
                    title: { display: true, text: 'Latency (μs)', color: '#c084fc' },
                    min: 0,
                    max: 2.5
                }
            },
            plugins: {
                legend: {
                    labels: { color: '#f8fafc', font: { family: 'Inter' } }
                }
            }
        }
    });
}

async function fetchTelemetry() {
    try {
        const res = await fetch('/api/telemetry');
        const data = await res.json();
        updateUI(data);
    } catch (err) {
        console.error('Failed to fetch telemetry:', err);
    }
}

function updateUI(data) {
    // 1. Update KPI Cards
    document.getElementById('valThroughput').innerHTML = `${data.gbps.toFixed(2)} <span class="unit">Gbps</span>`;
    document.getElementById('valPPS').innerHTML = `${data.pps.toLocaleString()} <span class="unit">PPS</span>`;
    document.getElementById('valLatency').innerHTML = `${data.avg_latency_us.toFixed(2)} <span class="unit">μs</span>`;
    document.getElementById('valP99').textContent = `${data.p99_latency_us.toFixed(2)} μs`;

    const bbloatCard = document.getElementById('bufferbloatCard');
    const bbloatVal = document.getElementById('valBufferbloat');
    const bbloatSub = document.getElementById('valBufferbloatSub');

    if (data.bufferbloat) {
        bbloatVal.textContent = 'HIGH JITTER DETECTED';
        bbloatVal.style.color = '#ef4444';
        bbloatSub.textContent = 'CoDel Dropping Bulk Chunks';
        bbloatSub.className = 'kpi-trend alert';
    } else {
        bbloatVal.textContent = 'OPTIMAL (MITIGATED)';
        bbloatVal.style.color = '#10b981';
        bbloatSub.textContent = 'Active Queue Management (CoDel)';
        bbloatSub.className = 'kpi-trend positive';
    }

    // 2. Update Charts
    const now = new Date().toLocaleTimeString();
    chartData.labels.push(now);
    chartData.throughput.push(data.gbps);
    chartData.latency.push(data.avg_latency_us);

    if (chartData.labels.length > 15) {
        chartData.labels.shift();
        chartData.throughput.shift();
        chartData.latency.shift();
    }
    throughputChart.update();

    // 3. Update Queue States
    const qList = document.getElementById('queueList');
    qList.innerHTML = data.queues.map(q => {
        const percent = Math.round((q.occupancy / q.max) * 100);
        return `
            <div class="queue-item">
                <div class="queue-item-header">
                    <strong>${q.name}</strong>
                    <span>${q.occupancy} / ${q.max} pkts (${percent}%)</span>
                </div>
                <div class="queue-progress-bg">
                    <div class="queue-progress-bar" style="width: ${percent}%;"></div>
                </div>
                <div class="queue-item-meta">
                    <span>Delay: ${q.delay_ms.toFixed(2)} ms</span>
                    <span>Drops: ${q.dropped}</span>
                </div>
            </div>
        `;
    }).join('');

    // 4. Update Active Flows Table
    const tbody = document.getElementById('flowTableBody');
    tbody.innerHTML = data.flows.map(f => {
        let clsTag = 'class-web';
        if (f.class === 'VOICE_CONTROL') clsTag = 'class-voice';
        if (f.class === 'GAMING_LOW_LAT') clsTag = 'class-gaming';
        if (f.class === 'STREAMING_VIDEO') clsTag = 'class-video';
        if (f.class === 'BULK_BACKGROUND') clsTag = 'class-bulk';

        return `
            <tr>
                <td>${f.src}</td>
                <td>${f.dst}</td>
                <td>${f.proto}</td>
                <td><span class="tag-class ${clsTag}">${f.class}</span></td>
                <td>${(f.confidence * 100).toFixed(0)}%</td>
                <td>${f.pps}</td>
                <td><span style="color: #10b981;">FORWARD (LPM Fast-Path)</span></td>
            </tr>
        `;
    }).join('');
}

async function injectBurst() {
    try {
        const res = await fetch('/api/inject');
        const d = await res.json();
        console.log(d.status);
    } catch (e) {
        console.error(e);
    }
}

window.addEventListener('DOMContentLoaded', () => {
    initChart();
    fetchTelemetry();
    setInterval(fetchTelemetry, 1000);
});
