/*
	This code manages a PZEM-004T power meter device through a Web server:
	- Values are reported every minute to MQTT (and optionnally Domoticz)
    - Debug can be done remotely through a local Telnet server
    - Embedded WEB server display status and allow settings display/changes
    - In addition to internal messages, Web server answers at the following URL:
		* /rest/data -> return all values formated as Milliseconds|Voltage|Current|Power|Energy|Frequency|Power Factor
		* /rest/millis -> return millis (milliseconds since boot)
		* /rest/voltage -> return voltage (V)
		* /rest/current -> return current (A)
		* /rest/power -> return power (W)
		* /rest/energy -> return energy (kWh)
		* /rest/frequency -> return frequency (Hz)
		* /rest/pf -> return power factor (0 to 1)
		* /rest/reset -> reset energy counter
		* /json/values -> return all values in json format
	- Data can also be broadcasted through UDP on request
		* Values is formated as Milliseconds|Voltage|Current|Power|Energy|Frequency|Power Factor

  External connections are:
    - ESP8266 RX/GPIO2 is connected to PZEM TX
    - ESP8266 TX/GPIO1 is connected to PZEM RX

  Precision:
	Voltage : 0.1V
	Current : 0.001A
	Power : 0.1W
	Energy : 0.001 kWh
	Frequency : 0.1 Hz
	Power Factor : 0.01 

	Written and maintained by Flying Domotic (https://github.com/FlyingDomotic/FF_PowerMeter)
*/

#define VERSION "1.0.0"										// Version of this code
#include <FF_WebServer.h>									// Defines associated to FF_WebServer class

//	User internal data
//		Declare here user's data needed by user's code

// ----- Domoticz -----
// Domoticz is supported on (asynchronous) MQTT
int domoticzVoltageIdx = 0;		// (V)  
int domoticzCurrentIdx = 0;     // (A)
int domoticzPowerIdx = 0;       // (W;kWh)
int domoticzFrequencyIdx = 0;   // (Hz)
int domoticzPfIdx = 0;          // Power factor

// ----- UDP broadcast -----
AsyncUDP udp;
int udpPort = 0;
int udpFlag = 0;

// ----- Power measurement -----
#include <PZEM004Tv30.h>                // https://github.com/mandulaj/PZEM-004T-v30
#ifdef PZEM004_NO_SWSERIAL
	// Define PZEM004_NO_SWSERIAL in PZEM004Tv30.h before compiling
	PZEM004Tv30 pzem(Serial);
#else
	#include <SoftwareSerial.h>
	SoftwareSerial pzemSerial(SOFTWARE_RX_PIN, SOFTWARE_TX_PIN);
	PZEM004Tv30 pzem(pzemSerial);
#endif

float checkPzemValue(float value);
void readPzem(void);

#define PZEM_INTERVAL_MS 1000                     // By default, get Pzem values every second
unsigned int pzemIntervalMs = PZEM_INTERVAL_MS;   // Interval between 2 PZEM reads (and UDP messages if enabled)
unsigned long lastPzemScanTime = 0;               // Time of last PZEM request
unsigned long lastPzemMqttTime = 0;               // Time of last PZEM MQTT sent message
bool pzemResetFlag = false;						  // Energy reset needed
float pzVoltage = 0;                              // (V)  
float pzCurrent = 0;                              // (A)
float pzPower = 0;                                // (W)
float pzEnergy = 0;                               // (kWh)
float pzFrequency = 0;                            // (Hz)
float pzPf = 0;                                   // Power factor

// Check Pzem values (returns -1 if NaN)
float checkPzemValue(float value) {
  if (isnan(value)) {
    return -1.0;
  } else {
    return value;
  }
}

//	User configuration data (should be in line with userconfigui.json)
//		Declare here user's configuration data needed by user's code
int configMQTT_Interval = 0;

// Declare here used callbacks
static CONFIG_CHANGED_CALLBACK(onConfigChangedCallback);
static DEBUG_COMMAND_CALLBACK(onDebugCommandCallback);
static REST_COMMAND_CALLBACK(onRestCommandCallback);
static JSON_COMMAND_CALLBACK(onJsonCommandCallback);

// Here are the callbacks code

/*!
	This routine is called when permanent configuration data has been changed.
		User should call FF_WebServer.load_user_config to get values defined in userconfigui.json.
		Values in config.json may also be get here.

	\param	none
	\return	none
*/

CONFIG_CHANGED_CALLBACK(onConfigChangedCallback) {
	trace_info_P("Entering %s", __func__);
	FF_WebServer.load_user_config("MQTTInterval", configMQTT_Interval);
	FF_WebServer.load_user_config("udpPort", udpPort);
	FF_WebServer.load_user_config("udpFlag", udpFlag);
	FF_WebServer.load_user_config("domoticzPowerIdx", domoticzPowerIdx);
	FF_WebServer.load_user_config("domoticzVoltageIdx", domoticzVoltageIdx);
	FF_WebServer.load_user_config("domoticzCurrentIdx", domoticzCurrentIdx);
	FF_WebServer.load_user_config("domoticzFrequencyIdx", domoticzFrequencyIdx);
	FF_WebServer.load_user_config("domoticzCosPhyIdx", domoticzPfIdx);
}

