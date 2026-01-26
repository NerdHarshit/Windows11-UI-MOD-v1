function setWidget(name, enabled) {
  window.chrome.webview.postMessage(
    "set:" + name + ":" + (enabled ? "1" : "0")
  );
}
//older was togglw widget
// All widgets master toggle
document.getElementById("enableAll").addEventListener("change", e => {
  const v = e.target.checked;
  setWidget("system", v);
  setWidget("weather", v);
  setWidget("digital", v);
  setWidget("analog", v);

  document.getElementById("systemWidget").checked = v;
  document.getElementById("weatherWidget").checked = v;
  document.getElementById("clockDigital").checked = v;
  document.getElementById("clockAnalog").checked = v;
});

// Individual
document.getElementById("systemWidget").addEventListener("change", e => {
  setWidget("system", e.target.checked);
});

document.getElementById("weatherWidget").addEventListener("change", e => {
  setWidget("weather", e.target.checked);
});

document.getElementById("clockDigital").addEventListener("change", e => {
  setWidget("digital", e.target.checked);
});

document.getElementById("clockAnalog").addEventListener("change", e => {
  setWidget("analog", e.target.checked);
});

function setCheckbox(name, value) {
  if (name === "system") document.getElementById("systemWidget").checked = value;
  if (name === "weather") document.getElementById("weatherWidget").checked = value;
  if (name === "digital") document.getElementById("clockDigital").checked = value;
  if (name === "analog") document.getElementById("clockAnalog").checked = value;
}


function setEnableAll(value) {
  document.getElementById("enableAll").checked = value;
}



document.querySelectorAll(".theme-dot").forEach(dot => {
  dot.addEventListener("click", () => {
    const theme = dot.dataset.theme;
    window.chrome.webview.postMessage("theme:" + theme);
  });
});
