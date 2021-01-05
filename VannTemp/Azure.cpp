#include "Azure.h"

AzureClass::AzureClass()
{
	messageCount = 1;
}
void AzureClass::Disconnect()
{
	Esp32MQTTClient_Close();
	if (hasWifi)
		Serial.println("has WiFi");
	if (WiFi.disconnect(true))
		Serial.println("WiFi disconnected");
	else
		Serial.println("Fail to disconnect WiFi");
}
void AzureClass::Reconnect()
{
	if (WiFi.reconnect())
		Serial.println("WiFi Reconnected");
	else
		Serial.println("Fail to reconnect WiFi");
}

void AzureClass::Setup()
{
	Serial.println("Initializing Azure...");
	// Initialize the WiFi module
	Serial.println(" > WiFi");
	hasWifi = false;
	InitWifi();
	if (!hasWifi)
	{
		return;
	}

	Serial.println(" > IoT Hub");
	Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "TTGO-T5-V2-3-2.13-TermoSleep");
	//Esp32MQTTClient_Init((const uint8_t*)connectionString, true);

	Esp32MQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
	Esp32MQTTClient_SetMessageCallback(MessageCallback);
	Esp32MQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
	Esp32MQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);

	Esp32MQTTClient_Init((const uint8_t*)connectionString, true);

	send_interval_ms = millis() - INTERVAL;
}

