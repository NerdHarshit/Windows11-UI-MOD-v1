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

function loadWeather() {
    if (!navigator.geolocation) {
        document.getElementById("desc").textContent = "Location unavailable";
        return;
    }

    navigator.geolocation.getCurrentPosition(
        async pos => {
            const lat = pos.coords.latitude;
            const lon = pos.coords.longitude;

            const weather = await getWeather(lat, lon);
            const current = weather.current;

            document.getElementById("city").textContent = "Local Weather";
            document.getElementById("temp").textContent = current.temperature_2m + "°C";
            document.getElementById("humidity").textContent = current.relative_humidity_2m + "%";
            document.getElementById("wind").textContent = current.wind_speed_10m + " km/h";
            document.getElementById("desc").textContent =
                weatherDescription(current.weather_code);
        },
        () => {
            document.getElementById("desc").textContent = "Location denied";
        }
    );
}

loadWeather();
setInterval(loadWeather, 600000);
