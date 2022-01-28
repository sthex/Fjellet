//************************* pin *****************************************************************
#define OneWirePin 32 //Temperature sensors. 
#define RELAY1 27 	  // Router restart. 
#define RELAY2 22     //      19 is no go
#define RELAY3 26     //
#define RELAY4 25     // 
//***********************************************************************************************

//#define AZURE_OFF
//#define TEST

#include <OneWire.h>
#include <DallasTemperature.h>
#include <GxFont_GFX.h>
#include <hexpwd.h>
#include <GxEPD.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include "Azure.h"
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/Picopixel.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include "SPI.h"
#include "esp_wifi.h"
#include "Esp.h"
#include "board_def.h"

#define FONT_24 &FreeSansBold24pt7b
#define FONT_12 &FreeSansBold12pt7b
#define FONT_9 &FreeMono9pt7b

/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN) 
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)  

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define uS_TO_M_FACTOR 60000000  /* Conversion factor for micro seconds to minutes */
#define SLEEP_MINUTES  10        /* Time ESP32 will go to sleep (in minutes) */
#define ONE_MINUTE  60000ul        // 1 Minutes
#define AVG_SHORT  60000ul        // 1 Minutes
#define AVG_MEDIUM  600000ul        // 10 Minutes
#define AVG_LONG  1800000ul        // 1/2 hour
#define HALF_HOUR  1800000ul        // 1/2 hour
#define ONE_HOUR  3600000ul        // 1 hour
#define SIX_HOUR  21600000ul        // 6 hour
#define HOUR12 43200000ul        // 12 hour
#define HOUR24 86400000ul        // 24 hour
#define DAYS1  86400000ul        // 24 hour
#define DAYS2 17280000ul        // 24 hour
#define DAYS3 25920000ul        // 24 hour
#define WEEK1 60480000ul        // 24 hour
#define MODE_OFFLINE  0        // No wifi or Azure
#define MODE_ACTIVE  1         // Send each change to Azure
#define MODE_AVERAGE  2        // Send average data to Azure each hour
#define EMPTYBUFF -999        //
#define BUFFSIZE  750        // 1556 almost one week each 5 min. Max 4096 byte in RTC
//#define BUFFSIZE  1556        // 2016 = one temp each 5 min for one week. Max 4096 byte in RTC

ulong timeMode2 = ONE_HOUR;    // Report interval in average mode (2), default 1 hour
ulong timeMode3 = 600000ul;    // Min interval in mode 3, default 10 min


#define MAX_INTERVAL_AZURE_ACTIVE 21600000ul        // Max interval between sends, 6 hour
//#define MAX_INTERVAL_AZURE_AVERAGE 3600000ul        // Max interval between sends, 1 hour
ulong lastSendTimeAzure = 0;        // time of last packet send
ulong lastPartialDisplayUpdate = 0;
ulong idleSince;
ulong lastTimeDisplayUpdate = 0;
ulong lastUpdate1 = 0;
ulong lastUpdate2 = 0;
AzureClass Azure;
String azureMsg;

bool rebootRequest = false;
const char *wifinet[] = {
	HEX_WIFI_IDM,
	HEX_WIFI_IDX
};
const char *wifipwd[] = {
	HEX_WIFI_passwordM,
	HEX_WIFI_passwordX
};
#define WIFI_COUNT 2  // Size of wifi array

//RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR ulong stayOnlineMinutes = 0;
RTC_DATA_ATTR int azureMode = MODE_ACTIVE;
RTC_DATA_ATTR int azureSendInterval = 6; //6*10min
RTC_DATA_ATTR int azureSendCounter = 4; // 0-azureSendInterval
RTC_DATA_ATTR int wifiFailCount = 0;
RTC_DATA_ATTR int wifiRestarts = 0;
RTC_DATA_ATTR int lastDisplay = 0;
RTC_DATA_ATTR int buffIndex1 = 0;
RTC_DATA_ATTR int16_t buff1[BUFFSIZE]; //2016 = one temp each 5 min for one week. Max 4096 byte in RTC
RTC_DATA_ATTR int buffIndex2 = 0;
RTC_DATA_ATTR int16_t buff2[BUFFSIZE]; //2016 = one temp each 5 min for one week. Max 4096 byte in RTC
RTC_DATA_ATTR int sensCount;  // number of one wire sensors


OneWire oneWire(OneWirePin);
DallasTemperature sensors(&oneWire);

float temperature1 = -99.0;
float temperature2 = -99.0;
float temperature3 = -99.0;
float temperature4 = -99.0;

float tmin1 = 0.0;
float tmax1 = 0.0;
float tmin2 = 0.0;
float tmax2 = 0.0;

typedef enum {
	RIGHT_ALIGNMENT = 0,
	LEFT_ALIGNMENT,
	CENTER_ALIGNMENT,
} Text_alignment;

struct tm timeinfo;

GxIO_Class io(SPI, ELINK_SS, ELINK_DC, ELINK_RESET);
GxEPD_Class display(io, ELINK_RESET, ELINK_BUSY);


#pragma region Display

