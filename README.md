# FF_PowerMeter
 This module connects a PZEM004T power meter module to Wifi, sending data though HTTP, MQTT, UDP and/or Domoticz.

## What's for?

Insert in a wall plug, with a PZEM004T, an ESP8266, a 5V power supply and this code, voltage, current, power, total energy, frequency and power Factor are displayed every seconds, on request sent by UDP with the same period, and sent given an interval (by default 60 seconds) by MQTT and/or Domoticz. Messages may contain all data, or only individual parts. In addition, requests may be done throuth http at any intervals, still globally or individually, in text or json format.

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

## Prerequisites

Can be used directly with Arduino IDE or PlatformIO.

The following libraries are used:
- https://github.com/me-no-dev/ESPAsyncTCP
- https://github.com/gmag11/NtpClient
- https://github.com/me-no-dev/ESPAsyncWebServer
- https://github.com/bblanchon/ArduinoJson
- https://github.com/FlyingDomotic/FF_Trace
- https://github.com/arcao/Syslog
- https://github.com/marvinroger/async-mqtt-client
- https://github.com/joaolopesf/RemoteDebug
- https://github.com/FlyingDomotic/FF_WebServer
- https://github.com/mandulaj/PZEM-004T-v30

## Installation

Clone repository somewhere on your disk.
```
cd <where_you_want_to_install_it>
git clone https://github.com/FlyingDomotic/FF_PowerMeter.git FF_PowerMeter
```

## Update

Go to code folder and pull new version:
```
cd <where_you_installed_FF_PowerMeter>
git pull
```

Note: if you did any changes to files and `git pull` command doesn't work for you anymore, you could stash all local changes using:
```
git stash
```
or
```
git checkout <modified file>
```
Warning: as files in data folder should be downloaded on ESP file system, it may be a good idea to check if some of them have been changed be a new version, to update them on your ESP.

Remind also that config.json and userconfig.json are specific to your installation. Don't forget to download them from your ESP before making a global file system upload.

## Parameters at compile time

