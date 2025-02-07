let lpg_threshold = 60;
let lpg_ppm = 0;

let lpg_client;
let isConnected = false;

// Document references
// mqtt status
const mqtt_status = document.querySelector(".mqtt-status");

// Connect button document reference
const connect_button = document.querySelector(".connect-button");

//  Status message document reference
const status_msg = document.querySelector(".status-msg");

// Guage fill and cover document reference
const guage = document.querySelector(".guage-fill");
const guage_text = document.querySelector(".guage-cover");

// Number input document reference
const threshold_input = document.querySelector(".num-input");

// plus and minus symbol document reference
const plus = document.querySelector(".fa-plus");
const minus = document.querySelector(".fa-minus");

// threshold button document reference
const thr_btn = document.querySelector(".thr-btn");

// buzzer button document reference
const bzz_btn = document.querySelector(".bzr-btn");

// Function implementations
// Function to update the chart with new data
function addDataGraph(value) {
  if (value > 1000) value = 1000;
  else if (value < 0) value = 0;

  const timestamp = Date.now(); // Current timestamp

  // Add new data point (timestamp and value) to chart
  if (data.labels.length > 50) {
    // Keep data to the last 50 points
    data.labels.shift();
    data.datasets[0].data.shift();
  }

  data.labels.push(timestamp);
  data.datasets[0].data.push(value);

  // Update the chart
  lpgLevelChart.update();
}

// Threshold input increment and decrement functions
plus.addEventListener("click", () => {
  threshold_input.stepUp();
});

minus.addEventListener("click", () => {
  threshold_input.stepDown();
});

// Update guage function
function updateGuage(value) {
  if (value > 1000) value = 1000;
  else if (value < 0) value = 0;
  value = Math.round(value);

  guage_text.innerText = `${value} PPM`;

  value = (value / 1000) * lpg_threshold;
  value = (value / lpg_threshold) * 0.5;
  guage.style.transform = `rotate(${value}turn)`;
}

// MQTT initiliziation
// MQTT broker URL
const broker_url = "wss://broker.hivemq.com:8884/mqtt";

// MQTT topics
const web_connection_status = "lpg-detection-alerting/web-connection-status";
const buzzer_terminate_topic = "lpg-detection-alerting/terminate-buzzer";
const gas_threshold_topic = "lpg-detection-alerting/gas-threshold";
const gas_threshold_fb_topic = "lpg-detection-alerting/gas-threshold-fb";
const lpg_ppm_topic = "lpg-detection-alerting/lpg-ppm";
const lpg_alert_topic = "lpg-detection-alerting/lpg-alert";

function connect_state() {
  connect_button.style.backgroundColor = "#ff8400";
  connect_button.style.width = "100px";
  connect_button.innerHTML = "Connect";
  connect_button.style.cursor = "pointer";
}

function connecting_state() {
  connect_button.style.backgroundColor = "grey";
  connect_button.style.width = "120px";
  connect_button.innerHTML = "Connecting...";
  connect_button.style.cursor = "not-allowed";
}

function connected_state() {
  connect_button.style.backgroundColor = "green";
  connect_button.style.width = "100px";
  connect_button.innerHTML = "Connected";
  connect_button.style.cursor = "pointer";
}

function disconnect_state() {
  connect_button.style.backgroundColor = "red";
  connect_button.style.width = "100px";
  connect_button.innerHTML = "Disconnect";
  connect_button.style.cursor = "pointer";
}

function disconnecting_state() {
  connect_button.style.backgroundColor = "red";
  connect_button.style.width = "130px";
  connect_button.innerHTML = "Disconnecting...";
  connect_button.style.cursor = "not-allowed";
}

connect_button.addEventListener("click", () => {
  if (!isConnected) {
    connecting_state();
    lpg_client = mqtt.connect(broker_url);

    lpg_client.on("connect", () => {
      console.log("Connected to MQTT broker");
      isConnected = true;
      mqtt_status.style.backgroundColor = "green";
      connected_state();

      setTimeout(() => {
        disconnect_state();
      }, 1000);

      lpg_client.publish(web_connection_status, "1", (err) => {
        console.log(
          `Publishing to topic ${web_connection_status}, error: ${err}`
        );
      });

      lpg_client.subscribe(lpg_ppm_topic, (err) => {
        if (err) {
          console.log("Subscription error: ", err);
        } else {
          console.log("Subscribed to: ", lpg_ppm_topic);
        }
      });

      lpg_client.subscribe(gas_threshold_fb_topic, (err) => {
        if (err) {
          console.log("Subscription error: ", err);
        } else {
          console.log("Subscribed to: ", gas_threshold_fb_topic);
        }
      });

      lpg_client.subscribe(lpg_alert_topic, (err) => {
        if (err) {
          console.log("Subscription error: ", err);
        } else {
          console.log("Subscribed to: ", lpg_alert_topic);
        }
      });

      // Listen for incoming messages
      lpg_client.on("message", (received_topic, msg) => {
        if (received_topic == lpg_ppm_topic) {
          console.log(msg);
          lpg_ppm = Number(msg);

          updateGuage(lpg_ppm);
          addDataGraph(lpg_ppm);
        } else if (received_topic == lpg_alert_topic) {
          msg = msg.toString();
          console.log(msg);
          if (msg === "lpg alert") {
            alert("LPG Level exceeded!!!");
            status_msg.innerHTML = "WARNING!! LPG Level exceeded!";
            status_msg.style.color = "red";
          } else {
            status_msg.innerHTML = "LPG level normal";
            status_msg.style.color = "green";
          }
        } else if ((received_topic = gas_threshold_fb_topic)) {
          console.log(msg);
          lpg_threshold = Number(msg);
          threshold_input.value = lpg_threshold;
        }
      });
    });

    lpg_client.on("error", (err) => {
      console.error(`Connection Error: ${err}`);
      isConnected = false;
      mqtt_status.style.backgroundColor = "red";
      connect_state();
      lpg_client.publish(web_connection_status, "1", (err) => {
        console.log(
          `Publishing to topic ${web_connection_status}, error: ${err}`
        );
      });
    });
  } else {
    disconnecting_state();
    lpg_client.publish(web_connection_status, "1", (err) => {
      console.log(
        `Publishing to topic ${web_connection_status}, error: ${err}`
      );
    });
    setTimeout(() => {
      lpg_client.end(true, () => {
        console.log("Disconnected from MQTT broker");
        isConnected = false;
        mqtt_status.style.backgroundColor = "red";
        connect_state();
      });
    }, 1000);
  }
});