void displayText(const String &str, int16_t y, uint8_t alignment, int16_t xOffset)
{
	int16_t x = 0;
	int16_t x1, y1;
	uint16_t w, h;
	display.setCursor(x, y);
	display.getTextBounds(str, x, y, &x1, &y1, &w, &h);

	switch (alignment) {
	case RIGHT_ALIGNMENT:
		display.setCursor(display.width() - w - x1 + xOffset, y);
		break;
	case LEFT_ALIGNMENT:
		display.setCursor(xOffset, y);
		break;
	case CENTER_ALIGNMENT:
		display.setCursor(display.width() / 2 - ((w + x1) / 2) + xOffset, y);
		break;
	default:
		break;
	}
	display.println(str);
}
void displayTextAt2(int16_t x, int16_t y, const String &str, const String &unit)
{
	int16_t x1, y1;
	uint16_t w, h;
	display.setCursor(x, y);
	display.getTextBounds(str, x, y, &x1, &y1, &w, &h);

	display.setCursor(x, y);
	display.println(str);
	display.setFont(FONT_12);
	display.setCursor(x + w + 6, y);
	display.println(unit);
}
void displayTextAt(int16_t x, int16_t y, const String &str)
{
	display.setCursor(x, y);
	display.println(str);
}

const unsigned char gImage_radio16[32] = { /* 0X00,0X01,0X10,0X00,0X10,0X00, */
0X00,0X00,0X00,0X00,0X00,0X00,0X10,0X08,0X24,0X24,0X29,0X94,0X4B,0XD2,0X4B,0XD2,
0X29,0X94,0X25,0XA4,0X11,0X88,0X01,0X80,0X01,0X80,0X01,0X80,0X01,0X80,0X03,0XC0,
};
const unsigned char gImage_radioLow16[32] = { /* 0X00,0X01,0X10,0X00,0X10,0X00, */
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X21,0X84,0X2B,0XD4,0X2B,0XD4,
0X21,0X84,0X01,0X80,0X01,0X80,0X01,0X80,0X01,0X80,0X01,0X80,0X01,0X80,0X03,0XC0,
};


void showMainPage(void)
{
	displayInit();
	ulong t = millis();

	display.fillScreen(GxEPD_WHITE);

	if (stayOnlineMinutes > 0)
		display.drawBitmap(115, 0, gImage_radio16, 16, 16, GxEPD_BLACK);
	else if (azureMode == MODE_ACTIVE)
		display.drawBitmap(115, 0, gImage_radioLow16, 16, 16, GxEPD_BLACK);
	else if (azureMode == 3)
	{
		display.drawBitmap(115, 0, gImage_radioLow16, 16, 16, GxEPD_BLACK);
		displayTextAt(114, 16, ".");
	}
	plot();
	//plot2();



	display.setFont(FONT_24);
	displayText(String(temperature2, 1), 37, RIGHT_ALIGNMENT, -18);
	displayTextAt2(0, 37, String(temperature1, 1), "C");
	//display.setFont(FONT_12);
	displayText("C", 37, RIGHT_ALIGNMENT, 0);

	display.update();
	//display.setRotation(1);
	//display.update();

	int ii = display.getWriteError();
	Serial.printf("getWriteError =%d\n", ii);


	lastDisplay = 1;
	lastTimeDisplayUpdate = lastPartialDisplayUpdate = millis();
}

