/* Akvárium vezérlő
 * Készítette: Rojkó Tibor
 * Verzió: 3.0
 * Dátum: 2018.11.17 */

//Header file-ok behívása
#include "pitches.h"                      //Dallam tár 
#include "OneWire.h"                      //1-WIre busz könyvtár
#include "DallasTemperature.h"            //Hőmérséklet szenzor könyvtár
#include "Wire.h"                         //I2C busz könyvtár
#include "RTClib.h"                       //Valós idejű óra könyvtár
#include "LiquidCrystal.h"                //LCD kijelző könyvtár 

//Globális deklarációk
RTC_DS3231 CLK;                           //Óra példányosítása

//1-wire bus
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//LCD kijelző
LiquidCrystal lcd(12, 11, 3, 5, 6, 7);    //LCD példányosítás

//Dallam beállítások
int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

#include "./aquaCTRL.h"                     //aquaCTRL osztály fájl
//AquaCTRL osztály példányosítása
AquaCTRL Aqua(9,10,0,1,2,13);

void setup() {
  Aqua.initializing();
}

void loop() {
  
  //Hőmérséklet lekérdezése, és ellenőrzése
  Aqua.getTemperature();
  //Idő lekérdezése, és az idő alapú vezérlések elvégzése (etetés, oxigén pumpálás, lámpa vezérlés)
  Aqua.doTiming();
  //Szűrő kezelés
  Aqua.filter();
  //Fedél zártságának az ellenőrzése
  Aqua.roof();
  //Főképernyő kiíratás
  Aqua.mainDisplay();
}
