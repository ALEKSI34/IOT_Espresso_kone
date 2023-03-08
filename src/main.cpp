/*********
  Aleksi Kettunen
  ESP32 - Espresso kone.
*********/

// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJSON.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <RTOS_PSM.h>
#include <PI.h>
#include "webpage.h"
#include "wifi_secrets.h"

// IO
#define relayPin 25 // Pin checked, Connect a port here. Port Component.
#define brewPin 12 // Pin checked, Connect brew button here. Port Component.
#define sensePin 27 // Pin checked, Connect sense from dimmer here. Port Component.
#define steamPin 13 // Pin checked, Connect steam button here. Port component.
#define PressurePin 34 // Pin checked, Connect pressure sensor here. Port Component.
#define dimmerControlPin 26 // Pin Checked. Port Component.

// SPI pins for MAX6675
#define thermoMISO 19 // Pin Checked, MISO, V_SPI_Q, Connect SO
#define thermoCS 23 // Pin Checked, MOSI/ V_SPI_D, Connect SS/CS
#define thermoCLK 5 // Pin checked, V_SPI_CS0, Connect SCK

SemaphoreHandle_t IOTSemaphore = xSemaphoreCreateBinary();

//MAX6675 thermocouple(thermoCLK, thermoCS, thermoMISO);

const uint8_t range = 100;
RTOS_PSM* pump;

// Replace with your network credentials
const char* ssid = SSID;
const char* password = WIFI_PASSWORD;

StaticJsonDocument<200> doc;
StaticJsonDocument<200> GeneralDoc;

hw_timer_t * updateTimer = NULL;

TaskHandle_t WebServerTask, OTATaskHandle, StateMachineTask, ControllerTask, KTypeUpdateTask, PressureUpdateTask, BoilerPITask, PressurePITask, BoilerPWMTaskHandle;

volatile uint8_t NO_INPUT, DATA_STORED = 0;

float_t BoilerReference, PressureReference = 0.0;
float_t temperatureReferenceOffset = 8.0; // Add the reference offset for gaggia classic.

uint8_t BrewCounter = 0; // When 1, count the brew counter.
float_t currentTime = 0; // Current running time of the ESP32. In float, as seconds, accuracy 0.5s
float_t brewTime = 0; // Current time of brewing
float_t brewTimeT0 = 0; // Brewing start time

typedef struct { String key; int val; } t_dictionary;

const String StateString[] = {
  "HEATING",
  "PREINFUSING",
  "BREWING",
  "STEAMING",
  "OVERHEATED"
};


enum States {
  HEATING = 0,
  PREINFUSING = 1,
  BREWING = 2,
  STEAMING = 3,
  OVERHEATED = 4
};

