//#include <hexpwd.h>
#include <esp_attr.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Azure.h"
#include "myTime.h"

// IO def
#define IO_POW3ON 16
#define IO_POW3OFF 17
#define IO_POW1OFF 18
#define IO_POW1ON 19
#define IO_POW2ON 21
#define IO_POW2OFF 22
#define IO_ONEWIRE1 23
#define IO_ONEWIRE2 26

#define MODE_OFF 0
#define MODE_ON 1
#define MODE_AUTO 2
#define MODE_TIMER 3
#define ONE_MINUTE 60000ul		// 1 Minutes
#define HALF_HOUR 1800000ul		// 1/2 hour
#define ONE_HOUR 3600000ul		// 1 hour
#define SIX_HOUR 21600000ul		// 6 hour
#define HOUR12 43200000ul		// 12 hour
#define uS_TO_M_FACTOR 60000000 /* Conversion factor for micro seconds to minutes */

#define MAX_INTERVAL_AZURE_ACTIVE 21600000ul // Max interval between sends, 6 hour
#define MAX_INTERVAL_AZURE_AVERAGE 3600000ul // Max interval between sends, 1 hour
ulong lastSendTimeAzure = 0;				 // time of last packet send

OneWire oneWire1(IO_ONEWIRE1);
DallasTemperature sensorsVVB(&oneWire1);
OneWire oneWire2(IO_ONEWIRE2);
DallasTemperature sensorsVK(&oneWire2);

RTC_DATA_ATTR int sleepMinutes = 4;			// Minutes of sleep = 4  ***********************************************************
RTC_DATA_ATTR int azureSendMinutes = 59;	// Minutes between send to Azure =59
RTC_DATA_ATTR int azureSendMinutesVK = 170; // Minutes between send to Azure =170
RTC_DATA_ATTR int vvbMode = MODE_OFF;		//0:off, 1:auto, 3:on
RTC_DATA_ATTR int kabelMode = MODE_AUTO;	//0:off, 1:auto, 3:on
RTC_DATA_ATTR int vvbTempSet = 10;			// Auto temperature
RTC_DATA_ATTR int kabelTempSet = 8;			// Auto temperature about 50% on
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int lastCh1 = 9;			  //Aux 0=off  1=on 9=ukjent fra start
RTC_DATA_ATTR int lastCh2 = 9;			  //Kabel
RTC_DATA_ATTR int lastCh3 = 9;			  //VVB
RTC_DATA_ATTR bool pulseAlways = false;	  //Send ouput on every check in auto
RTC_DATA_ATTR int ignoreSensorBit = 0x01; // ignore sensor 1 (i jorden under inntak) siden den er veldig treg/dempet
RTC_DATA_ATTR int inhibitSensorBit = 0;
RTC_DATA_ATTR float deadband = 1.0f;

RTC_DATA_ATTR short mytimer[]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
RTC_DATA_ATTR short mytimerLastHourDone = -1;

// DeviceAddress TVK0 = {0x28, 0xb0, 0xb7, 0x79, 0xa2, 0x00, 0x03, 0x67}; // kort gummikabel
// DeviceAddress TVK1 = {0x28, 0x78, 0xe3, 0x79, 0xa2, 0x00, 0x03, 0xd3}; // tykk jordkabel
// DeviceAddress TVK2 = {0x28, 0xf1, 0x80, 0x79, 0xa2, 0x00, 0x03, 0xf0}; // yttre på lang gummikabel
// DeviceAddress TVK3 = {0x28, 0xf5, 0xae, 0x79, 0xa2, 0x00, 0x03, 0x76}; // indre på lang gummikabel. 0.5 over de andre?

// DeviceAddress TG0 = {0x28, 0x83, 0x7a, 0x79, 0xa2, 0x00, 0x03, 0x1d}; // lang gummikabel tip. -127død?
// DeviceAddress TG1 = {0x28, 0xcf, 0x9a, 0x79, 0xa2, 0x00, 0x03, 0xcf}; // lang gummikabel 50cm fra tip. -127død?

DeviceAddress TVK10 = {0x28, 0x54, 0x15, 0x79, 0xa2, 0x00, 0x03, 0x22}; // ny gummi VK lang arm. under rør inn
DeviceAddress TVK11 = {0x28, 0xf2, 0x5e, 0x79, 0xa2, 0x00, 0x03, 0x55}; // ny gummi VK kort arm1 mrk. 90 gr bronse bend
DeviceAddress TVK12 = {0x28, 0xf5, 0x41, 0x79, 0xa2, 0x00, 0x03, 0xc3}; // ny gummi VK kort arm2.

