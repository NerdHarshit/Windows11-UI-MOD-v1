console.log("System widget JS loaded");

window.chrome.webview.addEventListener("message", event => {
    console.log("Received from C++:", event.data);

    document.getElementById("cpu").textContent = event.data.cpu + "%";
    document.getElementById("ram").textContent = event.data.ram + "%";
    document.getElementById("disk").textContent = event.data.disk + "%";
});

function setTheme(theme) {
  document.body.className = theme;
}