The following parameters are used at compile time:
	REMOTE_DEBUG: (default=defined) Enable telnet remote debug (optional)
	SERIAL_DEBUG: (default=defined) Enable serial debug (optional, only if REMOTE_DEBUG is disabled)
	SERIAL_COMMAND_PREFIX: (default=no defined) Prefix of debug commands given on Serial (i.e. `command:). No commands are allowed on Serial if not defined
	NO_SERIAL_COMMAND_CALLBACK: (default: not defined) Disable Serial command callback
	HARDWARE_WATCHDOG_PIN: (default=D4) Enable watchdog external circuit on D4 (optional)
	HARDWARE_WATCHDOG_ON_DELAY: (default=5000) Define watchdog level on delay (in ms)
	HARDWARE_WATCHDOG_OFF_DELAY: (default=1) Define watchdog level off delay (in ms)
	HARDWARE_WATCHDOG_INITIAL_STATE (default 0) Define watchdog initial state
	FF_TRACE_KEEP_ALIVE: (default=5 * 60 * 1000) Trace keep alive timer (optional)
	FF_TRACE_USE_SYSLOG: (default=defined) Send trace messages on Syslog (optional)
	FF_TRACE_USE_SERIAL: (default=defined) Send trace messages on Serial (optional)
	INCLUDE_DOMOTICZ:(default=defined) Include Domoticz support (optional)
	CONNECTION_LED: (default=-1) Connection LED pin, -1 to disable
	AP_ENABLE_BUTTON: (default=-1) Button pin to enable AP during startup for configuration, -1 to disable
	AP_ENABLE_TIMEOUT: (default=240) If the device can not connect to WiFi it will switch to AP mode after this time (Seconds, max 255),  -1 to disable
	DEBUG_FF_WEBSERVER: (default=defined) Enable internal FF_WebServer debug
	FF_DISABLE_DEFAULT_TRACE: (default=not defined) Disable default trace callback
	FF_TRACE_USE_SYSLOG: (default=defined) SYSLOG to be used for trace


## Parameters defined at run time

The following parameters can be defined, either in json files before loading LittleFS file system, or through internal http server.

### In config.json:
	ssid: Wifi SSID to connect to
	pass: Wifi key
	ip: this node's fix IP address (useless if DHCP is true)
	netmask: this node's netmask (useless if DHCP is true)
	gateway: this node's IP gateway (useless if DHCP is true)
	dns: this node's DHCP server (useless if DHCP is true)
	dhcp: use DHCP if true, fix IP address else
	ntp: NTP server to use
	NTPperiod: interval between two NTP updates (in minutes)
	timeZone: NTP time zone (use internal HTTP server to set)
	daylight: true if DST is used
	deviceName: this node name

### In userconfig.json:
	MQTTClientID: MQTT client ID, used during connection to MQTT
	MQTTUser: User to connect to MQTT server²
	MQTTPass: Password to connect to MQTT server
	MQTTTopic: base MQTT topic, used to set LWT topic of SMS server
	MQTTCommandTopic: topic to read debug commands to be executed
	MQTTHost: MQTT server to connect to
	MQTTPort: MQTT port to connect to
	MQTTInterval: MQTT/Domoticz update interval
	SyslogServer: syslog server to use (empty if not to be used)
	SyslogPort: syslog port to use (empty if not to be used)
	udpPort: UDP port to send messages every seconds to
	udpFlag: set to True to send UDP messages
	domoticzPowerIdx: IDX of Domoticz power/energy device (or zero)
	domoticzVoltageIdx: IDX of Domoticz voltage device (or zero)
	domoticzCurrentIdx: IDX of Domoticz current device (or zero)
	domoticzFrequencyIdx: IDX of Domoticz frequency device (or zero)
	domoticzCosPhyIdx: IDX of Domoticz power factor device (or zero)

## Debug commands

Debug commands are available to help understanding what happens, and may be a good starting point to help troubleshooting.

Debug output is available on Telnet, Serial and Syslog. Note that settings can disable some of these outputs.

### Access
Debug is available through:
- Telnet: connect with a telnet tool on ESP's port 23, and strike commands on keyboard
- MQTT: send the raw command to mqttCommandTopic (don't set the retain flag unless you want the command to be executed at each ESP restart)

### Commands
The following commands are allowed:
	- ? or h or help: display this message
	- m: display memory available
	- v: set debug level to verbose
	- d: set debug level to debug
	- i: set debug level to info
	- w: set debug level to warning
	- e: set debug level to errors
	- s: set debug silence on/off
	- cpu80 : ESP8266 CPU at 80MHz
	- cpu160: ESP8266 CPU at 160MHz
	- reset: reset the ESP8266
	- vars: Dump standard variables
	- user: Dump user variables
	- debug: Toggle debug flag
	- trace: Toggle trace flag

## Available Web pages (in FF_WebServer library folder)

- / and /index.htm -> index root file
- /admin.html -> administration menu
- /general.html -> general settings (device name)
- /ntp.html -> ntp settings
- /system.html -> system configuration (file system edit, HTTP OTA update, reboot, authentication parameters)
- /config.html -> network settings
- /user.html -> user specific settings

## Interactions with web server

Three root URLs are defined to communicate with web server, namely /rest, /json and /post. /rest is used to pass/return information in an unstructured way, while /json uses JSON message format. They both are using GET HTTP format, while /post is using HTTP POST format.

## Available URLs

WebServer answers to the following URLs. If authentication is turned on, some of them needs user to be authenticated before being served.

### Internal URLs
- /list?dir=/ -> list file system content
- /edit -> load editor (GET) , create file (PUT), delete file (DELETE)
- /admin/generalvalues -> return deviceName and userVersion in json format
- /admin/values -> return values to be loaded in index.html and indexuser.html
- /admin/connectionstate -> return connection state
- /admin/infovalues -> return network info data
- /admin/ntpvalues -> return ntp data
- /scan -> return wifi network scan data
- /admin/restart -> restart ESP
- /admin/wwwauth -> ask for authentication
- /admin -> return admin.html contents
- /update/updatepossible
- /setmd5 -> set MD5 OTA file value
- /update -> update system with OTA file
- /rconfig (GET) -> get configuration data
- /pconfig (POST) -> set configuration data
- /rest -> activate a rest request to get some (user's) values (*)
- /rest/values -> return values to be loaded in index.html and index_user.html (*)
- /json -> activate a rest request to get some (user's) values (*)
- /post -> activate a post request to set some (user's) values (*)

### HTML files (in addition to available web pages described before)
- /index_user.html -> user specific part of index.html (*)
- /favicon.ico -> icon to be displayed by browser

### JSON files
- /config.json -> system configuration data (!)
- /userconfig.json -> user's configuration data (!)
- /userconfigui.json -> user's specific configuration UI data (*)

### JavaScript files
- /microajax.js -> ajax minimalistic routines
- /browser.js -> FF_WebServer browser side JS functions
- /spark-md5.js -> md5 calculation function

### CSS files
- style.css -> CSS style file

(*) Should be adapted by user to specific configuration
(!) Could be adapted by user to initially set configuration values (i.e. server's ip or port, SSID...)