float temperature11 = -99.0;
float temperature12 = -99.0;
float temperature13 = -99.0;
float temperature14 = -99.0;
float temperature15 = -99.0;

float temperature21 = -99.0;
float temperature22 = -99.0;
bool autoActionDone = false;
ulong idleSince;
ulong stayOnlineSec = 10;

void PulsIo(int io)
{
	Serial.printf("Pulse out pin %d. ", io);
	digitalWrite(io, LOW);
	delay(500);
	digitalWrite(io, HIGH);
	delay(1000);
	switch (io)
	{
	case IO_POW3ON:
		Serial.println("VVB på.");
		lastCh3 = 1;
		break;
	case IO_POW3OFF:
		Serial.println("VVB av.");
		lastCh3 = 0;
		break;
	case IO_POW2ON:
		Serial.println("Kabel på.");
		lastCh2 = 1;
		break;
	case IO_POW2OFF:
		Serial.println("Kabel av.");
		lastCh2 = 0;
		break;
	case IO_POW1ON:
		lastCh1 = 1;
		break;
	case IO_POW1OFF:
		lastCh1 = 0;
		break;
	}
}

void EmptySerial()
{
	while (Serial.available())
	{
		char ch = Serial.read();
	}
}

void DoCommand(const char *cmd)
{
	String cmdstring(cmd);
	int i = -1;
	
	if (cmdstring.indexOf("reboot") >= 0)
	{
		ESP.restart();
	}
	if (cmdstring.indexOf("cleartimers") >= 0)
	{
		Serial.printf("Clear all timers.\n");
		for (int i=0;i<24;i++)
			mytimer[i]=-1;
	}

	if (cmdstring.indexOf("ch1on") >= 0)
	{
		PulsIo(IO_POW1ON);
	}
	if (cmdstring.indexOf("ch1off") >= 0)
	{
		PulsIo(IO_POW1OFF);
	}
	if (cmdstring.indexOf("ch2on") >= 0)
	{
		PulsIo(IO_POW2ON);
	}
	if (cmdstring.indexOf("ch2off") >= 0)
	{
		PulsIo(IO_POW2OFF);
	}
	if (cmdstring.indexOf("ch3on") >= 0)
	{
		PulsIo(IO_POW3ON);
	}
	if (cmdstring.indexOf("ch3off") >= 0)
	{
		PulsIo(IO_POW3OFF);
	}

	if (cmdstring.indexOf("vvbauto") >= 0)
	{
		Serial.println("VV bereder AUTO.");
		vvbMode = MODE_AUTO;
	}

	i = cmdstring.indexOf("vvbon");
	if (i >= 0)
	{
		int hour=-1;
		if (cmdstring.length() >= i + 7 && cmdstring[i+5] >= '0' && cmdstring[i+5] <= '9')//bug? 7 was 8. VVB1 STILL 8
			hour = cmdstring.substring(i + 5, i + 7).toInt();

		if (hour >= 0 && hour < 24)
		{
			Serial.printf("Set VVB on kl. %d .\n", hour);
			mytimer[hour] = 31;
			vvbMode = MODE_TIMER;
		}
		else
		{
			Serial.println("VV bereder på.");
			vvbMode = MODE_ON;
			PulsIo(IO_POW3ON);
		}
	}

	i = cmdstring.indexOf("vvboff");
	if (i >= 0)
	{
		int hour=-1;
		if (cmdstring.length() >= i + 8 && cmdstring[i+6] >= '0' && cmdstring[i+6] <= '9')
			hour = cmdstring.substring(i + 6, i + 8).toInt();

		if (hour >= 0 && hour < 24)
		{
			Serial.printf("Set VVB off kl. %d .\n", hour);
			mytimer[hour] = 30;
			vvbMode = MODE_TIMER;
		}
		else
		{
			Serial.println("VV bereder av.");
			vvbMode = MODE_OFF;
			PulsIo(IO_POW3OFF);
		}
	}
	if (cmdstring.indexOf("vvbtimer") >= 0)
	{
		Serial.println("VV bereder Timer mode.");
		vvbMode = MODE_TIMER;
	}

	if (cmdstring.indexOf("vkauto") >= 0)
	{
		Serial.println("Kabel auto.");
		kabelMode = MODE_AUTO;
	}
	if (cmdstring.indexOf("vkon") >= 0)
	{
		Serial.println("Kabel på.");
		kabelMode = MODE_ON;
		PulsIo(IO_POW2ON);
	}
	if (cmdstring.indexOf("vkoff") >= 0)
	{
		Serial.println("Kabel av.");
		kabelMode = MODE_OFF;
		PulsIo(IO_POW2OFF);
	}

	if (cmdstring.indexOf("pulseonce") >= 0)
	{
		Serial.println("PulsAlways set to false");
		pulseAlways = true;
	}
	else if (cmdstring.indexOf("pulsealways") >= 0)
	{
		Serial.println("PulsAlways set to true");
		pulseAlways = true;
	}
	if (cmdstring.indexOf("stayonline") >= 0)
	{
		Serial.println("Stay online for two hour.");
		stayOnlineSec = 2 * 60 * 60;
	}
	else if (cmdstring.indexOf("gotosleep") >= 0)
	{
		Serial.println("Request sleep.");
		stayOnlineSec = 0;
	}

	i = cmdstring.indexOf("vvbtemp");
	if (i >= 0 && cmdstring.length() >= i + 9)
	{
		int xt = cmdstring.substring(i + 7, i + 9).toInt();
		Serial.printf("Set VVB temperatur på %d grader.\n", xt);
		if (xt > 0 && xt < 99)
			vvbTempSet = xt;
	}
	i = cmdstring.indexOf("vktemp");
	if (i >= 0 && cmdstring.length() >= i + 8)
	{
		int xt = cmdstring.substring(i + 6, i + 8).toInt();
		Serial.printf("Set kabel temperatur på %d grader.\n", xt);
		if (xt > 0 && xt < 99)
			kabelTempSet = xt;
	}

	i = cmdstring.indexOf("sleepminutes"); // Minutes to sleep
	if (i >= 0 && cmdstring.length() >= i + 14)
	{
		int xt = cmdstring.substring(i + 12, i + 14).toInt();
		Serial.printf("Set sleep minutter %d.\n", xt);
		if (xt >= 0 && xt < 99)
			sleepMinutes = xt;
	}
	i = cmdstring.indexOf("ignore"); // Ignore vk sensor value in auto
	if (i >= 0 && cmdstring.length() >= i + 8)
	{
		int xt = cmdstring.substring(i + 6, i + 8).toInt();
		Serial.printf("Ignore vk sensor bit 0x%x.\n", xt);
		if (xt >= 0x00 && xt < 0x20)
			ignoreSensorBit = xt; //eg. vktemp02 deadband02 ignore00, default vktemp02 deadband10 ignore01
	}
	i = cmdstring.indexOf("inhibit"); // Inhibit vk sensor bit
	if (i >= 0 && cmdstring.length() >= i + 9)
	{
		int xt = cmdstring.substring(i + 7, i + 9).toInt();
		Serial.printf("Inhibit vk sensor (don't read) bit 0x%x.\n", xt);
		if (xt >= 0x00 && xt < 0x20)
			inhibitSensorBit = xt;
	}
	i = cmdstring.indexOf("deadband"); // Deadband of vk temperatur regulator
	if (i >= 0 && cmdstring.length() >= i + 10)
	{
		int xt = cmdstring.substring(i + 8, i + 10).toInt();
		Serial.printf("Deadband of vk temperatur regulator %f.\n", xt / 10.0f);
		if (xt >= 1 && xt < 50)
			deadband = (float)xt / 10.f;
	}

	i = cmdstring.indexOf("azuresendminutesvk"); //NB 3 tall godtas 'azuresendminutes123'
	if (i >= 0 && cmdstring.length() >= i + 19)
	{
		int xt = cmdstring.substring(i + 16, i + 19).toInt();
		Serial.printf("Send VK to Azure hver %d minutt.\n", xt);
		if (xt > 0 && xt < 99)
			azureSendMinutesVK = xt;
	}
	else
	{
		i = cmdstring.indexOf("azuresendminutes"); //NB 3 tall godtas 'azuresendminutes123'
		if (i >= 0 && cmdstring.length() >= i + 19)
		{
			int xt = cmdstring.substring(i + 16, i + 19).toInt();
			Serial.printf("Send to Azure hver %d minutt.\n", xt);
			if (xt > 0 && xt < 99)
				azureSendMinutes = xt;
		}
	}

	// vvboff vvbtimer timer04310530

	i = cmdstring.indexOf("timer") + 5; // timer og time og cmd, "timer-10010311130"
	while (i >= 5 && cmdstring.length() >= i + 4)
	{
		int hour = cmdstring.substring(i + 0, i + 2).toInt();
		int icmd = cmdstring.substring(i + 2, i + 4).toInt();
		if (hour < 0)
		{
			Serial.printf("Clear all timers.\n");
			for (int i=0;i<24;i++)
				mytimer[i]=-1;
		}
		else  if (hour < 24 && icmd > 0 && icmd < 127 )
		{
			Serial.printf("Timer ved time %d kommando %d.\n", hour, icmd);
			mytimer[hour]=(short)icmd;
			if (icmd == 30 || icmd == 31 )
				vvbMode = MODE_TIMER;
		}
		i+=4;
	}

	idleSince = millis();
}
void SetTimers(char *txt)
{
	int len = strlen(txt);
	Serial.printf("SetTimers %s (len=%d).\n", txt, len);
	if (len>=4)
	{
		for (int i=0; i<len; i+=4)
		{
			int hour = 10* (*(txt+i)-'0') + (*(txt+i+1) -'0') ;
			int icmd = 10* (*(txt+i+2)-'0') + (*(txt+i+3) -'0');
			if (*(txt+i) == '-' || hour < 0 )
			{
				Serial.printf("Clear all timers.\n");
				for (int i=0;i<24;i++)
					mytimer[i]=-1;
			}
			else  if (hour < 24 && icmd > 0 && icmd < 127 )
			{
				Serial.printf("Timer ved time %d kommando %d.\n", hour, icmd);
				mytimer[hour]=(short)icmd;
				if (icmd == 30 || icmd == 31 )
					vvbMode = MODE_TIMER;
			}
		}
	}

}
void SetTimersS(String txt)
{
	Serial.printf("SetTimers %s.\n", txt.c_str());
	int i=0;
	
	while (txt.length() >= i + 4)
	{
		int hour = txt.substring(i + 0, i + 2).toInt();
		int icmd = txt.substring(i + 2, i + 4).toInt();
		if (hour < 0)
		{
			Serial.printf("Clear all timers.\n");
			for (int i=0;i<24;i++)
				mytimer[i]=-1;
		}
		else  if (hour < 24 && icmd > 0 && icmd < 127 )
		{
			Serial.printf("Timer ved time %d kommando %d.\n", hour, icmd);
			mytimer[hour]=(short)icmd;
			if (icmd == 30 || icmd == 31 )
				vvbMode = MODE_TIMER;
		}
		i+=4;
	}
}
void DoCommandI(int icmd)
{
	Serial.printf("DoCommandI %d \n", icmd);

	switch (icmd)
	{
		case 30:
			Serial.println("VV bereder av...");
			PulsIo(IO_POW3OFF);
			break;
		case 31:
			Serial.println("VV bereder på...");
			PulsIo(IO_POW3ON);
			break;
		// case 32:
		// 	DoCommand("vvbauto");
		// 	break;
		// case 21:
		// 	DoCommand("vkon");
		// 	break;
		// case 20:
		// 	DoCommand("vkoff");
		// 	break;
		// case 22:
		// 	DoCommand("vkauto");
		// 	break;
		// case 10:
		// 	DoCommand("ch1on");
		// 	break;
		// case 11:
		// 	DoCommand("ch1off");
		// 	break;
		// case 40:
		// 	DoCommand("stayonline");
		// 	break;
		// case 41:
		// 	DoCommand("gotosleep");
		// 	break;
		default:
			Serial.printf("DoCommandI %d NOT KNOWN!\n", icmd);
			break;
	}


	idleSince = millis();
}

