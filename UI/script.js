// ================================
// PRISM STATE
// ================================




const clockDigital = document.getElementById("clockDigital");
const clockAnalog = document.getElementById("clockAnalog");

const ampmToggle = document.getElementById("ampmToggle");
const ampmSpan = document.getElementById("ampm");




enableAll.addEventListener("change", () => {
  const enabled = enableAll.checked;

  Object.values(widgets).forEach(widget => {
    widget.style.display = enabled ? "block" : "none";
  });
});



clockDigital.addEventListener("change", updateClock);
clockAnalog.addEventListener("change", updateClock);

// Initial state
updateClock();

themeDots.forEach(dot => {
  dot.addEventListener("click", () => {
    const theme = dot.dataset.theme;

    Object.values(widgets).forEach(widget => {
      widget.classList.remove(
        "theme-frost",
        "theme-night",
        "theme-ocean",
        "theme-desert",
        "theme-forest",
        "theme-sunset"
      );
      widget.classList.add(theme);
    });
  });
});

document.querySelector(".control-panel").style.userSelect = "none";

const digitalTime = document.getElementById("digitalTime");
const digitalDay = document.getElementById("day");
const digitalDate = document.getElementById("dd mm yyyy");



ampmToggle.addEventListener("change", updateDigitalClock);

const hourHand = document.querySelector(".hand.hour");
const minuteHand = document.querySelector(".hand.minute");
const secondHand = document.querySelector(".hand.second");

function updateAnalogClock() {
  const now = new Date();

  const seconds = now.getSeconds();
  const minutes = now.getMinutes();
  const hours = now.getHours();

  const secondsDeg = seconds * 6;               // 360 / 60
  const minutesDeg = minutes * 6 + seconds * 0.1;
  const hoursDeg = (hours % 12) * 30 + minutes * 0.5;

  secondHand.style.transform =
    `translateX(-50%) rotate(${secondsDeg}deg)`;

  minuteHand.style.transform =
    `translateX(-50%) rotate(${minutesDeg}deg)`;

  hourHand.style.transform =
    `translateX(-50%) rotate(${hoursDeg}deg)`;
}

updateAnalogClock();
setInterval(updateAnalogClock, 1000);

updateDigitalClock();               // run once immediately
setInterval(updateDigitalClock, 1000); // update every second