thr_btn.addEventListener("click", () => {
  lpg_threshold = threshold_input.value;
  lpg_client.publish(gas_threshold_topic, lpg_threshold, (err) => {
    console.log(`Publishing to topic ${gas_threshold_topic}, error: ${err}`);
  });
});

bzz_btn.addEventListener("mousedown", () => {
  // When the button is pressed down, publish "ON"
  lpg_client.publish(buzzer_terminate_topic, "ON", (err) => {
    if (err) {
      console.error(
        `Error publishing to topic ${buzzer_terminate_topic}: ${err.message}`
      );
    } else {
      console.log(
        `Published "ON" to topic ${buzzer_terminate_topic} successfully.`
      );
    }
  });
});

bzz_btn.addEventListener("mouseup", () => {
  // When the button is released, publish "OFF" after 2 seconds
  setTimeout(() => {
    lpg_client.publish(buzzer_terminate_topic, "OFF", (err) => {
      if (err) {
        console.error(
          `Error publishing to topic ${buzzer_terminate_topic}: ${err.message}`
        );
      } else {
        console.log(
          `Published "OFF" to topic ${buzzer_terminate_topic} successfully.`
        );
      }
    });
  }, 2000); // Delay of 2000 milliseconds (2 seconds)
});

// Chart document reference
// Get the context of the canvas element
const ctx = document.querySelector(".lpg-chart").getContext("2d");

// Create a linear gradient for the background color
const gradient = ctx.createLinearGradient(0, 0, 0, 400);
gradient.addColorStop(0, "#fa5502");
gradient.addColorStop(1, "#fae102");

// Initialize the chart with initial data (starting from 0)
const data = {
  labels: [], // Empty initial labels
  datasets: [
    {
      label: "LPG level",
      data: [], // Empty initial data
      borderColor: "#808080", // Grey line
      backgroundColor: gradient,
      fill: true,
      pointRadius: 5,
      pointHoverRadius: 10,
      pointHitRadius: 10,
      borderWidth: 2,
      pointBackgroundColor: "#808080", // Grey dot
      pointBorderColor: "#ffffff", // White border on the dot
      pointBorderWidth: 2,
    },
  ],
};

const config = {
  type: "line",
  data: data,
  options: {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: {
        position: "top",
      },
      tooltip: {
        mode: "index",
        intersect: false,
        callbacks: {
          // Modify the tooltip label to format the x value as HH:MM:SS
          title: function (tooltipItem) {
            // Get the timestamp from the data labels
            const timestamp = data.labels[tooltipItem[0].dataIndex];
            const date = new Date(timestamp);
            const hours = String(date.getHours()).padStart(2, "0");
            const minutes = String(date.getMinutes()).padStart(2, "0");
            const seconds = String(date.getSeconds()).padStart(2, "0");
            return `${hours}:${minutes}:${seconds}`; // Return formatted time
          },
          label: function (tooltipItem) {
            const value = tooltipItem.raw;
            return `LPG Level: ${value} PPM`; // Display the LPG level in PPM
          },
        },
      },
    },
    scales: {
      y: {
        beginAtZero: false,
        ticks: {
          stepSize: 50,
          max: 1000,
          min: 0,
          callback: function (value) {
            return value + " PPM";
          },
        },
      },
      x: {
        type: "linear",
        position: "bottom",
        ticks: {
          autoSkip: true,
          maxTicksLimit: 20,
          callback: function (value) {
            // Convert timestamp to HH:MM:SS format
            const date = new Date(value);
            const hours = String(date.getHours()).padStart(2, "0");
            const minutes = String(date.getMinutes()).padStart(2, "0");
            const seconds = String(date.getSeconds()).padStart(2, "0");
            return `${hours}:${minutes}:${seconds}`;
          },
        },
      },
    },
    animation: {
      duration: 2000, // Animation duration
      easing: "easeInOutQuad",
    },
    responsive: true,
  },
};

// Initialize the chart
const lpgLevelChart = new Chart(ctx, config);