void ReadTemperature()
{
	sensorsVK.requestTemperatures(); // Send the command to get temperatures
	//if (vkByIndex)
	//{
	if ((inhibitSensorBit & 0x01) == 0)
		temperature11 = sensorsVK.getTempC(TVK10);
	else
		temperature11 = -126;
	if ((inhibitSensorBit & 0x02) == 0)
		temperature12 = sensorsVK.getTempC(TVK11);
	else
		temperature12 = -126;
	if ((inhibitSensorBit & 0x04) == 0)
		temperature13 = sensorsVK.getTempC(TVK12);
	else
		temperature13 = -126;
	Serial.printf("Temperaturer kabel: %7.2f %7.2f %7.2f \n", temperature11, temperature12, temperature13);
	//}
	//else
	//{
	//	temperature11 = sensorsVK.getTempCByIndex(0);
	//	temperature12 = sensorsVK.getTempCByIndex(1);
	//	temperature13 = sensorsVK.getTempCByIndex(2);
	//	temperature14 = sensorsVK.getTempCByIndex(3);
	//	temperature15 = sensorsVK.getTempCByIndex(4);
	//  Serial.printf("Temperaturer1: %7.2f %7.2f %7.2f %7.2f %7.2f\n", temperature11, temperature12, temperature13, temperature14, temperature15);
	//}

	sensorsVVB.requestTemperatures(); // Send the command to get temperatures
	temperature21 = sensorsVVB.getTempCByIndex(0);
	temperature22 = sensorsVVB.getTempCByIndex(1);
	Serial.printf("Temperatur VVB: %7.2f %7.2f\n", temperature21, temperature22);
}