void plot()
{
	//if (buff1[buffIndex1] != EMPTYBUFF)
	{
		const int minX = 10;
		const int maxX = display.width() - 5;
		const int minY = 42;
		const int maxY = display.height(); //max 128
		int min1 = 999, max1 = -999, min2 = 999, max2 = -999;
		int num = 0;
		for (int i = buffIndex1 - 1; num < BUFFSIZE; i--)
		{
			if (i < 0) i = BUFFSIZE - 1;
			if (buff1[i] == EMPTYBUFF) break;
			if (buff1[i] < min1) min1 = buff1[i];
			if (buff1[i] > max1) max1 = buff1[i];
			if (buff2[i] == EMPTYBUFF) break;
			if (buff2[i] < min2) min2 = buff2[i];
			if (buff2[i] > max2) max2 = buff2[i];
			++num;
		}
		tmin1 = min1 / 10.0;
		tmax1 = max1 / 10.0;
		tmin2 = min2 / 10.0;
		tmax2 = max2 / 10.0;
		Serial.printf("1: index=%d, num=%d\n", buffIndex1, num);
		if (num < 3) return;

		//display.setFont(FONT_9);
		//displayTextAt(0, 30, "Min:" + String(min1 / 10.0, 1) + "C");
		//displayTextAt(0, 15, "Max:" + String(max1 / 10.0, 1) + "C");
		display.setFont(&Picopixel);
		double span = SLEEP_MINUTES * num;
		double dx = (double)(maxX - minX) / (double)span;
#pragma region hour grid
		int time = 1;
		int pxTime = 60 * dx; // one hour
		for (int i = maxX - pxTime; i > minX; i -= pxTime)
		{
			if (time % 24 == 0)
			{
				display.setCursor(i + 2, minY + 6);
				display.printf("%d", time / 24);
				display.drawFastVLine(i, minY, maxY - minY, GxEPD_BLACK);
			}
			else if (time % 12 == 0)
			{
				for (int y = minY; y < maxY; y += 2)
					display.drawPixel(i, y, GxEPD_BLACK);
			}
			else if (time % 6 == 0)
				display.drawFastVLine(i, maxY - 8, 8, GxEPD_BLACK);
			else if (pxTime > 1)
				display.drawFastVLine(i, maxY - 4, 4, GxEPD_BLACK);
			time++;
		}

#pragma endregion
		//min1 = min1 - 10;
		//max1 = max1 + 10;
		//min2 = min2 - 10;
		//max2 = max2 + 10;
		if (max1 + 40 < min2)
		{
			int mid = minY + (float)(maxY - minY)*(float)(max1 - min1) / (float)(max1 - min1 + max2 - min2);
			plotT(buff2, minY, mid - 2, min2, max2, num, buffIndex2);
			plotT(buff1, mid + 2, maxY, min1, max1, num, buffIndex1);
			display.drawLine(minX - 2, mid - 1, minX + 2, mid + 1, GxEPD_BLACK);
		}
		else if (max2 + 40 < min1)
		{
			int mid = minY + (float)(maxY - minY)*(float)(max1 - min1) / (float)(max1 - min1 + max2 - min2);
			plotT(buff1, minY, mid - 2, min1, max1, num, buffIndex1);
			plotT(buff2, mid + 2, maxY, min2, max2, num, buffIndex2);
			display.drawLine(minX - 2, mid - 1, minX + 2, mid + 1, GxEPD_BLACK);
		}
		else
		{
			int minT = -10 + ((min1 < min2) ? min1 : min2);
			int maxT = 10 + ((max1 > max2) ? max1 : max2);

			double dy1 = (double)(maxY - minY) / (double)(max1 - min1);
			double dy2 = (double)(maxY - minY) / (double)(max2 - min2);
			double dy = (double)(maxY - minY) / (double)(maxT - minT);
			display.setFont(&Picopixel);
			Serial.printf("min1 =%d max1 =%d min1 =%d max2 =%d \n", min1, max1, min2, max2);
			Serial.printf("minT =%d maxT =%d dy =%f\n", minT, maxT, dy);

#pragma region Temerature grid

			display.drawFastVLine(minX - 1, minY, maxY - minY, GxEPD_BLACK); // y axis past
			display.drawFastVLine(maxX - 1, minY, maxY - minY, GxEPD_BLACK); // y axis now
			//display.drawFastHLine(minX - 2, (int)(maxY + minA * dy), 4, GxEPD_BLACK); // x axis
			for (int tens = -300; tens < maxT; tens += 100)
			{
				if (tens > minT && tens < maxT)
				{
					int y = (int)(maxY - (tens - minT) * dy);
					for (int x = minX - 1; x < maxX; x += 2)
						display.drawPixel(x, y, GxEPD_BLACK); // x axis
					display.setCursor(minX - 10, y + 2);
					display.printf("%02d", (int)(tens / 10));
				}
			}
			for (int tens = -250; tens < maxT; tens += 100)
			{
				if (tens > minT && tens < maxT)
				{
					int y = (int)(maxY - (tens - minT) * dy);
					for (int x = minX - 1; x < maxX; x += 4)
						display.drawPixel(x, y, GxEPD_BLACK); // x axis
					display.setCursor(minX - 10, y + 2);
					display.printf("%02d", (int)(tens / 10));
				}
			}
			if (maxT - minT < 80)
				for (int tens = -300; tens < maxT; tens += 10)
				{
					if (tens > minT && tens < maxT)
					{
						int y = (int)(maxY - (tens - minT) * dy);
						for (int x = minX - 1; x < maxX; x += 8)
							display.drawPixel(x, y, GxEPD_BLACK); // x axis
					}
				}
#pragma endregion

			int i = 0;
			for (int i1 = buffIndex1 - 1; i++ < num; i1--)
			{
				if (i1 < 0) i1 = BUFFSIZE - 1;
				int i2 = i1 - 1;
				if (i2 < 0) i2 = BUFFSIZE - 1;
				if (buff1[i1] == EMPTYBUFF) break;
				if (buff1[i2] == EMPTYBUFF) break;

				display.drawLine(
					(int)(maxX - (double)(SLEEP_MINUTES* (i - 1)) * dx),
					(int)(maxY - (buff1[i1] - minT) * dy),
					(int)(maxX - (double)(SLEEP_MINUTES* i) * dx),
					(int)(maxY - (buff1[i2] - minT)* dy),
					GxEPD_BLACK);
			}
			i = 0;
			for (int i1 = buffIndex2 - 1; i++ < num; i1--)
			{
				if (i1 < 0) i1 = BUFFSIZE - 1;
				int i2 = i1 - 1;
				if (i2 < 0) i2 = BUFFSIZE - 1;
				if (buff2[i1] == EMPTYBUFF) break;
				if (buff2[i2] == EMPTYBUFF) break;

				display.drawLine(
					(int)(maxX - (double)(SLEEP_MINUTES* (i - 1)) * dx),
					(int)(maxY - (buff2[i1] - minT) * dy),
					(int)(maxX - (double)(SLEEP_MINUTES* i) * dx),
					(int)(maxY - (buff2[i2] - minT)* dy),
					GxEPD_BLACK);
			}
		}
	}
}

