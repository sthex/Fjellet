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

#include <hexpwd.h>
#include <WiFi.h>
#include "AzureIotHub.h"
#include "Esp32MQTTClient.h"

#define INTERVAL 10000

#define DEVICE_ID "VVS1"
#define CONNECTION_STRING_AZURE CONNECTION_STRING_AZURE_vvs1

#define MESSAGE_MAX_LEN 256


static const char *wifinet[] = {
  HEX_WIFI_IDM,
  HEX_WIFI_IDG
};
static const char *wifipwd[] = {
  HEX_WIFI_passwordM,
  HEX_WIFI_passwordG
};
static RTC_DATA_ATTR int wifiNum = 0;


static int azureMode;
static int azureSendInterval; //6*10min
static int azureSendCounter;

extern void DoCommand(const char *cmd);

//extern ulong timeMode2;    // Min interval in mode 2, average. Default one hour
//extern ulong timeMode3;    // Min interval in mode 3
//extern ulong lastSendTimeAzure;        // time of last packet send
//bool rebootRequest;
extern float temperature11;
extern float temperature12;
extern float temperature13;
extern float temperature14;
extern float temperature15;

extern float temperature21;
extern float temperature22;
extern int lastCh1; //Aux
extern int lastCh2; //Kabel
extern int lastCh3; //VVB
extern int vvbMode;    //0:off, 1:auto, 3:on
extern int kabelMode;  //0:off, 1:auto, 3:on
extern int vvbTempSet;  // Auto temperature 
extern int kabelTempSet; // Auto temperature
extern int sleepMinutes; // Minutes to sleep
extern int azureSendMinutes; // Minutes between send to Azure

//extern int wifiFailCount;
//extern int wifiRestarts;
//extern float tmin1;
//extern float tmax1;
//extern float tmin2;
//extern float tmax2;


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
  static void SendVVB();
  static void SendVK();
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

	//static constexpr char *messageData = "{\"deviceId\":\"%s\", \"messageId\":%d, \"t1\":%f, \"t2\":%f, \"t3\":%f, \"t4\":%f, \"t5\":%f}";
	static constexpr char *messageDataVVB = "{\"deviceId\":\"%s\", \"messageId\":%d, \"vvbmode\":%d, \"vvbon\":%d, \"vvbset\":%d, \"t1\":%f, \"t2\":%f}";
	static constexpr char *messageDataVK = "{\"deviceId\":\"%s\", \"messageId\":%d, \"vkmode\":%d, \"vkon\":%d, \"vkset\":%d, \"t1\":%f, \"t2\":%f, \"t3\":%f, \"t4\":%f, \"t5\":%f}";

	/*

protected:

	int messageCount = 1;
	bool messageSending = true;
	uint64_t send_interval_ms;
	*/

};
extern AzureClass Azure;
#endif

