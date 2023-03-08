
#ifndef _WEBPAGE_H_
#define _WEBPAGE_H_

#include <Arduino.h>

// Whole webpage here btw.
// Uses google charts for plotting, so internet connection required for your device.
// Should still work without one idk, haven't tested
// It is recommended that you switch IDE language mode to HTML, so you get proper language highlighting.

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2{
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  .content {
    padding: 30px;
    max-width: 600px;
    margin: 0 auto;
  }
  .card {
    background-color: #F8F7F9;;
    box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    padding-top:10px;
    padding-bottom:20px;
    margin-bottom:15px;
  }
  .slidecontainer {
    width: 100%; /* Width of the outside container */
  }
  .slider {
    -webkit-appearance: none;  /* Override default CSS styles */
    appearance: none;
    width: 100%; /* Full-width */
    display: flex;
    flex-direction: column;
    height: 20px;
    border-radius: 5px;   
    background: #d3d3d3; /* Grey background */
    outline: none; /* Remove outline */
    opacity: 0.7; /* Set transparency (for mouse-over effects on hover) */
    -webkit-transition: .2s; /* 0.2 seconds transition on hover */
    transition: opacity .2s;
  }
  /* The slider handle (use -webkit- (Chrome, Opera, Safari, Edge) and -moz- (Firefox) to override default look) */
  .slider::-webkit-slider-thumb {
    -webkit-appearance: none; /* Override default look */
    appearance: none;
    width: 25px; /* Set a specific slider handle width */
    height: 25px; /* Slider handle height */
    background: #0f8b8d; /* Green background */
    cursor: pointer; /* Cursor on hover */
  }
  .slider::-moz-range-thumb {
    width: 30px;
    height: 30px;
    border-radius: 10px;
    background: #0f8b8d;
    cursor: pointer;
  }
  .button {
    padding: 15px 50px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #0f8b8d;
    border: none;
    border-radius: 5px;
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
    -webkit-tap-highlight-color: rgba(0,0,0,0);
   }
   /*.button:hover {background-color: #0f8b8d}*/
   .button:active {
     background-color: #0f8b8d;
     box-shadow: 2 2px #CDCDCD;
     transform: translateY(2px);
   }
   .state {
     font-size: 1.5rem;
     color:#8c8c8c;
     font-weight: bold;
   }
  </style>
<title>Gaggia Classic Pro</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
</head>
<body>
  <div class="topnav">
    <h1>IOT ESPRESSOKEITIN</h1>
  </div>
  <div class="content">
    <div class="card">
      <h2>General Information</h2>
      <p class="value">On-time: <span id="ONTIME">%ONTIME%</span></p>
      <p class="value">Temperature: <span id="TEMP">%TEMP%</span></p>
      <p class="value">Brew Time: <span id="BREWTIME">%BREWTIME%</span></p>
      <p class="value">Pressure: <span id="PRESSURE">%PRESSURE%</span></p>
      <p class="value">State: <span id="STATE">%STATE%</span></p>
    </div>
    <div id='chart_div', style='height: 400px'></div>
    
    <div class="card">
      <h2>Heating Parameters:</h2>
      <p class="value">Brew Temperature: <span id="BREWTEMPREF">%BREWTEMPREF%</span></p>
      <div class="slidecontainer">
        <input type="range" min="80" max="110" value="95" class="slider" onchange="updateVal('BREWTEMPREF',this.value)">
      </div>
      <p class="value">Steam Temperature: <span id="STEAMTEMPREF">%STEAMTEMPREF%</span></p>
      <div class="slidecontainer">
        <input type="range" min="120" max="155" value="145" class="slider" onchange="updateVal('STEAMTEMPREF',this.value)">
      </div>
    </div>
    
    <div class="card">
      <h2>Brewing Parameters</h2>
      <p class="value">Pre-Infusion [s]: <span id="PREINFTIME">%PREINFTIME%</span></p>
      <div class="slidecontainer">
        <input type="range" min="0" max="15" value="8" class="slider" onchange="updateVal('PREINFTIME',this.value)">
      </div>
      <p class="value">Pre-Infusion [bar]: <span id="PREINFBAR">%PREINFBAR%</span></p>
      <div class="slidecontainer">
        <input type="range" min="1" max="10" value="2" step="0.25" class="slider" onchange="updateVal('PREINFBAR',this.value)">
      </div>
      <p class="value">Brew [s]: <span id="BREWTIMEREF">%BREWTIMEREF%</span></p>
      <div class="slidecontainer">
        <input type="range" min="10" max="45" value="30" class="slider" onchange="updateVal('BREWTIMEREF',this.value)">
      </div>
      <p class="value">Brew Initial [bar]: <span id="BREWPRESSUREINIT">%BREWPRESSUREINIT%</span></p>
      <div class="slidecontainer">
        <input type="range" min="1" max="10" value="9" step="0.25" class="slider" onchange="updateVal('BREWPRESSUREINIT',this.value)">
      </div>
      <p class="value">Brew Final [bar]: <span id="BREWPRESSUREEND">%BREWPRESSUREEND%</span></p>
      <div class="slidecontainer">
        <input type="range" min="1" max="10" value="8" step="0.25" class="slider" onchange="updateVal('BREWPRESSUREEND',this.value)">
      </div>
    </div>
  </div>
<script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);

  google.charts.load('current', {
    packages: ['corechart','line']
  });
  google.charts.setOnLoadCallback(drawChart);

  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
  function updateVal(Key_in,Value_in) {
    var Unit = {
      Key: Key_in,
      Value: Value_in
    };
    websocket.send(JSON.stringify(Unit));
  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    var state;
    var messages = JSON.parse(event.data);
    if(!Array.isArray(messages)){
      messages = [messages];
    }
    messages.forEach(function(message) {
      if (message.hasOwnProperty('Key')){
        document.getElementById(message.Key).innerHTML = message.Value;
      } else {
        updateChart(message);
      }
    });
  }
  function onLoad(event) {
    initWebSocket();
  }
  var data;
  var options;
  var chart;
  function drawChart() {
    data = google.visualization.arrayToDataTable([
      ['Second', 'Brew Pressure', 'Boiler Temperature'],
      [0, 0, 22]
    ]);
      // create options object with titles, colors, etc.
    options = {
      title: 'Real-Time Monitoring',
      curveType: 'function',
      legend: { position: 'none'},
      intervals: {'style':'line'},
      animation: {
        duration: 500
      },
      series: {
        0: {targetAxisIndex: 0},
        1: {targetAxisIndex: 1}
      },
      hAxis: {
        title: 'Time [s]',
        minValue: 0,
        gridlines: {interval : [1, 2, 2.5, 5]},
        scaleType: 'linear',
        viewWindow : {max: 60}
      },
      vAxes: {
        0: {title: 'Pressure (bar)',
            logScale: false,
            maxValue: 10},
        1: {title: 'Temperature (Celsius)',
            logScale: false, 
            maxValue: 160}
      }
    };
    // draw chart on load
    chart = new google.visualization.LineChart(
      document.getElementById('chart_div')
    );
    chart.draw(data, options);
  }
  var PrevTime = null;
  function updateChart(message) {
    let Time = JSON.parse(message.Time);
    let Pressure = JSON.parse(message.Pressure);
    let Temperature = JSON.parse(message.Temperature);

    let T_val = parseFloat(Time.Value);
    let P_val = parseFloat(Pressure.Value);
    let Temp_val = parseFloat(Temperature.Value);

    if (PrevTime !=  null && T_val < PrevTime) {
      data.removeRows(0,data.getNumberOfRows());
      options.hAxis.viewWindow = { min: T_val - 60, max: T_val };
    }

    PrevTime = T_val;

    data.addRow([T_val, P_val, Temp_val]);

    if (data.getNumberOfRows() > (75/0.5)){
      data.removeRow(0);
    }

    // Update the viewWindow to show the most recent 60 seconds of data
    let lastTime = parseFloat(data.getValue(data.getNumberOfRows()-1, 0));
    let firstTime = parseFloat(data.getValue(0, 0));
    let timeRange = lastTime - firstTime;

    if (timeRange > 60) {
      options.hAxis.viewWindow = { min: lastTime - 60, max: lastTime };
    } else {
      options.hAxis.viewWindow = { min: firstTime, max: lastTime };
    }

    // Update the viewWindow for the vertical axes
    let P_min = data.getColumnRange(1).min;
    let P_max = data.getColumnRange(1).max;
    let P_range = P_max - P_min;
    options.vAxes[0].viewWindow = { min: P_min - P_range * 0.1, max: P_max + P_range * 0.1 };

    let Temp_min = data.getColumnRange(2).min;
    let Temp_max = data.getColumnRange(2).max;
    let Temp_range = Temp_max - Temp_min;
    options.vAxes[1].viewWindow = { min: Temp_min - Temp_range * 0.1, max: Temp_max + Temp_range * 0.1 };

    chart.draw(data,options);
  }
</script>
</body>
</html>
)rawliteral";

#endif