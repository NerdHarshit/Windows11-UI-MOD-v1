const timeEl = document.getElementById("time");
const dateEl = document.getElementById("date");
const ampmEl = document.getElementById("ampm");

function tick() {
  const now = new Date();
  let h = now.getHours();
  const m = now.getMinutes();
  const s = now.getSeconds();

  const ampm = h >= 12 ? "PM" : "AM";
  h = h % 12 || 12;

  timeEl.textContent = `${h}:${String(m).padStart(2,"0")}:${String(s).padStart(2,"0")}`;
  ampmEl.textContent = ampm;
  dateEl.textContent = now.toDateString();
}

setInterval(tick, 1000);
tick();