void plotT(int16_t *buff, int minY, int maxY, int minA, int maxA, int num, int buffIndex)
{
	Serial.printf("minY =%d maxY =%d min =%d max =%d \n", minY, maxY, minA, maxA);
	minA -= 10;
	maxA += 10;
	const int minX = 10;
	const int maxX = display.width() - 1;
	if (num < 3) return;

	double span = SLEEP_MINUTES * num;
	double dx = (double)(maxX - minX) / (double)span;				//128/7505=0.017
	double dy = (double)(maxY - minY) / (double)(maxA - minA);//48/0.2=240



#pragma region Temerature grid

	display.drawFastVLine(minX - 1, minY, maxY - minY, GxEPD_BLACK); // y axis
	//display.drawFastHLine(minX - 2, (int)(maxY + minA * dy), 4, GxEPD_BLACK); // x axis
	for (int tens = -300; tens < maxA; tens += 100)
	{
		if (tens > minA && tens < maxA)
		{
			int y = (int)(maxY - (tens - minA) * dy);
			for (int x = minX - 1; x < maxX; x += 2)
				display.drawPixel(x, y, GxEPD_BLACK); // x axis
			display.setCursor(minX - 10, y + 2);
			display.printf("%02d", (int)(tens / 10));
		}
	}
	for (int tens = -250; tens < maxA; tens += 100)
	{
		if (tens > minA && tens < maxA)
		{
			int y = (int)(maxY - (tens - minA) * dy);
			for (int x = minX - 1; x < maxX; x += 2)
				display.drawPixel(x, y, GxEPD_BLACK); // x axis
			display.setCursor(minX - 10, y + 2);
			display.printf("%02d", (int)(tens / 10));
		}
	}
	if (maxA - minA < 140)
		for (int tens = -300; tens < maxA; tens += 10)
		{
			if (tens > minA && tens < maxA)
			{
				int y = (int)(maxY - (tens - minA) * dy);
				for (int x = minX - 1; x < maxX; x += 4)
					display.drawPixel(x, y, GxEPD_BLACK); // x axis
			}
		}
#pragma endregion

	int i = 0;
	for (int i1 = buffIndex - 1; i++ < num; i1--)
	{
		if (i1 < 0) i1 = BUFFSIZE - 1;
		int i2 = i1 - 1;
		if (i2 < 0) i2 = BUFFSIZE - 1;
		if (buff[i1] == EMPTYBUFF) break;
		if (buff[i2] == EMPTYBUFF) break;
		display.drawLine(
			(int)(maxX - (double)(SLEEP_MINUTES* (i - 1)) * dx),
			(int)(maxY - (buff[i1] - minA) * dy),
			(int)(maxX - (double)(SLEEP_MINUTES* i) * dx),
			(int)(maxY - (buff[i2] - minA)* dy),
			GxEPD_BLACK);
	}
}

