// Azure.h

#ifndef _AZURE_h
#define _AZURE_h

//#if defined(ARDUINO) && ARDUINO >= 100
//	#include "Arduino.h"
//#else
//	#include "WProgram.h"
//#endif

// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 

#include <WiFi.h>
#include <hexpwd.h>
#include "AzureIotHub.h"
#include "Esp32MQTTClient.h"

#define INTERVAL 10000


#define DEVICE_ID "RC2" // på båten   "T5_n5" is now used by 2.9 temp BLE
#define CONNECTION_STRING_AZURE CONNECTION_STRING_AZURE_rc2
// #define DEVICE_ID "T5_n4"  // org. Fjellro router control
// #define CONNECTION_STRING_AZURE CONNECTION_STRING_AZURE_n4



#define MESSAGE_MAX_LEN 256

extern int azureMode;
extern int azureSendInterval; //6*10min
extern int azureSendCounter;

// extern void DoCommand(const char *cmd);
extern String cmdstring;

//extern ulong timeMode2;    // Min interval in mode 2, average. Default one hour
//extern ulong timeMode3;    // Min interval in mode 3
//extern ulong lastSendTimeAzure;        // time of last packet send
extern bool rebootRequest;
extern float temperature1;
extern float temperature2;
extern float temperature3;
extern float temperature4;
extern int wifiFailCount;
extern int wifiRestarts;
extern float tmin1;
extern float tmax1;
extern float tmin2;
extern float tmax2;


static bool hasWifi = false;
static bool messageSending = true;
static uint64_t send_interval_ms;
static int messageCount;
static int errorCount;

class AzureClass
{
public:
	AzureClass();
	static void Setup();
	static void Send();
	static void Check();
	static void Disconnect();
	static void Reconnect();
	static void InitWifi();
	static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result);
	static void MessageCallback(const char* payLoad, int size);
	static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size);
	static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size);

private:
	// Please input the SSID and password of WiFi
	static constexpr char* ssid = HEX_WIFI_IDM;
	static constexpr char* password = HEX_WIFI_passwordM;


	/*String containing Hostname, Device Id & Device Key in the format:                         */
	/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
	/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */

	static constexpr char* connectionString = CONNECTION_STRING_AZURE;

	static constexpr char *messageData = "{\"deviceId\":\"%s\", \"messageId\":%d, \"t1\":%f, \"t2\":%f, \"t3\":%f, \"t4\":%f, \"actuate\":%d}";

	/*

protected:

	int messageCount = 1;
	bool messageSending = true;
	uint64_t send_interval_ms;
	*/

};
extern AzureClass Azure;
#endif