/*!
	This routine is called when a user's debug command is received.

	User should analyze here debug command and execute them properly.

	\note	Note that standard commands are already taken in account by server and never passed here.

	\param[in]	debugCommand: last debug command entered by user
	\return	none
*/

DEBUG_COMMAND_CALLBACK(onDebugCommandCallback) {
	trace_info_P("Entering %s with %s", __func__, debugCommand.c_str());
	// "user" command is a standard one used to print user variables
	if (debugCommand == "user") {
		trace_info_P("traceFlag=%d", FF_WebServer.traceFlag);
		trace_info_P("debugFlag=%d", FF_WebServer.debugFlag);
		// -- Add here your own user variables to print
		trace_info_P("pzVoltage=%.1f", pzVoltage);
		trace_info_P("pzCurrent=%.3f", pzCurrent);
		trace_info_P("pzPower=%.1f", pzPower);
		trace_info_P("pzEnergy=%.3f", pzEnergy);
		trace_info_P("pzFrequency=%.1f", pzFrequency);
		trace_info_P("pzPf=%.2f", pzPf);
		trace_info_P("domoticzVoltageIdx=%d", domoticzVoltageIdx);
		trace_info_P("domoticzCurrentIdx=%d", domoticzCurrentIdx);
		trace_info_P("domoticzPowerIdx=%d", domoticzPowerIdx);
		trace_info_P("domoticzFrequencyIdx=%d", domoticzFrequencyIdx);
		trace_info_P("domoticzPfIdx=%d", domoticzPfIdx);
		trace_info_P("udpPort=%d", udpPort);
		trace_info_P("udpFlag=%d", udpFlag);
		// -----------
		return true;
	}
	return false;
}

/*!
	This routine analyze and execute REST commands sent through /rest GET command
	It should answer valid requests using a request->send(<error code>, <content type>, <content>) and returning true.

	If no valid command can be found, should return false, to let server returning an error message.

	\note	Note that minimal implementation should support at least /rest/values, which is requested by index.html
		to get list of values to display on root main page. This should at least contain "header" topic,
		displayed at top of page. Standard header contains device name, versions of user code, FF_WebServer template
		followed by device uptime. You may send something different.
		It should then contain user's values to be displayed by index_user.html file.

	\param[in]	request: AsyncWebServerRequest structure describing user's request
	\return	true for valid answered by request->send command, false else
*/

REST_COMMAND_CALLBACK(onRestCommandCallback) {
	char tempBuffer[200];
	tempBuffer[0] = 0;
	if (request->url() == "/rest/values") {
		snprintf_P(tempBuffer, sizeof(tempBuffer),
			PSTR(
				// -- Put header composition
				"header|%s V%s/%s, up since %s|div\n"
				// -- Put here user variables to be available in index_user.html
				"v|%.1f|div\n"
				"a|%.3f|div\n"
				"w|%.1f|div\n"
				"kwh|%.3f|div\n"
				"hz|%.1f|div\n"
				"pf|%.2f|div\n"
				// -----------------
				)
			// -- Put here values of header line
			,FF_WebServer.getDeviceName().c_str()
			,VERSION, FF_WebServer.getWebServerVersion()
			,NTP.getUptimeString().c_str()
			// -- Put here values in index_user.html
			,pzVoltage
			,pzCurrent
			,pzPower
			,pzEnergy
			,pzFrequency
			,pzPf
			// -----------------
			);
		request->send(200, "text/plain", tempBuffer);
		return true;
	}
	trace_info_P("Entering %s with %s", __func__, request->url().c_str());					// Put trace here as /rest/value is called every second0
    if (request->url() == "/rest/data") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%lu|%.1f|%.3f|%.1f|%.3f|%.1f|%.2f", 
        millis(), pzVoltage, pzCurrent, pzPower, pzEnergy, pzFrequency, pzPf);
    } else if (request->url() == "/rest/millis") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%lu", millis());
    } else if (request->url() == "/rest/voltage") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%.1f", pzVoltage);
    } else if (request->url() == "/rest/current") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%.3f", pzCurrent);
    } else if (request->url() == "/rest/power") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%.1f", pzPower);
    } else if (request->url() == "/rest/energy") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%.3f", pzEnergy);
    } else if (request->url() == "/rest/reset") {
      snprintf(tempBuffer, sizeof(tempBuffer), "Energy reset at %.3f", pzEnergy);
	  pzemResetFlag = true;
    } else if (request->url() == "/rest/frequency") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%.1f", pzFrequency);
    } else if (request->url() == "/rest/pf") {
      snprintf(tempBuffer, sizeof(tempBuffer), "%.2f", pzPf);
	}
	if (tempBuffer[0]) {
		request->send(200, "text/plain", tempBuffer);
		return true;
	}
	return false;
}