void plot1()
{
	//if (buff1[buffIndex1] != EMPTYBUFF)
	{
		const int minX = 10;
		const int maxX = display.width() - 1;
		const int minY = 42;
		const int maxY = display.height(); //max 128
		int minA = 999, maxA = -999;
		int num = 0;
		for (int i = buffIndex1 - 1; num < BUFFSIZE; i--)
		{
			if (i < 0) i = BUFFSIZE - 1;
			if (buff1[i] == EMPTYBUFF) break;
			if (buff1[i] < minA) minA = buff1[i];
			if (buff1[i] > maxA) maxA = buff1[i];
			++num;
		}
		Serial.printf("index=%d, num=%d\n", buffIndex1, num);
		if (num < 3) return;

		//display.setFont(FONT_9);
		//displayTextAt(0, 30, "Min:" + String(minA / 10.0, 1) + "C");
		//displayTextAt(0, 15, "Max:" + String(maxA / 10.0, 1) + "C");

		minA = minA - 10;
		maxA = maxA + 10;
		double span = SLEEP_MINUTES * num;
		double dx = (double)(maxX - minX) / (double)span;				//128/7505=0.017
		double dy = (double)(maxY - minY) / (double)(maxA - minA);//48/0.2=240

		display.setFont(&Picopixel);

#pragma region Temerature grid

		display.drawFastVLine(minX - 1, minY, maxY - minY, GxEPD_BLACK); // y axis
		//display.drawFastHLine(minX - 2, (int)(maxY + minA * dy), 4, GxEPD_BLACK); // x axis
		for (int tens = -300; tens < maxA; tens += 100)
		{
			if (tens > minA && tens < maxA)
			{
				int y = (int)(maxY - (tens - minA) * dy);
				for (int x = minX - 1; x < maxX; x += 2)
					display.drawPixel(x, y, GxEPD_BLACK); // x axis
				display.setCursor(minX - 10, y + 2);
				display.printf("%02d", (int)(tens / 10));
			}
		}
		for (int tens = -250; tens < maxA; tens += 100)
		{
			if (tens > minA && tens < maxA)
			{
				int y = (int)(maxY - (tens - minA) * dy);
				for (int x = minX - 1; x < maxX; x += 4)
					display.drawPixel(x, y, GxEPD_BLACK); // x axis
				display.setCursor(minX - 10, y + 2);
				display.printf("%02d", (int)(tens / 10));
			}
		}
		if (maxA - minA < 140)
			for (int tens = -300; tens < maxA; tens += 10)
			{
				if (tens > minA && tens < maxA)
				{
					int y = (int)(maxY - (tens - minA) * dy);
					for (int x = minX - 1; x < maxX; x += 8)
						display.drawPixel(x, y, GxEPD_BLACK); // x axis
				}
			}
#pragma endregion

#pragma region hour grid
		int time = 1;
		int pxTime = 60 * dx; // one hour
		for (int i = maxX - pxTime; i > minX; i -= pxTime)
		{
			if (time % 24 == 0)
			{
				display.setCursor(i + 2, minY + 6);
				display.printf("%d", time / 24);
				display.drawFastVLine(i, minY, maxY - minY, GxEPD_BLACK);
			}
			else if (time % 12 == 0)
			{
				for (int y = minY; y < maxY; y += 2)
					display.drawPixel(i, y, GxEPD_BLACK);
			}
			else if (time % 6 == 0)
				display.drawFastVLine(i, maxY - 8, 8, GxEPD_BLACK);
			else if (pxTime > 1)
				display.drawFastVLine(i, maxY - 4, 4, GxEPD_BLACK);
			time++;
		}

#pragma endregion
		int i = 0;
		for (int i1 = buffIndex1 - 1; i++ < num; i1--)
		{
			/*		if (i < 0) i = BUFFSIZE - 1;

				for (int i = 1; i < num; i++)
				{
					int i1 = buffIndex1 - i;*/

			if (i1 < 0) i1 = BUFFSIZE - 1;
			int i2 = i1 - 1;
			if (i2 < 0) i2 = BUFFSIZE - 1;
			if (buff1[i1] == EMPTYBUFF) break;
			if (buff1[i2] == EMPTYBUFF) break;
			//Serial.printf("val [%d, %d] %d, %d to %d, %d\n", i1,i2,
			//	(int)(maxX - (double)(SLEEP_MINUTES* (i - 1)) * dx),
			//	(int)(maxY - (buff1[i1] - minA) * dy),
			//	(int)(maxX - (double)(SLEEP_MINUTES* i) * dx),
			//	(int)(maxY - (buff1[i2] - minA)* dy));
			display.drawLine(
				(int)(maxX - (double)(SLEEP_MINUTES* (i - 1)) * dx),
				(int)(maxY - (buff1[i1] - minA) * dy),
				(int)(maxX - (double)(SLEEP_MINUTES* i) * dx),
				(int)(maxY - (buff1[i2] - minA)* dy),
				GxEPD_BLACK);
		}
	}
}
void plot2()
{
	//if (buff2[buffIndex2] != EMPTYBUFF)
	{
		const int minX = 10;
		const int maxX = display.width() - 1;
		const int minY = 42;
		const int maxY = display.height(); //max 128
		int minA = 999, maxA = -999;
		int num = 0;
		for (int i = buffIndex2 - 1; num < BUFFSIZE; i--)
		{
			if (i < 0) i = BUFFSIZE - 1;
			if (buff2[i] == EMPTYBUFF) break;
			if (buff2[i] < minA) minA = buff2[i];
			if (buff2[i] > maxA) maxA = buff2[i];
			++num;
		}
		Serial.printf("index=%d, num=%d\n", buffIndex2, num);
		if (num < 3) return;

		//display.setFont(FONT_9);
		//displayTextAt(0, 30, "Min:" + String(minA / 10.0, 1) + "C");
		//displayTextAt(0, 15, "Max:" + String(maxA / 10.0, 1) + "C");

		minA = minA - 10;
		maxA = maxA + 10;
		double span = SLEEP_MINUTES * num;
		double dx = (double)(maxX - minX) / (double)span;				//128/7505=0.017
		double dy = (double)(maxY - minY) / (double)(maxA - minA);//48/0.2=240

		display.setFont(&Picopixel);

#pragma region Temerature grid

		display.drawFastVLine(minX - 1, minY, maxY - minY, GxEPD_BLACK); // y axis
		//display.drawFastHLine(minX - 2, (int)(maxY + minA * dy), 4, GxEPD_BLACK); // x axis
		for (int tens = -300; tens < maxA; tens += 100)
		{
			if (tens > minA && tens < maxA)
			{
				int y = (int)(maxY - (tens - minA) * dy);
				//for (int x = minX - 1; x < maxX; x += 2)
				//	display.drawPixel(x, y, GxEPD_BLACK); // x axis
				display.setCursor(maxX - 10, y + 2);
				display.printf("%02d", (int)(tens / 10));
			}
		}
		for (int tens = -250; tens < maxA; tens += 100)
		{
			if (tens > minA && tens < maxA)
			{
				int y = (int)(maxY - (tens - minA) * dy);
				//for (int x = minX - 1; x < maxX; x += 4)
				//	display.drawPixel(x, y, GxEPD_BLACK); // x axis
				display.setCursor(maxX - 10, y + 2);
				display.printf("%02d", (int)(tens / 10));
			}
		}
		//if (maxA - minA < 140)
		//	for (int tens = -300; tens < maxA; tens += 10)
		//	{
		//		if (tens > minA && tens < maxA)
		//		{
		//			int y = (int)(maxY - (tens - minA) * dy);
		//			for (int x = minX - 1; x < maxX; x += 8)
		//				display.drawPixel(x, y, GxEPD_BLACK); // x axis
		//		}
		//	}
#pragma endregion

		int i = 0;
		for (int i1 = buffIndex2 - 1; i++ < num; i1--)
		{
			/*		if (i < 0) i = BUFFSIZE - 1;

				for (int i = 1; i < num; i++)
				{
					int i1 = buffIndex2 - i;*/

			if (i1 < 0) i1 = BUFFSIZE - 1;
			int i2 = i1 - 1;
			if (i2 < 0) i2 = BUFFSIZE - 1;
			if (buff2[i1] == EMPTYBUFF) break;
			if (buff2[i2] == EMPTYBUFF) break;
			//Serial.printf("val [%d, %d] %d, %d to %d, %d\n", i1,i2,
			//	(int)(maxX - (double)(SLEEP_MINUTES* (i - 1)) * dx),
			//	(int)(maxY - (buff2[i1] - minA) * dy),
			//	(int)(maxX - (double)(SLEEP_MINUTES* i) * dx),
			//	(int)(maxY - (buff2[i2] - minA)* dy));
			display.drawLine(
				(int)(maxX - (double)(SLEEP_MINUTES* (i - 1)) * dx),
				(int)(maxY - (buff2[i1] - minA) * dy),
				(int)(maxX - (double)(SLEEP_MINUTES* i) * dx),
				(int)(maxY - (buff2[i2] - minA)* dy),
				GxEPD_BLACK);
		}
	}
}

