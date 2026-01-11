const desktop = document.getElementById("desktop");

const widgets = {};

function loadWidget(name) {
  fetch(`../widgets/${name}/index.html`)
    .then(res => res.text())
    .then(html => {
      const container = document.createElement("div");
      container.className = "widget theme-frost";

      // Fix relative paths
      const base = document.createElement("base");
      base.href = `../widgets/${name}/`;
      container.appendChild(base);

      // Create temp container
      const temp = document.createElement("div");
      temp.innerHTML = html;

      // Move all non-script elements
      [...temp.children].forEach(el => {
        if (el.tagName !== "SCRIPT") {
          container.appendChild(el);
        }
      });

      // Append to desktop first
      desktop.appendChild(container);

      // Now execute scripts
      temp.querySelectorAll("script").forEach(oldScript => {
        const script = document.createElement("script");
        script.src = oldScript.src;
        script.type = oldScript.type || "text/javascript";
        container.appendChild(script);
      });

      widgets[name] = container;
    });
}


// load digital clock
loadWidget("digital-clock");

// Enable / disable
document.getElementById("enableAll").addEventListener("change", e => {
  Object.values(widgets).forEach(w => {
    w.style.display = e.target.checked ? "block" : "none";
  });
});

// Theme
document.querySelectorAll(".theme-dot").forEach(dot => {
  dot.addEventListener("click", () => {
    const theme = dot.dataset.theme;
    Object.values(widgets).forEach(w => {
      w.className = `widget ${theme}`;
    });
  });
});
