// analog-clock/clock.js

/*

// Function to create hour marks dynamically
function createHourMarks() {
  for (let i = 0; i < 12; i++) {
    const mark = document.createElement('div');
    mark.classList.add('mark');
    mark.style.transform = `rotate(${i * 30}deg)`; // 360/12 = 30 deg
    clock.appendChild(mark);
  }
}*/
//const clock = document.getElementById('analogClock');
const hourHand = document.getElementById('hour-hand');
const minuteHand = document.getElementById('minute-hand');
const secondHand = document.getElementById('seconds-hand');

var hourAngle =0;
var minuteAngle =0;
var secondAngle=0;
// Function to update clock hands
function updateClock() {
  const now = new Date();
  const hours = now.getHours();
  const minutes = now.getMinutes();
  const seconds = now.getSeconds();

  hourAngle = (hours % 12) * 30 + minutes * 0.5; // 30 deg per hour + 0.5 per minute
  minuteAngle = minutes * 6 + seconds*0.1; // 360/60
  secondAngle = seconds * 6;

  hourHand.style.transform = `rotate(${hourAngle}deg)`;
  minuteHand.style.transform = `rotate(${minuteAngle}deg)`;
  secondHand.style.transform = `rotate(${secondAngle}deg)`;
}
function setTheme(theme) {
  document.body.className = theme;
}

// Initialize
//createHourMarks();
updateClock();
setInterval(updateClock, 1000); // Update every second