void showPage2(void)
{
	displayInit();
	display.fillScreen(GxEPD_WHITE);
	display.setFont(&FreeMono9pt7b);

	//displayTextAt(0, 10, String(ti1.LastValue(), 1) + "C");
	//displayTextAt(0, 25, String(ti1.Avg(10000), 2) + "Avg 10s");
	//displayTextAt(0, 40, String(ti1.Avg(120000), 2) + "Avg 2m");
	//displayTextAt(0, 55, String(ti1.Avg(1800000), 2) + "Avg 30m");

	//displayText(String(ti1.Count()) + "#", 25, RIGHT_ALIGNMENT, 0);

	//displayTextAt(0, 70, "Min " + String(ti1.min, 1));
	//displayTextAt(0, 85, "Max " + String(ti1.max, 1));

	displayTextAt(5, 127, "Reset btn: 1-min/max 2-");

	display.update();
	lastDisplay = 2;
}
void showRebootPage(void)
{
	displayInit();
	display.fillScreen(GxEPD_WHITE);
	display.setFont(FONT_24);
	displayText("REBOOT", 84, CENTER_ALIGNMENT, 0);
	display.update();
}
void showTextPage(const char *cmd)
{
	displayInit();
	display.fillScreen(GxEPD_WHITE);
	display.setFont(&FreeMono9pt7b);

	display.setFont(FONT_12);
	display.setCursor(0, 16);
	display.println(cmd);
	
	display.update();
	lastDisplay = 2;
}

#pragma endregion

#pragma region Init diplay and wifi azure

void displayInit(void)
{
	static bool isInit = false;
	if (isInit) {
		return;
	}
	Serial.printf("displayInit\n");

	isInit = true;
	display.init();
	display.setRotation(1);
	display.eraseDisplay();
	display.setTextColor(GxEPD_BLACK);
	display.setFont(FONT_9);
	display.setTextSize(0);

	// needed????
	//display.fillScreen(GxEPD_WHITE);
	//display.update();
	//delay(2000);
}

void InitWifiAndAzure()
{
	Serial.printf("Connecting to %s ", wifinet[0]);
	WiFi.begin(wifinet[0], wifipwd[0]);
	int i = 0;
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");

		if (++i > 60)
		{
			if (++wifiFailCount > 4)
			{
				Serial.println("Press button to start/stop WiFi router.");
				wifiRestarts++;
				wifiFailCount = 0;
				showTextPage("Can't connect to WiFi\nReboot router");
				PulsOut(RELAY1);  //Reboot router
				GotoSleep();
			}
			Serial.println("Can't connect to WiFi.");
			showTextPage("Can't connect to WiFi");
			Serial.flush();
			GotoSleep();
			//ESP.restart();
		}
	}
	wifiFailCount = 0;
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.printf("RSSI %d Ch.%d IP ", WiFi.RSSI(), WiFi.channel());
	Serial.println(WiFi.localIP());

	Azure.Setup();
}
#pragma endregion