void DoAuto()
{
	static float minT1 = 999.0;
	static float minT2 = 999.0;

	if (kabelMode == MODE_AUTO)
	{
		if (temperature11 < minT1 && temperature11 < 85.0f && temperature11 > -15.0f && temperature11 != 25.0f && (ignoreSensorBit & 0x01) == 0)
			minT1 = temperature11;
		if (temperature12 < minT1 && temperature12 < 85.0f && temperature12 > -15.0f && temperature12 != 25.0f && (ignoreSensorBit & 0x02) == 0)
			minT1 = temperature12;
		if (temperature13 < minT1 && temperature13 < 85.0f && temperature13 > -15.0f && temperature13 != 25.0f && (ignoreSensorBit & 0x04) == 0)
			minT1 = temperature13;
		if (temperature14 < minT1 && temperature14 < 85.0f && temperature14 > -15.0f && temperature14 != 25.0f && (ignoreSensorBit & 0x08) == 0)
			minT1 = temperature14;
		if (temperature15 < minT1 && temperature15 < 85.0f && temperature15 > -15.0f && temperature15 != 25.0f && (ignoreSensorBit & 0x10) == 0)
			minT1 = temperature15;
		Serial.printf("Min Temperatur1 (kabel): %7.2f \n", minT1);
		if (minT1 < 998.0)
		{
			if (minT1 < (kabelTempSet - deadband))
			{
				if (lastCh2 != 1)
					autoActionDone = true;
				//if (lastCh2 != 1 || pulseAlways)
				PulsIo(IO_POW2ON);
			}
			else if (minT1 > (kabelTempSet + deadband))
			{
				if (lastCh2 != 0)
					autoActionDone = true;
				if (lastCh2 != 0 || pulseAlways || (temperature12 > 29.0 && temperature12 < 33.0))
					PulsIo(IO_POW2OFF);
			}
		}
		else
		{ // sensor fail
			if (lastCh2 != 1)
				autoActionDone = true;
			if (lastCh2 != 1 || pulseAlways)
				PulsIo(IO_POW2ON);
		}
	}
	else if (kabelMode == MODE_ON && lastCh2 == 9)
	{
		PulsIo(IO_POW2ON); //Viktig at varmekabel settes på etter strømbrudd
	}
	// else if (vkMode == MODE_TIMER){}												


	if (vvbMode == MODE_AUTO)												
	{
		if (temperature21 < minT2 && temperature21 < 85.0f && temperature21 > -5.0f)
			minT2 = temperature21;
		if (temperature22 < minT2 && temperature22 < 85.0f && temperature22 > -5.0f)
			minT2 = temperature22;
		Serial.printf("Min Temperatur2 (VVB): %7.2f \n", minT2);

		if (minT2 < 998.0)
		{
			if (minT2 < (vvbTempSet - 1.0f))
			{
				if (lastCh3 != 1)
					autoActionDone = true;
				if (lastCh3 != 1 || pulseAlways)
					PulsIo(IO_POW3ON);
			}
			else if (minT2 > (vvbTempSet + 1.0f))
			{
				if (lastCh3 != 0)
					autoActionDone = true;
				if (lastCh3 != 0 || pulseAlways)
					PulsIo(IO_POW3OFF);
			}
		}
	}
	else if (vvbMode == MODE_TIMER)												
	{
		int hour = timeHour();
		// int hour = timeMinute();
		// if (hour >= 30) hour -= 30;
	
		Serial.printf("Check timer for hour: %d \n", hour);
		if (hour != mytimerLastHourDone && hour>=0 && hour < 24)
		{
			mytimerLastHourDone = hour;
			if (mytimer[hour] > 0)
			{
				DoCommandI(mytimer[hour]);
				autoActionDone = true;
			}	
		}
	}
}

