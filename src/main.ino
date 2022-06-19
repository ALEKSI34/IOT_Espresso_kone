/*********
  Aleksi Kettunen
  ESP32 - Espresso kone.
*********/

// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJSON.h>
#include <EEPROM.h>
#include <max6675.h>
#include <PSM.h>

// IO
#define relayPin 3
#define brewPin 8
#define zcPin 2
#define steamPin 9
#define PressurePin 10
#define dimmerPin 11

// SPI pins for MAX6675
#define thermoDO 19
#define thermoCS 23
#define thermoCLK 5

//MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

const uint8_t range = 127;
//PSM pump(zcPin, dimmerPin, range, FALLING);

// Replace with your network credentials
const char* ssid = "KRP 01";
const char* password = "Salasana1";

StaticJsonDocument<200> doc;
StaticJsonDocument<200> GeneralDoc;

hw_timer_t * updateTimer = NULL;

TaskHandle_t WebServerTask, ControllerTask;

volatile uint8_t NO_INPUT, DATA_STORED = 0;

typedef struct { String key; int val; } t_dictionary;

enum States {
  HEATING = 0,
  PREINFUSING = 1,
  BREWING = 2,
  STEAMING = 3
};

const char *StateStrings[4] = {
  "HEATING",
  "PREINFUSING",
  "BREWING",
  "STEAMING"
};

enum Variables {
  ONTIME = 0,
  TEMP = 1,
  BREWTIME = 2,
  PRESSURE = 3,
  STATE = 4,
  BREWTEMPREF = 5,
  STEAMTEMPREF = 6,
  PREINFTIME = 7,
  PREINFBAR = 8,
  BREWTIMEREF = 9,
  BREWPRESSUREINIT = 10,
  BREWPRESSUREEND = 11
};

static t_dictionary lookuptable[] = {
  {"ONTIME", ONTIME},
  {"TEMP", TEMP},
  {"BREWTIME", BREWTIME},
  {"PRESSURE", PRESSURE},
  {"STATE", STATE},
  {"BREWTEMPREF", BREWTEMPREF},
  {"STEAMTEMPREF", STEAMTEMPREF},
  {"PREINFTIME", PREINFTIME},
  {"PREINFBAR", PREINFBAR},
  {"BREWTIMEREF", BREWTIMEREF},
  {"BREWPRESSUREINIT", BREWPRESSUREINIT},
  {"BREWPRESSUREEND", BREWPRESSUREEND}
};

#define NKEYS 12

enum OutputType {
  eFloat = 0,
  eTime = 1,
  eStateString = 2
};

typedef struct { float_t value; Variables eKey; } t_EspressoItem;

SemaphoreHandle_t IOTSemaphore = xSemaphoreCreateMutex();
t_EspressoItem EspressoIOTArray[] = {
  {0.0, TEMP},
  {0.0, BREWTIME},
  {0.0, PRESSURE},
  {0.0, BREWTEMPREF},
  {0.0, STEAMTEMPREF},
  {0.0, PREINFTIME},
  {0.0, PREINFBAR},
  {0.0, BREWTIMEREF},
  {0.0, BREWPRESSUREINIT},
  {0.0, BREWPRESSUREEND}
};
uint8_t EspIOTArrLen = 10;
// Pointers for easier access of these variables.
float_t *CurPressure = &EspressoIOTArray[2].value;
float_t *CurTemp = &EspressoIOTArray[0].value;

// Stuff stored to EEPROM, initialized to 0.0, updated from old values from eeprom.
t_EspressoItem BrewParameters[] = {
  {0.0, BREWTEMPREF},
  {0.0, STEAMTEMPREF},
  {0.0, PREINFTIME},
  {0.0, PREINFBAR},
  {0.0, BREWTIMEREF},
  {0.0, BREWPRESSUREINIT},
  {0.0, BREWPRESSUREEND}
};
const uint8_t ParameterCount = 7;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

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
    var message = JSON.parse(event.data);
    if (message.hasOwnProperty('Key')){
      document.getElementById(message.Key).innerHTML = message.Value;
    } else {
      updateChart(message);
    }
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
        viewWindow : {max: 30}
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
  function updateChart(message) {
    let Time = JSON.parse(message.Time);
    let Pressure = JSON.parse(message.Pressure);
    let Temperature = JSON.parse(message.Temperature);

    let T_val = parseFloat(Time.Value);
    let P_val = parseFloat(Pressure.Value);
    let Temp_val = parseFloat(Temperature.Value);

    data.addRow([T_val, P_val, Temp_val]);
    if (T_val > 30) {
        options.hAxis.viewWindow = { min: T_val-30, max: T_val};
    } 
    if (data.getNumberOfRows() > (45/0.5)){
      data.removeRow(0);
    }
    chart.draw(data,options);
  }