void PulsOut(int io)
 {
	digitalWrite(io, HIGH);
	delay(5000);
	digitalWrite(io, LOW);
 }

void DemoOut()
{
	digitalWrite(RELAY1, HIGH);
	delay(1000);
	digitalWrite(RELAY1, LOW);
	digitalWrite(RELAY2, HIGH);
	delay(1000);
	digitalWrite(RELAY2, LOW);
	digitalWrite(RELAY3, HIGH);
	delay(1000);
	digitalWrite(RELAY3, LOW);
	digitalWrite(RELAY4, HIGH);
	delay(1000);
	digitalWrite(RELAY4, LOW);
}

void setup()
{
	Serial.begin(115200);

	// pinMode(RED_LED, OUTPUT);
	pinMode(BUTTON_3, INPUT);

	pinMode(RELAY1, OUTPUT);
	pinMode(RELAY2, OUTPUT);
	pinMode(RELAY3, OUTPUT);
	pinMode(RELAY4, OUTPUT);
	digitalWrite(RELAY1, LOW);
	digitalWrite(RELAY2, LOW);
	digitalWrite(RELAY3, LOW);
	digitalWrite(RELAY4, LOW);
	
	Serial.printf("Init OneWire on pin %d.\n", OneWirePin);
	sensors.begin(); // temperature one wire

	Serial.printf("Init SPI on pin %d %d %d.\n", SPI_CLK, SPI_MISO, SPI_MOSI);
	SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, -1);  //display?

	if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) 
	{
		Serial.print("\n***** RouterCtrlTemp. Device Id: ");
		Serial.println(DEVICE_ID);

		FindSensors();

		Serial.println("Empty buffer.");
		for (int i = 0; i < BUFFSIZE; i++)
			buff2[i] = buff1[i] = EMPTYBUFF;

		if (digitalRead(BUTTON_3) == LOW)
		{
			Serial.println("BUTTON DOWN");
			for (int i=0;i<3;i++)
			{
				DemoOut();
				delay(500);
			}	
		}
		
#ifdef TEST
		int num = BUFFSIZE - 1;
		for (int i = 0; i < num; i++)
		{
			buff1[i] = 200 + 20.0 * sin((double)(0.05*(i & 0xff)));
			buff2[i] = 50 + 50.0 * cos((double)(0.03*(i & 0xff)));
		}
		buffIndex2 = buffIndex1 = num;
#endif
	}

	ReadTemperature();

	showMainPage();

#ifndef	AZURE_OFF
	if (azureMode != MODE_OFFLINE)
	{
		Serial.printf("Init WiFi and Azure.\n");
		InitWifiAndAzure();
		if (azureMode != MODE_OFFLINE && WiFi.status() == WL_CONNECTED && ++azureSendCounter >= azureSendInterval)
		{
			azureSendCounter = 0;
			Azure.Send();
		}
	}
#endif
	if (WiFi.status() == WL_CONNECTED) {
		Azure.Check();
	}

	if (rebootRequest)
	{
		showRebootPage();
		ESP.restart();
	}

	if (!cmdstring.isEmpty())
	{
		Serial.printf("DoCommand from setup: '%s'.\n", cmdstring.c_str());
		DoCommand();
	}

	if (stayOnlineMinutes == 0)
		GotoSleep();

	Serial.printf("Setup complete. Stay online for %u minutes.\n", stayOnlineMinutes);
	idleSince = millis(); 
}

void loop()
{
	ulong t = millis();
	static ulong lastTimeTempRead = t;
	// static int led = 0;
	static bool sentStatusToAzureDone = false;

	if (t - lastTimeTempRead > SLEEP_MINUTES * 60000)
	{
		lastTimeTempRead = t;
		ReadTemperature();
		showMainPage();

		if (azureMode != MODE_OFFLINE && WiFi.status() == WL_CONNECTED && ++azureSendCounter >= azureSendInterval)
		{
			azureSendCounter = 0;
			Azure.Send();
		}
	}

	if (t - idleSince > stayOnlineMinutes * 60000)
	{
		stayOnlineMinutes = 0;
		GotoSleep();
		idleSince = t;   // In case SLEEP_MINUTES is null
	}
	if (t - lastUpdate1 > 10000)
	{
		ulong sec= (idleSince + stayOnlineMinutes * 60000 - t)/1000;
		Serial.printf("Time to sleep: %02u:%02u:%02u \n", numberOfHours(sec), numberOfMinutes(sec),numberOfSeconds(sec));
		lastUpdate1 = t;  
	}
			
	Azure.Check();
	if (!cmdstring.isEmpty())
	{
		Serial.printf("DoCommand from loop: '%s'.\n", cmdstring.c_str());
		DoCommand();
	}
	delay(500);
}


void FindSensors()
{
	sensCount = sensors.getDS18Count();
    Serial.printf("Found %d sensores on pin %d.\n", sensCount, OneWirePin);

  { // echo sensor adresses
    DeviceAddress adr;
    int i = 0;
    Serial.printf("OneWire on pin %d:\n", OneWirePin);
    while (sensors.getAddress(adr, i))
    {
      Serial.printf("  DeviceAddress T%d= {", i);
      Serial.printf("0x%02x", adr[0]);
      for (int j = 1; j < 8; j++)
        Serial.printf(", 0x%02x", adr[j]);

      Serial.println("};");
      i++;
    }
  }
}

