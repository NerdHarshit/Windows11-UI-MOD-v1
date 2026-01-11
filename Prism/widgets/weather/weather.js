async function getLocation() {
    const res = await fetch("https://ipapi.co/json/");
    const data = await res.json();
    return {
        city: data.city,
        lat: data.latitude,
        lon: data.longitude
    };
}

async function getWeather(lat, lon) {
    const url =
      `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code`;

    const res = await fetch(url);
    return await res.json();
}

function weatherDescription(code) {
    if (code === 0) return "Clear";
    if (code < 3) return "Cloudy";
    if (code < 60) return "Foggy";
    if (code < 70) return "Rain";
    if (code < 80) return "Snow";
    return "Stormy";
}

async function loadWeather() {
    try {
        const loc = await getLocation();
        const weather = await getWeather(loc.lat, loc.lon);

        const current = weather.current;

        document.getElementById("city").textContent = loc.city;
        document.getElementById("temp").textContent = current.temperature_2m + "°C";
        document.getElementById("humidity").textContent = current.relative_humidity_2m + "%";
        document.getElementById("wind").textContent = current.wind_speed_10m + " km/h";
        document.getElementById("desc").textContent = weatherDescription(current.weather_code);
    }
    catch {
        document.getElementById("desc").textContent = "Weather unavailable";
    }
}

// Load now and every 10 minutes
loadWeather();
setInterval(loadWeather, 600000);