</script>
</body>
</html>
)rawliteral";

String FormJSONMessage (String Key, String Value) {
  String jsonString = "";
  JsonObject object = doc.to<JsonObject>();
  object["Key"] = Key;
  object["Value"] = Value;
  serializeJson(doc, jsonString);
  return jsonString;
}

void handleWebSocketMessage(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    
    DeserializationError error = deserializeJson(doc, data);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      UpdateInternalValue(doc["Key"],doc["Value"]);
      ws.textAll(FormJSONMessage(doc["Key"],doc["Value"]));
      DATA_STORED = 0; // Flag for plausible data update.
      NO_INPUT = 0;
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage( server, client, type, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

int keyfromstring(const String& var){
  int i = 0;
  t_dictionary *item;
  for ( i = 0; i < NKEYS; i++) {
    t_dictionary *item = &lookuptable[i];
    if (item->key == var){
      return item->val;
    }
  }
  return -1;
}

String StringfromEnum(Variables var) {
  int i = 0;
  t_dictionary *item;
  for ( i = 0; i < NKEYS; i++) {
    t_dictionary *item = &lookuptable[i];
    if (item->val == var){
      return item->key;
    }
  }
  return String();
}

String SecondsToString(int32_t seconds) {
  String Minutes = String(int32_t(seconds / 60));
  String Seconds = String(int32_t(seconds % 60));
  return Minutes + " min "+ Seconds + " s";
}

String processor(const String& var){
  uint8_t i;
  int Key = keyfromstring(var);
  for (i = 0; i < EspIOTArrLen; i++) {
    if (EspressoIOTArray[i].eKey == Key) {
      return String(EspressoIOTArray[i].value);
    }
  }
  return String();
}

uint8_t IfUpdate = 0;

void onUpdate(void){
  IfUpdate = 1;
}

String GetGeneralInformationSerialized(float seconds) {
  String jsonString = "";
  JsonObject object = GeneralDoc.to<JsonObject>();
  object["Time"] = FormJSONMessage(StringfromEnum(ONTIME),String(seconds));
  object["Pressure"] = FormJSONMessage(StringfromEnum(PRESSURE),String(EspressoIOTArray[2].value));
  object["Temperature"] = FormJSONMessage(StringfromEnum(TEMP),String(EspressoIOTArray[0].value));
  serializeJson(GeneralDoc, jsonString);
  return jsonString;
}


States StateMachineState = HEATING;
static float seconds = 0;

void SetupWebServer(void * parameters) {
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Start server
  server.begin();

  static float referenceSec = 0.0;

  for (;;) {
    ws.cleanupClients();
    seconds += 0.5; // Visual time hacky hack
    xSemaphoreTake( IOTSemaphore, portMAX_DELAY);
    ws.textAll(FormJSONMessage(StringfromEnum(ONTIME),SecondsToString(seconds)));
    ws.textAll(FormJSONMessage(StringfromEnum(STATE),String(StateStrings[StateMachineState])));
    ws.textAll(GetGeneralInformationSerialized(seconds));
    xSemaphoreGive(IOTSemaphore);
    vTaskDelay( 500 / portTICK_PERIOD_MS );
    if (NO_INPUT) {
      if (seconds-referenceSec > 15 ) {
        if (!DATA_STORED) {
          UpdateParametersArray();
          StoreParametersToEEPROM(0);
        }
      }
    } else {
      referenceSec = seconds;
      NO_INPUT = 1;
    }
  }
  vTaskDelete(NULL);
}

void RunMainController(void * parameters)  {
  for (;;) {
    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
    *CurPressure = float_t(10.0);
    *CurTemp = float_t(10.0);
    xSemaphoreGive(IOTSemaphore);
    vTaskDelay( 50 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void UpdateInternalValue(String Key, String Value) {
  uint8_t i;
  int KeyEnum = keyfromstring(Key);
  for (i = 0; i < EspIOTArrLen; i++) {
    if (EspressoIOTArray[i].eKey == KeyEnum) {
        EspressoIOTArray[i].value = Value.toFloat();
        Serial.println(Key + " Changed to -> " + Value);
        break;
    }
  }
}

void UpdateParametersArray() {
  t_EspressoItem *LiveValue, *ParamValue;

  uint8_t i,j;
  for (i = 0; i < EspIOTArrLen; i++) {
    for (j = 0; j < ParameterCount; j++) {
      LiveValue = &EspressoIOTArray[i];
      ParamValue = &BrewParameters[j];
      if (LiveValue->eKey == ParamValue->eKey) {
        ParamValue->value = LiveValue->value;
      }
    }
  }
}

void SyncParametersToLive() {
  t_EspressoItem *LiveValue, *ParamValue;

  uint8_t i,j;
  for (i = 0; i < EspIOTArrLen; i++) {
    for (j = 0; j < ParameterCount; j++) {
      LiveValue = &EspressoIOTArray[i];
      ParamValue = &BrewParameters[j];
      if (LiveValue->eKey == ParamValue->eKey) {
        LiveValue->value = ParamValue->value;
      }
    }
  }
}

void StoreParametersToEEPROM(uint8_t FirstAddress){
  uint8_t eeAddress = FirstAddress;
  uint8_t i = 0;
  float_t *Value;
  Serial.printf("Storing Values to EEPROM...\n");
  for (i = 0; i < ParameterCount;i++) {
    t_EspressoItem *CurrentParam = &BrewParameters[i];
    Value = &(CurrentParam->value);
    if (*Value != EEPROM.readFloat(eeAddress)) {
      EEPROM.writeFloat(eeAddress, *Value);
      EEPROM.commit();
      Serial.println(StringfromEnum(CurrentParam->eKey)+" => "+String(EEPROM.readFloat(eeAddress)) + " Address: "+String(eeAddress));
    } else {
      Serial.println(StringfromEnum(CurrentParam->eKey)+ " => X, No change detected. Value: "+String(*Value));
    }
    eeAddress += sizeof(float_t);
  }
  DATA_STORED = 1;
  Serial.printf("-----------------------\n");
}

void GetParametersFromEEPROM(uint8_t FirstAddress){
  uint8_t eeAddress = FirstAddress; // Start address to proper place.
  uint8_t i, pByte = 0;
  float_t Value;
  Serial.printf("Reading Values from EEPROM...\n");
  for (i = 0; i < ParameterCount;i++) {
    t_EspressoItem *CurrentParam = &BrewParameters[i];

    Value = EEPROM.readFloat(eeAddress);
    CurrentParam->value = Value;
    Serial.println(StringfromEnum(CurrentParam->eKey)+" == "+String(CurrentParam->value)+ " Address: "+ String(eeAddress));
    eeAddress += sizeof(float_t);
  }
  Serial.printf("-----------------------\n");
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  EEPROM.begin(30); // We don't need more that 30 bytes

  //pinMode(relayPin, OUTPUT);
  //pinMode(brewPin, INPUT_PULLUP);
  //pinMode(steamPin, INPUT_PULLUP);

  uint8_t ParametersEEAddress = 0;

  GetParametersFromEEPROM(ParametersEEAddress); // Load up parameters from EEPROM

  SyncParametersToLive();

  updateTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(updateTimer, &onUpdate, true);
  timerAlarmWrite(updateTimer, 1000000, true);
  timerAlarmEnable(updateTimer);
  
  xTaskCreatePinnedToCore(
    SetupWebServer,
    "WebServerTask",
    10000,
    NULL,
    2,
    &WebServerTask,
    CONFIG_ARDUINO_RUNNING_CORE
  );

  
  xTaskCreatePinnedToCore(
    RunMainController,
    "ControllerTask",
    10000,
    NULL,
    1,
    &ControllerTask,
    !CONFIG_ARDUINO_RUNNING_CORE
  ); 
}

void loop() {

}