void AzureClass::SendVVB()
{
	if (hasWifi)
	{
		if (messageSending &&
			(int)(millis() - send_interval_ms) >= INTERVAL)
		{
			Serial.println("-- Sending to Azure --");
			// Send teperature data
			char messagePayload[MESSAGE_MAX_LEN];
			//float temperature = (float)random(0, 50);
			//float humidity = (float)random(0, 1000) / 10;
			//*messageData = "{\"deviceId\":\"%s\", \"messageId\":%d, \"vvbmode\":%d, \"vvbon\":%d, \"vvbset\":%d, \"t1\":%f, \"t2\":%f}";
			snprintf(messagePayload, MESSAGE_MAX_LEN, messageDataVVB, DEVICE_ID, 22,
				vvbMode, lastCh3, vvbTempSet, temperature21, temperature22);
			Serial.println(messagePayload);
			EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
			//Esp32MQTTClient_Event_AddProp(message, "temperatureAlert", "true");
			Esp32MQTTClient_SendEventInstance(message);

			send_interval_ms = millis();
		}
		else
		{
			Esp32MQTTClient_Check();
		}
	}
	delay(200);
}
void AzureClass::SendVK()
{
  if (hasWifi)
  {
    if (messageSending) //&& (int)(millis() - send_interval_ms) >= INTERVAL)
    {
      Serial.println("-- Sending VK to Azure --");
      // Send teperature data
      char messagePayload[MESSAGE_MAX_LEN];
      //*messageData = "{\"deviceId\":\"%s\", \"messageId\":%d, \"vvbmode\":%d, \"vvbon\":%d, \"vvbset\":%d, \"t1\":%f, \"t2\":%f}";
      snprintf(messagePayload, MESSAGE_MAX_LEN, messageDataVK, DEVICE_ID, 23,
        kabelMode, lastCh2, kabelTempSet, temperature11, temperature12, temperature13, temperature14, temperature15);
      Serial.println(messagePayload);
      EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
      //Esp32MQTTClient_Event_AddProp(message, "temperatureAlert", "true");
      Esp32MQTTClient_SendEventInstance(message);

      send_interval_ms = millis();
    }
    else
    {
      Esp32MQTTClient_Check();
    }
  }
  delay(200);
}
void AzureClass::Check()
{
	if (hasWifi)
	{
		Esp32MQTTClient_Check();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities
void AzureClass::InitWifi()
{
	if (WiFi.status() == WL_CONNECTED)
	{
		Serial.println("WiFi already connectied.");
		hasWifi = true;
		return;
	}

  Serial.printf("Connecting to %s ", wifinet[wifiNum]);
  WiFi.begin(wifinet[wifiNum], wifipwd[wifiNum]);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    i++;
    if (i == 20)
    {
      if (++wifiNum > 1) wifiNum = 0;
      Serial.printf("\nConnecting to %s ", wifinet[wifiNum]);
      WiFi.begin(wifinet[wifiNum], wifipwd[wifiNum]);
    }
    else if (i > 60)
    {
      Serial.println("Can't connect.");
      Serial.println("No WiFi");
      hasWifi = false;
      return;
//      Serial.flush();
//      ESP.restart();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.printf("RSSI %d Ch.%d IP ", WiFi.RSSI(), WiFi.channel());
  Serial.println(WiFi.localIP());
   
  hasWifi=true;



//	for (int i = 0; i < 3; i++)
//	{
//		Serial.println("Connecting Wifi...");
//		WiFi.begin(ssid, password);
//		for (int j = 0; j < 120; j++)
//		{
//			if (WiFi.status() == WL_CONNECTED) {
//				hasWifi = true;
//				Serial.println("");
//				Serial.println("WiFi connected");
//				Serial.println("IP address: ");
//				Serial.println(WiFi.localIP());
//				return;
//			}
//			delay(500);
//			Serial.print(".");
//		}
//	}
//	hasWifi = false;
//	Serial.println("");
//	Serial.println("No WiFi");
}

void AzureClass::SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
	if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
	{
		Serial.println("Send Confirmation Callback finished.");
	}
	else
	{
		errorCount++;
		Serial.printf("Send failed. ERROR %d\n", result);
	}
}

void AzureClass::MessageCallback(const char* payLoad, int size)
{
	Serial.println("Message callback:");
	Serial.println(payLoad);

  DoCommand(payLoad);
}

void AzureClass::DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
	char *temp = (char *)malloc(size + 1);
	if (temp == NULL)
	{
		return;
	}
	memcpy(temp, payLoad, size);
	temp[size] = '\0';
	// Display Twin message.
	Serial.print("Twin message: ");
	Serial.println(temp);
	free(temp);
}

int  AzureClass::DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
	LogInfo("Try to invoke method %s", methodName);
	/*const*/ char *responseMessage = "\"Successfully invoke device method\"";
	int result = 200;

	if (strcmp(methodName, "start") == 0)
	{
		LogInfo("Start sending data");
		messageSending = true;
	}
	else if (strcmp(methodName, "stop") == 0)
	{
		LogInfo("Stop sending data");
		messageSending = false;
	}
  else if (strcmp(methodName, "status") == 0)
  {
    responseMessage = new char[280];
    memset(responseMessage, 0, 280);
    //sprintf(responseMessage, "\"Status: Err=%d Sent=%d Mode=%d Last=%fA Avg1h=%fA Consumed=%fAh\"", 
    //  errorCount, messageCount, azureMode,
    //  ampBuff.LastValue(), ampBuff.Avg(3600000ul), ampBuff.TotalAh());

    //{"Errors":0, "Sent" : 2, "Mode" : 2, "Amp" : -4.2, "Avg1h" : -4.266617, "ConsumedAh" : -4.287439, "b1volt" : 13.2, "b2volt" : 13.4, "RSSI" : "-55"}
    sprintf(responseMessage, "{\"VVBmode\":%d, \"Setpunkt\":%d,\"On\":%d,\"T1\":%f,\"T2\":%f,\"RSSI\":\"%d\"}",
      vvbMode, vvbTempSet, lastCh3,
      temperature21,temperature22, WiFi.RSSI()
    );

    //sprintf(responseMessage, "\"Status: Err=%d Sent:%d Mode:%d\"", errorCount, messageCount, azureMode);
    LogInfo("Requested status");
  }
  else if (strcmp(methodName, "statusvk") == 0)
  {
    responseMessage = new char[280];
    memset(responseMessage, 0, 280);
    sprintf(responseMessage, "{\"KabelMode\":%d, \"Setpunkt\":%d,\"On\":%d,\"T1\":%f,\"T2\":%f,\"T3\":%f,\"T4\":%f,\"T5\":%f}",
      kabelMode, kabelTempSet, lastCh2,
      temperature11,temperature12, temperature13,temperature14, temperature15
    );
    LogInfo("Requested status");
  }
	//else if (strcmp(methodName, "time2") == 0)
	//{
	//	ulong time = 0;
	//	char *expectedPayload = new char[80];
	//	memset(expectedPayload, 0, 80);
	//	if (memcpy(expectedPayload, payload, 79) > 0)
	//	{
	//		time = atol(expectedPayload);
	//	}
	//	if (time > 50000ul && time < 86400000ul)
	//	{
	//		LogInfo("Set mode 2 timer %u ms", time);
	//		timeMode2 = time;
	//	}
	//	else
	//	{
	//		LogInfo("Time %u not accepted. '%s'", time, expectedPayload);
	//		responseMessage = "\"Not accepted argument\"";
	//		result = 404;
	//	}
	//}
	//else if (strcmp(methodName, "time3") == 0)
	//{
	//	ulong time = 0;
	//	char *expectedPayload = new char[80];
	//	memset(expectedPayload, 0, 80);
	//	if (memcpy(expectedPayload, payload, 79) > 0)
	//	{
	//		time = atol(expectedPayload);
	//	}
	//	if (time > 50000ul && time < 86400000ul)
	//	{
	//		LogInfo("Set mode 3 timer %u", time);
	//		timeMode3 = time;
	//	}
	//	else
	//	{
	//		LogInfo("Time %u not accepted. '%s'", time, expectedPayload);
	//		responseMessage = "\"Not accepted argument\"";
	//		result = 404;
	//	}
	//}
	else
	{
		LogInfo("No method %s found", methodName);
		responseMessage = "\"No method found\"";
		result = 404;
	}

	*response_size = strlen(responseMessage) + 1;
	*response = (unsigned char *)strdup(responseMessage);

	return result;
}

extern AzureClass Azure;