void ReadTemperature()
{
	double avg = 0;
	int num = 0;
	for (int i = 0; i < 10; i++)
	{
		sensors.requestTemperatures(); // Send the command to get temperatures
		double t1 = sensors.getTempCByIndex(0);
		if (t1 > -120.0)
		{
			avg += t1;
			num++;
			//Serial.printf("Temperatur %7.4f Avg:%d  %7.2f \n", t1, i, avg / (double)num);
		}
	}
	temperature2 = sensors.getTempCByIndex(1);
	//temperature3 = sensors.getTempCByIndex(2);
	//temperature4 = sensors.getTempCByIndex(3);

	if (num > 0)
	{
		temperature1 = avg / (double)num;
		buff1[buffIndex1] = (int16_t)(temperature1 * 10.0);
		if (++buffIndex1 >= BUFFSIZE)buffIndex1 = 0;
		buff1[buffIndex1] = EMPTYBUFF;
	}
	else
		temperature1 = -99.0;

	if (temperature2 > -80)
	{
		buff2[buffIndex2] = (int16_t)(temperature2 * 10.0);
		if (++buffIndex2 >= BUFFSIZE)buffIndex2 = 0;
		buff2[buffIndex2] = EMPTYBUFF;
	}
	else
		temperature2 = -99.0;

	Serial.printf("   Temperatur %7.2f %7.2f\n", temperature1, temperature2);
}

void GotoSleep()
{
	esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_3, LOW);
	esp_sleep_enable_timer_wakeup(SLEEP_MINUTES * uS_TO_M_FACTOR);
	Serial.printf("Going to sleep for %d minutes", SLEEP_MINUTES);
	Serial.flush();
	delay(1000);
	esp_deep_sleep_start();
}

// char todo[80];
String cmdstring;

// void DoCommand(const char *cmd)
void DoCommand()
{
	if (cmdstring.isEmpty())
	{
		Serial.println("DoCommand: nothing to do");
		return;
	}
	Serial.println("DoCommand");

	// String cmdstring(cmd);

  if (cmdstring.indexOf("reboot") >= 0)
  {
    ESP.restart();
  }
  
 	if (cmdstring.indexOf("puls1") >= 0)
    {
        Serial.printf("Do puls1.\n");
		wifiRestarts++;
        PulsOut(RELAY1);
		// WiFi will die
		GotoSleep();
    }

 	if (cmdstring.indexOf("puls2") >= 0)
    {
        Serial.printf("Do puls2.\n");
        PulsOut(RELAY2);
    }
	else if (cmdstring.indexOf("turnoff2") >= 0)
    {
        Serial.printf("Turn off 2.\n");
       	digitalWrite(RELAY2, LOW); 
    }
	else if (cmdstring.indexOf("turnon2") >= 0)
    {
        Serial.printf("Turn on 2.\n");
       	digitalWrite(RELAY2, HIGH); 
    }

	
 	if (cmdstring.indexOf("puls3") >= 0)
    {
        Serial.printf("Do puls3.\n");
        PulsOut(RELAY3);
    }
	else if (cmdstring.indexOf("turnoff3") >= 0)
    {
        Serial.printf("Turn off 3.\n");
       	digitalWrite(RELAY3, LOW); 
    }
	else if (cmdstring.indexOf("turnon3") >= 0)
    {
        Serial.printf("Turn on 3.\n");
       	digitalWrite(RELAY3, HIGH); 
    }


 	if (cmdstring.indexOf("puls4") >= 0)
    {
        Serial.printf("Do puls4.\n");
        PulsOut(RELAY4);
    }
	else if (cmdstring.indexOf("turnoff4") >= 0)
    {
        Serial.printf("Turn off 4.\n");
       	digitalWrite(RELAY4, LOW); 
    }
	else if (cmdstring.indexOf("turnon4") >= 0)
    {
        Serial.printf("Turn on 4.\n");
       	digitalWrite(RELAY4, HIGH); 
    }


	if (cmdstring.indexOf("stayonline") >= 0)
	{
		Serial.println("Stay online for 8 hour.");
		stayOnlineMinutes = 8 * 60;
	}
	else if (cmdstring.indexOf("gotosleep") >= 0)
	{
		Serial.println("Request sleep.");
		stayOnlineMinutes = 0;
	}

	
	int i = cmdstring.indexOf("azureinterval");  //NB 2 tall godtas 'azureinterval12' Default 6
    if (i >= 0 && cmdstring.length() >= i + 15)
    {
      int xt = cmdstring.substring(i + 13, i + 15).toInt();
      Serial.printf("Send to Azure hver %d *10 minutt.\n", xt);
      if (xt > 0 && xt <100)
        azureSendInterval = xt;
    }

	showTextPage(cmdstring.c_str());
	cmdstring.clear();

	idleSince = millis();
}
