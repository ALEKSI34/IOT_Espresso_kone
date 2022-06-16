/*********
  Aleksi Kettunen
  ESP32 - Espresso kone.
*********/

// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Replace with your network credentials
const char* ssid = "KRP 01";
const char* password = "Salasana1";

hw_timer_t * updateTimer = NULL;

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

enum OutputType {
  eFloat = 0,
  eTime = 1,
  eStateString = 2
};

typedef struct { float value; Variables eKey; } t_EspressoItem;

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
  {0.0, BREWPRESSUREEND},
};



#define NKEYS 12

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
    cursor-colo
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
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
  function updateVal(Key,Value) {
    BaseStr = '';
    websocket.send(BaseStr.concat(Key,'~',Value));
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
    var message = event.data;
    var fields = message.split('~');
    var key = fields[0];
    var value = fields[1];
    document.getElementById(key).innerHTML = value;
  }
  function onLoad(event) {
    initWebSocket();
  }
</script>
</body>
</html>
)rawliteral";

void notifyClients(String Key, String Value) {
  Serial.println(Key+String("~")+Value+String('\n'));
  ws.textAll(Key+"~"+Value);
}

void handleWebSocketMessage(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    //Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
    //Serial.printf("\n");
    data[len] = 0;
    
    char* Message = strtok((char*)data, "~");
    if (Message) {
      char *Key = Message;
      Message = strtok(NULL, "~");
      char *Value = Message;
      notifyClients(String(Key),String(Value));
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
  switch (keyfromstring(var)) {
    case ONTIME:
      return "15 min 26 s";
    case TEMP:
      return "82.4";
    case BREWTIME:
      return "0.0";
    case PRESSURE:
      return "1.0";
    case STATE:
      return "HEATING";
    case BREWTEMPREF:
      return "95";
    case STEAMTEMPREF:
      return "145";
    case PREINFTIME:
      return "8 s";
    case PREINFBAR:
      return "2 bar";
    case BREWTIMEREF:
      return "30 s";
    case BREWPRESSUREINIT:
      return "9 bar";
    case BREWPRESSUREEND:
      return "8 bar";
  }
  return String();
}

uint8_t IfUpdate = 0;

void onUpdate(void){
  IfUpdate = 1;
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  
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

  updateTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(updateTimer, &onUpdate, true);
  timerAlarmWrite(updateTimer, 1000000, true);
  timerAlarmEnable(updateTimer);
}

States StateMachineState = HEATING;
static int32_t seconds = 0;
void loop() {
  uint8_t i;
  ws.cleanupClients();
  if (IfUpdate) {
    seconds += 1;
    ws.textAll(StringfromEnum(ONTIME)+'~'+SecondsToString(seconds));
    ws.textAll(StringfromEnum(STATE)+'~'+String(StateStrings[StateMachineState]));
    IfUpdate = 0;  
  }
}
