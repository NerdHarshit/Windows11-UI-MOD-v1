const cpuEl = document.getElementById("cpu");
const ramEl = document.getElementById("ram");
const diskEl = document.getElementById("disk");

window.chrome.webview.addEventListener("message", event => {
    const data = event.data;

    cpuEl.textContent = data.cpu + "%";
    ramEl.textContent = data.ram + "%";
    diskEl.textContent = data.disk + "%";
});
