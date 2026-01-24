// 🔹 NEW: helper to talk to C++
function toggleWidget(name) {
  // Sends message to C++:
  // Example: "toggle:system"
  window.chrome.webview.postMessage("toggle:" + name);
}

/* =========================
   Enable / Disable all
   ========================= */
document.getElementById("enableAll").addEventListener("change", e => {
  const enabled = e.target.checked;

  // 🔹 Ask C++ to toggle all widgets
  toggleWidget("system");
  toggleWidget("weather");
  toggleWidget("digital");
  toggleWidget("analog");
});

/* =========================
   Individual widget toggles
   ========================= */
document.getElementById("systemWidget").addEventListener("change", () => {
  toggleWidget("system");   // matches name in C++
});

document.getElementById("weatherWidget").addEventListener("change", () => {
  toggleWidget("weather");
});

document.getElementById("clockDigital").addEventListener("change", () => {
  toggleWidget("digital");
});

document.getElementById("clockAnalog").addEventListener("change", () => {
  toggleWidget("analog");
});

/* =========================
   Theme logic (UI-only for now)
   ========================= */
document.querySelectorAll(".theme-dot").forEach(dot => {
  dot.addEventListener("click", () => {
    const theme = dot.dataset.theme;

    // 🔹 Only changes control panel theme for now
    document.getElementById("controlPanel").className =
      "control-panel " + theme;

    // 🔹 Later: we will send theme to widgets via C++
    // window.chrome.webview.postMessage("theme:" + theme);
  });
});