/*!
	Set JSON command callback

	\param[in]	Address of user routine to be called when a JSON (/json) command is received
	\return	A pointer to this class instance
*/

JSON_COMMAND_CALLBACK(onJsonCommandCallback) {
	trace_info_P("Entering %s with %s", __func__, request->url().c_str());
	if (request->url() == "/json/values") {
		char tempBuffer[200];
		snprintf(tempBuffer, sizeof(tempBuffer), "{\"v\":%.1f,\"a\":%.3f,\"w\":%.1f,\"kwh\":%.3f,\"hz\":%.1f,\"pf\":%.2f}",
			pzVoltage, pzCurrent, pzPower, pzEnergy, pzFrequency, pzPf);
		request->send(200, "text/plain", tempBuffer);
		return true;
	}
	return false;
}

// Here are user's functions

// Read Pzem values
void readPzem(void) {
	if ((millis() - lastPzemScanTime) >= pzemIntervalMs) {
		pzVoltage = checkPzemValue(pzem.voltage());
		pzCurrent = checkPzemValue(pzem.current());
		pzPower = checkPzemValue(pzem.power());
		pzEnergy = checkPzemValue(pzem.energy());
		pzFrequency = checkPzemValue(pzem.frequency());
		pzPf = checkPzemValue(pzem.pf());
		lastPzemScanTime = millis();
		
		if (udpFlag && udpPort) {
			char msg[200];
			snprintf(msg, sizeof(msg), "%lu|%.1f|%.3f|%.1f|%.3f|%.1f|%.2f", millis(), pzVoltage, pzCurrent, pzPower, pzEnergy, pzFrequency, pzPf);
			udp.broadcastTo(msg, udpPort);
		}

		if ((millis() - lastPzemMqttTime) >= (((unsigned long) configMQTT_Interval) * 1000)) {
			if (pzVoltage > 0) {
				lastPzemMqttTime = millis();
				char buffer[200];
				time_t t = now();
				snprintf(buffer, sizeof(buffer), "{\"time\":\"%4d-%02d-%02dT%02d:%02d:%02d\",\"ENERGY\":{\"voltage\":%.1f,\"current\":%.3f,\"power\":%.1f,\"energy\":%.3f,\"frequency\":%.1f,\"factor\":%.2f}}",
					year(t), month(t), day(t), hour(t), minute(t), second(t), pzVoltage, pzCurrent, pzPower, pzEnergy, pzFrequency, pzPf);
				FF_WebServer.mqttPublish("energy", buffer);  
				if (domoticzPowerIdx) {
					FF_WebServer.sendDomoticzPower(domoticzPowerIdx, pzPower, pzEnergy);
				}
				if (domoticzVoltageIdx) {
					snprintf(buffer, sizeof(buffer), "%.1f", pzVoltage);
					FF_WebServer.sendDomoticzValues(domoticzVoltageIdx, buffer, 0);
				}
				if (domoticzCurrentIdx) {
					snprintf(buffer, sizeof(buffer), "%.3f", pzCurrent);
					FF_WebServer.sendDomoticzValues(domoticzCurrentIdx, buffer, 0);
				}
				if (domoticzFrequencyIdx) {
					snprintf(buffer, sizeof(buffer), "%.1f", pzFrequency);
					FF_WebServer.sendDomoticzValues(domoticzFrequencyIdx, buffer, 0);
				}
				if (domoticzPfIdx) {
					snprintf(buffer, sizeof(buffer), "%.2f", pzPf);
					FF_WebServer.sendDomoticzValues(domoticzPfIdx, buffer, 0);
				}
			}
		}
	}
}

//	This is the setup routine.
//		Initialize Serial, LittleFS and FF_WebServer.
//		You also have to set callbacks you need,
//		and define additional debug commands, if required.
void setup() {
	// Open serial connection
	Serial.begin(9600);
	// Start Little File System
	LittleFS.begin();
	// Start FF_WebServer
	FF_WebServer.begin(&LittleFS, VERSION);
	// Set user's callbacks
	FF_WebServer.setConfigChangedCallback(&onConfigChangedCallback);
	FF_WebServer.setDebugCommandCallback(&onDebugCommandCallback);
	FF_WebServer.setRestCommandCallback(&onRestCommandCallback);
	FF_WebServer.setJsonCommandCallback(&onJsonCommandCallback);
	// Initialize PZEM
	Serial.begin(9600);
}

//	This is the main loop.
//	Do what ever you want and call FF_WebServer.handle()
void loop() {
	// Reset energy if needed
	if (pzemResetFlag) {
		pzem.resetEnergy();
		pzemResetFlag = false;
	}
	// User part of loop
	readPzem();
	// Manage Web Server
	FF_WebServer.handle();
}