States StateMachineState = HEATING;
enum Variables {
  TEMP = 0,
  BREWTIME = 1,
  PRESSURE = 2,
  BREWTEMPREF = 3,
  STEAMTEMPREF = 4,
  PREINFTIME = 5,
  PREINFBAR = 6,
  BREWTIMEREF = 7,
  BREWPRESSUREINIT = 8,
  BREWPRESSUREEND = 9,
  ONTIME = 10,
  STATE = 11
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

typedef struct { float_t value; Variables eKey; } t_EspressoItem;

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

float_t BoilerDutycycle = 0.0;

// Pointers for easier access of these variables.
float_t *CurPressure = &EspressoIOTArray[PRESSURE].value; // Pointer to Current Pressure value
float_t *CurTemp = &EspressoIOTArray[TEMP].value; // Pointer to Current Temperature value
float_t *CurDutyCycleReference = &BoilerDutycycle;

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

void StoreParametersToEEPROM(uint8_t FirstAddress);

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

float_t InterpolateValueOnCurve(float_t xn,float_t y0, float_t y1, float_t x0, float_t x1) {
  // Simple linear interpolation implementation
  if (xn > x1) {
    return y1;
  } 
  else {
    return y0 + (xn - x0) * (y1 - y0) / (x1 - x0);
  }
}

/////////////////////////
//// WEBSERVER STUFF ////
/////////////////////////

String FormJSONMessage (String Key, String Value) {
  String jsonString = "";
  JsonObject object = doc.to<JsonObject>();
  if (Key != NULL) 
  {
    object["Key"] = Key;
  }
  else{
    object["Key"] = "MISC";
  }
    if (Value != NULL) 
  {
    object["Value"] = Value;
  }
  else
  {
    object["Value"] = "MISC";
  }
  serializeJson(doc, jsonString);
  return jsonString;
}

String mergeJSONMessage (const std::vector<String>& jsonList)
{
    String result = "[";
    for (const String& json : jsonList) {
        result += json + ",";
    }
    result.remove(result.length() - 1);
    result += "]";
    return result;
}

void handleWebSocketMessage(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    
    DeserializationError error = deserializeJson(doc, data);

    if (!error) {
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

String GetGeneralInformationSerialized(uint8_t bBrewing) {
  // Sends objects the plotting
  String jsonString = "";
  JsonObject object = GeneralDoc.to<JsonObject>();
  if (bBrewing) { 
    object["Time"] = FormJSONMessage(StringfromEnum(BREWTIME),String(EspressoIOTArray[BREWTIME].value));
  } else {
    object["Time"] = FormJSONMessage(StringfromEnum(ONTIME),String(currentTime));
  }
  object["Pressure"] = FormJSONMessage(StringfromEnum(PRESSURE),String(EspressoIOTArray[PRESSURE].value));
  object["Temperature"] = FormJSONMessage(StringfromEnum(TEMP),String(EspressoIOTArray[TEMP].value));
  serializeJson(GeneralDoc, jsonString);
  return jsonString;
}

void SetupWebServer(void * parameters) {
  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Start server
  server.begin();

  static float_t referenceSec = 0.0;

  String jsonmessage;
  String generalInformation;
  String stateString;
  String currentTimeString, stateMachineString, temperatureString, pressureString, brewTimeString;

  Serial.println("Webserver Initialized");

  xSemaphoreGive(IOTSemaphore); // Semaphore must be given so the program loop can start.

  uint8_t i;
  for (;;) {
    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
    ws.cleanupClients();
    currentTime += 0.5; // Visual time hacky hack
    stateString = StateString[StateMachineState];
    if (stateString == NULL) {
      stateString = String("MISC");
    }
    currentTimeString = FormJSONMessage(StringfromEnum(ONTIME),SecondsToString(currentTime)); // currentTime
    stateMachineString = FormJSONMessage(StringfromEnum(STATE),stateString); //StateMachineState
    temperatureString = FormJSONMessage(StringfromEnum(TEMP),String(*CurTemp));
    pressureString = FormJSONMessage(StringfromEnum(PRESSURE),String(*CurPressure));
    if (BrewCounter) {
      EspressoIOTArray[BREWTIME].value = currentTime-brewTimeT0;
    } else {
      brewTimeT0 = currentTime;
      EspressoIOTArray[BREWTIME].value = 0.0;
    }
    brewTimeString = FormJSONMessage(StringfromEnum(BREWTIME),SecondsToString(EspressoIOTArray[BREWTIME].value));
    generalInformation = GetGeneralInformationSerialized(BrewCounter);

    jsonmessage = mergeJSONMessage(
      {
        currentTimeString,
        stateMachineString,
        temperatureString,
        pressureString,
        brewTimeString
      }
    );
    xSemaphoreGive(IOTSemaphore);

    ws.textAll(jsonmessage);
    ws.textAll(generalInformation);

    vTaskDelay( 500 / portTICK_PERIOD_MS );

    if (NO_INPUT) {
      if (currentTime-referenceSec > 15 ) {
        if (!DATA_STORED) {
          xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
          UpdateParametersArray();
          StoreParametersToEEPROM(0);
          xSemaphoreGive(IOTSemaphore);
        }
      }
    } else {
      referenceSec = currentTime;
      NO_INPUT = 1;
    }
  }
  vTaskDelete(NULL);
}

//////////////////
//// STATES  /////
//////////////////

void HeatingState() {
  // HEATING STATE //
  BrewCounter = 0;
  BoilerReference = EspressoIOTArray[BREWTEMPREF].value;
  PressureReference = 0.0;
}

void SteamingState() {
  // STEAMING STATE //
  BrewCounter = 0;
  BoilerReference = EspressoIOTArray[STEAMTEMPREF].value;
  PressureReference = 0.0;
}

void PreInfusingState() {
  // PREINFUSING STATE //

  BrewCounter = 1;

  PressureReference = EspressoIOTArray[PREINFBAR].value;
  BoilerReference = EspressoIOTArray[BREWTEMPREF].value;
}

void BrewingState() {
  // BREWING STATE //
  BrewCounter = 1;

  BoilerReference = EspressoIOTArray[BREWTEMPREF].value;

  // Update Pressure reference accordingly.
  PressureReference = InterpolateValueOnCurve(
    brewTime, 
    EspressoIOTArray[BREWPRESSUREINIT].value,
    EspressoIOTArray[BREWPRESSUREEND].value,
    EspressoIOTArray[PREINFTIME].value,
    EspressoIOTArray[BREWTIMEREF].value  
  );
}

void OverheatingState() {
  // STEAMING STATE //
  digitalWrite(relayPin, LOW);
  BoilerReference = 0.0;
  PressureReference = 0.0;
}

void RunMainStateMachine(void * parameters)  {

  Serial.println("Statemachine Initialized");

  for (;;) {
    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);

    uint8_t BrewSwitch = !digitalRead(brewPin);
    uint8_t SteamSwitch = !digitalRead(steamPin);
    uint8_t PreInfusingEnabled = (EspressoIOTArray[PREINFTIME].value > 0.5);
    // Main state machine

    // Check for overheating
    if (*CurTemp > float_t(160.0)) {
      StateMachineState = OVERHEATED;
    }

    switch (StateMachineState)
    {
    case HEATING:
      if (BrewSwitch) {
        StateMachineState = PREINFUSING;
        brewTimeT0 = currentTime;
        brewTime = 0.0;
      } 
      else if (SteamSwitch){
        StateMachineState = STEAMING;
      } 
      else {
        HeatingState();
      }
      break;
    case STEAMING:
      if (!SteamSwitch) {
        StateMachineState = HEATING;
      }
      else {
        SteamingState();
      }
      break;
    case PREINFUSING:
      brewTime += 0.05;
      if (!BrewSwitch) {
        StateMachineState = HEATING;
      }
      else if (brewTime >= EspressoIOTArray[PREINFTIME].value) 
      {
        StateMachineState = BREWING;
      }
      else if (PreInfusingEnabled) 
      {
        PreInfusingState();
      }
      else 
      {
        StateMachineState = BREWING;
      }
      break;
    case BREWING:
      brewTime += 0.05;
      if (BrewSwitch) {
        BrewingState();
      } else {
        StateMachineState = HEATING;
      }
      break;
    case OVERHEATED:
      if (*CurTemp > float_t(160.0)) {
        OverheatingState();
      }
      else {
        StateMachineState = HEATING;
      }
      break;
    }

    EspressoIOTArray[STATE].value = StateMachineState;

    xSemaphoreGive(IOTSemaphore);
    vTaskDelay( 50 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

/////////////////////////////
//// SENSOR UPDATE STUFF ////
/////////////////////////////

byte spiread(void) {
  int i;
  byte d = 0;

  for (i=7; i>=0; i--)
  {
    digitalWrite(thermoCLK, LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    if (digitalRead(thermoMISO)) {
      //set the bit to 0 no matter what
      d |= (1 << i);
    }

    digitalWrite(thermoCLK, HIGH);
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }

  return d;
}


float_t kThermoread(){
  uint16_t v;

  digitalWrite(thermoCS, LOW);

  vTaskDelay(1 / portTICK_PERIOD_MS);

  v = spiread();
  v <<= 8;
  v |= spiread();

  digitalWrite(thermoCS, HIGH);
  vTaskDelay(1 / portTICK_PERIOD_MS);

  if (v & 0x4) {
    return NAN;
  }
  v >>= 3;

  return (float_t) v*0.25;
}

void UpdateKTypeTemp(void * parameters) {
  // Temperature value arrives via SPI from MAX6675
  float_t temperature;

  Serial.println("Temperature sensor Initialized");

  for (;;) {
    temperature = kThermoread();
    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
    *CurTemp = temperature;
    xSemaphoreGive(IOTSemaphore);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

float_t PressureTransducerVoltage() {
  // ESP32 has black zones in 0-0.1V and 3.2V-3.3V.
  uint16_t measurement = analogRead(PressurePin);
  float_t espVoltage;
  if (measurement <= 0) 
  {
    espVoltage = 0.1;
  } else if (measurement >= 4095) 
  {
    espVoltage = 3.2;
  }
  else
  {
    espVoltage = float(measurement)/4096 * (3.1) + 0.1;
  }
  return espVoltage * (5.0/3.3);
}

void UpdatePressure(void * parameters) {
  // Pressure Sensor reports output, in range 0.5V for 0bar, 4.5V for 12bar. 
  // ESP32 can only accept signal upto 3.3V. Voltage conversion is applied with simple non inverting op-amp circuit and a voltage divider.
  // This will mean that, the ranges of these input signals stay the same.
  // So the total RANGE of these voltages is 4.0V, with offset of 0.5V.
  float_t Offset = 0.5; // Volts
  float_t MaxBar = 12; // Bar
  float_t Range = 4; // Volts
  float_t Prev_Voltage = PressureTransducerVoltage();

  Serial.println("Pressure sensor Initialized");

  for(;;) {
    float_t Voltage = PressureTransducerVoltage();
    // Update the current pressure. Use average of 2 previous measurements as the voltage value, for some filtering.
    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
    *CurPressure = MaxBar*((((Voltage+Prev_Voltage)/2)-Offset)/Range);
    xSemaphoreGive(IOTSemaphore);
    Prev_Voltage = Voltage;
    vTaskDelay(12.5 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}


/////////////////////////
//// PID CONTROLLERS ////
/////////////////////////

void BoilerPIController(void * parameters) {
  // PI Controller, adjusting the boiler temperature
  // Since the frequency of PWM frequency for relay must be relatively slow (1-10Hz), we can just generate it ourselves.

  unsigned int piControllerStepSize = 500;

  // Define Ranges for unit conversion
  float OutputRange[] = {0.0, 1.0};
  PI_Controller PIC = PI_Controller(0.05, 0.01, OutputRange, 1, piControllerStepSize);

  Serial.println("Boiler PI Initialized");

  for (;;) {
    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
    PIC.reference = BoilerReference+temperatureReferenceOffset; // Add 
    PIC.Calculate(*CurTemp);
    *CurDutyCycleReference = PIC.output;
    xSemaphoreGive(IOTSemaphore);
    vTaskDelay(piControllerStepSize / portTICK_PERIOD_MS); // Sleep 10 ms

  }
  vTaskDelete(NULL);
}

void BoilerPWMTask(void * parameters) {

  uint16_t PWM_RESOLUTION = 100;
  uint16_t PERIOD_MS = 500; // 2 Hz.
  uint16_t STEPSIZE_MS = (PERIOD_MS / PWM_RESOLUTION);

  for (;;) {

    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
    float_t PWM_DUTY_CYCLE = *CurDutyCycleReference;
    xSemaphoreGive(IOTSemaphore);
    int onTime = (int)(PWM_DUTY_CYCLE * PERIOD_MS);
    int offtime =  PERIOD_MS - onTime;

    digitalWrite(relayPin, HIGH);
    vTaskDelay(onTime / portTICK_PERIOD_MS);

    digitalWrite(relayPin, LOW);
    vTaskDelay(offtime / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void PressurePIController(void * parameters) {
  // PI Controller for pressure.

  // Define Ranges for unit conversion
  float OutputRange[] = {0, float(range)};

  unsigned int piControllerStepSize = 25;

  PI_Controller PIC = PI_Controller(0.1, 0.05, OutputRange, 100, piControllerStepSize);

  Serial.println("Pressure PI Initialized");

  for (;;) {
    xSemaphoreTake(IOTSemaphore, portMAX_DELAY);
    PIC.reference = PressureReference;
    PIC.Calculate(*CurPressure);
    if (PressureReference < 1.0) {
      pump->set(0);
    } else {
      pump->set(PIC.output);
    }
    xSemaphoreGive(IOTSemaphore);
    vTaskDelay( piControllerStepSize / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}


//////////////////////
//// EEPROM STUFF ////
//////////////////////

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

//////////////////////
////  OTA TASK    ////
//////////////////////

void HandleOTAUpdates(void *pvParameters) {
  for (;;)
  {
    ArduinoOTA.handle();
    vTaskDelay(1000/ portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  EEPROM.begin(30); // We don't need more that 30 bytes

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Initialize OTA
  

  ArduinoOTA.onStart([]() {Serial.println("Starting over the air update...");});
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setPassword((const char *)"password");
  ArduinoOTA.begin();
  Serial.println("OTA begin triggered. Auth: Arduino:password");
  
  // Initialize I/O
  pinMode(relayPin, OUTPUT);
  pinMode(brewPin, INPUT_PULLUP);
  pinMode(steamPin, INPUT_PULLUP);

  pinMode(PressurePin, INPUT_PULLDOWN);

  // Initialize MAX6675
  pinMode(thermoCS, OUTPUT);
  pinMode(thermoCLK, OUTPUT);
  pinMode(thermoMISO, INPUT);

  digitalWrite(thermoCS,HIGH);

  // Get stuff from EEPROM
  uint8_t ParametersEEAddress = 0;
  GetParametersFromEEPROM(ParametersEEAddress); // Load up parameters from EEPROM
  SyncParametersToLive();
  
  pump = new RTOS_PSM(sensePin, dimmerControlPin, range, RISING, 2, 4); // Pump initializes new task. Initializing before setup, causes memory leaks and errors

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
    HandleOTAUpdates,
    "OTAHandlerTask",
    8192,
    NULL,
    1,
    &OTATaskHandle,
    CONFIG_ARDUINO_RUNNING_CORE
  );

  xTaskCreatePinnedToCore(
    RunMainStateMachine,
    "StateMachineTask",
    10000,
    NULL,
    1, // Priority
    &ControllerTask,
    !CONFIG_ARDUINO_RUNNING_CORE // Run the task on the another core, opposed to webserver.
  ); 

  xTaskCreatePinnedToCore(
    UpdateKTypeTemp,
    "KTypeUpdateTask",
    10000,
    NULL,
    3, // Priority
    &KTypeUpdateTask,
    !CONFIG_ARDUINO_RUNNING_CORE // Run the task on the another core, opposed to webserver.
  ); 

  xTaskCreatePinnedToCore(
    UpdatePressure,
    "PressureUpdateTask",
    10000,
    NULL,
    3, // Priority
    &PressureUpdateTask,
    !CONFIG_ARDUINO_RUNNING_CORE // Run the task on the another core, opposed to webserver.
  );

  xTaskCreatePinnedToCore(
    BoilerPIController,
    "Boiler PI Controller Task",
    10000,
    NULL,
    2, // Priority
    &BoilerPITask,
    !CONFIG_ARDUINO_RUNNING_CORE // Run the task on the another core, opposed to webserver.
  );

  xTaskCreatePinnedToCore(
    PressurePIController,
    "Pressure PI Controller Task",
    10000,
    NULL,
    2, // Priority
    &PressurePITask,
    !CONFIG_ARDUINO_RUNNING_CORE // Run the task on the another core, opposed to webserver.
  );
  xTaskCreatePinnedToCore(
    BoilerPWMTask,
    "Boiler PWM Generation",
    2048,
    NULL,
    1,
    &BoilerPWMTaskHandle,
    !CONFIG_ARDUINO_RUNNING_CORE
  );
}

void loop() {
  // Loop function is empty. RTOS handles running tasks.
}