void GotoSleep()
{
	if (sleepMinutes > 0)
	{
		//esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, LOW);
		esp_sleep_enable_timer_wakeup(sleepMinutes * uS_TO_M_FACTOR);
		Serial.printf("Going to sleep for %d minutes", sleepMinutes);
		Serial.flush();
		delay(100);
		esp_deep_sleep_start();
	}
}

void SendStatus()
{
	Azure.SendVVB();
	for (int i = 0; i < 10; i++)
	{
		delay(400);
		Azure.Check();
	}
	Azure.SendVK();
}

void setup()
{
	Serial.begin(115200);
	//Serial.printf("Start setup - Fjellro VannTemp v2020.03.03.\n");
	//Serial.printf("Start setup - Fjellro VannTemp v2020.08.02.\n");// Default auto, 4 grader
	//Serial.printf("Start setup - Fjellro VannTemp v2020.09.02.\n");// Ny kabel. Ignore 25.0 grader
	//Serial.printf("Start setup - Fjellro VannTemp v2020.09.05.\n");// Ny komanndoer: ignore/inhibit
	//Serial.printf("Start setup - Fjellro VannTemp v2021.01.02.\n"); // vktemp 2 gr
	// Serial.printf("Start setup - Fjellro VannTemp v2021.02.27.\n"); // vktemp 8 gr, ny deadband, vkoff ved 30 grader
	Serial.printf("Start setup - Fjellro VannTemp v2022.02.00.\n"); // tmers

	pinMode(IO_POW1ON, OUTPUT);
	pinMode(IO_POW1OFF, OUTPUT);
	pinMode(IO_POW2ON, OUTPUT);
	pinMode(IO_POW2OFF, OUTPUT);
	pinMode(IO_POW3ON, OUTPUT);
	pinMode(IO_POW3OFF, OUTPUT);

	digitalWrite(IO_POW1ON, HIGH);
	digitalWrite(IO_POW1OFF, HIGH);
	digitalWrite(IO_POW2ON, HIGH);
	digitalWrite(IO_POW2OFF, HIGH);
	digitalWrite(IO_POW3ON, HIGH);
	digitalWrite(IO_POW3OFF, HIGH);

	Serial.printf("Init OneWire on pin %d and %d.\n", IO_ONEWIRE1, IO_ONEWIRE2);
	sensorsVVB.begin(); // temperature one wire
	sensorsVK.begin();	// temperature one wire
	ReadTemperature();
	DoAuto();

	// int hour=timeHour();
	// Serial.printf("Time : %d \n", hour);
    // printLocalTime();


	if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED)
	{
	}
	else if (!autoActionDone && ++bootCount * sleepMinutes < azureSendMinutes && sleepMinutes > 0)
	{
		GotoSleep();
	}
	bootCount = 0;
	autoActionDone = false; // reset. it will be sent in loop

	Serial.printf("Init Azure.\n");
	Azure.Setup();

    // GetTime();

	EmptySerial();
	Serial.printf("Setup done.\n");

	idleSince = millis();
}

// Add the main program code into the continuous loop() function
void loop()
{
	ulong t = millis();
	static ulong lastTimeTempRead = t;
	static bool sentStatusToAzureDone = false;

	if (WiFi.status() == WL_CONNECTED)
	{
		if (!sentStatusToAzureDone)
		{
			SendStatus();
			sentStatusToAzureDone = true;
		}
		Azure.Check();
	}

	if (Serial.available())
	{
		String buffer = Serial.readStringUntil(10); //13=CR 10=LF
		Serial.print("Got '");
		Serial.print(buffer);
		Serial.println(" from Serial0.");
		DoCommand(buffer.c_str());
	}

	if (t - lastTimeTempRead > sleepMinutes * 60000)
	{
		ReadTemperature();
		DoAuto();
		lastTimeTempRead = t;
		if (autoActionDone)
		{
			sentStatusToAzureDone = false;
			autoActionDone = false;
		}
	}

	if (t - idleSince > stayOnlineSec * 1000)
	{
		GotoSleep();
		idleSince = t; // In case sleep time is null
	}

	delay(1500);
	// int hour=timeHour();
	// Serial.printf("Time : %d \n", hour);
    // printLocalTime();
}
