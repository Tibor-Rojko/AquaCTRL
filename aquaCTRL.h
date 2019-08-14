class AquaCTRL{
	public:
		AquaCTRL(int bpin, int lcdpin, int fpin, int o2pin, int lpin, int hpin);
		void initializing();
		void getTemperature();
		void mainDisplay();
		void warningDisplay(char * msg);
		void alarmDisplay(char * msg);
		void doTiming();
		int O2Pumping(int stat);
		int feeding(int stat);
		int heating(int stat);
		void relayController(int device, int stat);
		void filter();
		int roof();
		void beep(int delayms);
		void alarm(int delayms);
		void lampCtrl();
	private:
		int hom = 0;                               	//Aktuális hőmérséklet tárolása
		int duringHeating;							//segéd
		int duringFeeding;							//segéd 2
		int duringPumping;							//seged 3
		int roofOpen;								//segéd 4
		char msg1[13] = "Etetni kell!";             //LCD ünenet
		char msg2[13] = "Meleg a viz!";             //LCD ünenet
		char msg3[14] = "Fedel nyitva!";            //LCD ünenet
		//Speciális karakter az LCD kijelzőhöz a celsius fok jelzésére
		byte circle[8] = {    	B01000,  
								B10100,
								B01000,
								B00000,
								B00000,
								B00000,
								B00000,
		};
		int beep_pin;                      //beep kimeneti pin (PWM)
		int LCD_backLight;                     //LCD háttérvilágítás anódra kimenet (PWM)
		int filterpin;                      //szürő relét vezérlő pin
		int o2_pump;                      //oxigén pumpa relét vezérlő pin
		int lamp;                      //lámpa relét vezérlő pin
		int heater;                     //vízmelegítő relét vezérlő pin
		DateTime now;
};		

AquaCTRL::AquaCTRL(int bpin, int lcdpin, int fpin, int o2pin, int lpin, int hpin){
	beep_pin = bpin;
	LCD_backLight = lcdpin;
	filterpin = fpin;
	o2_pump = o2pin;
	lamp = lpin;
	heater = hpin;
}

//Ebben a függvényben állítom be a szenzorokat, órát, és vezérlő pineket >>> setup() -ban kell meghívni
void AquaCTRL::initializing(){
	//LCD init
	lcd.begin(16, 2);                               //LCD sor, oszlop beállítása
	lcd.createChar(1, circle);                      //speciális karakter létrehozása
	pinMode(LCD_backLight, OUTPUT);                            //10-es PWM LCD_backLight pin beállítása kimenetként
	digitalWrite(LCD_backLight, HIGH);              //Kijelző háttérvilágítás bekapcsolása
	lcd.clear();
	lcd.write("Inicializalas...");
	delay(1500);
	now = CLK.now();
	if(now.hour() > 5 && now.hour() < 22){
		analogWrite(LCD_backLight, 255);  //A fényerő beállítása 100%-ra
    }else{
		analogWrite(LCD_backLight, 64); //Fényerő beállítása 25%-ra
    }

	//relét vezérlő pinek beállítása kimenetként
	pinMode(filterpin ,OUTPUT);
	pinMode(o2_pump ,OUTPUT);
	pinMode(lamp ,OUTPUT);
	pinMode(heater ,OUTPUT);
	
	//beep init
	pinMode(beep_pin, OUTPUT); 
	
	//óra init
	CLK.begin();
	delay(3000);
	if (CLK.lostPower()) {
		lcd.clear();
		lcd.write("ERROR");
		lcd.setCursor(0,1);
		lcd.write("RTC HUB hiba");
	}
	
	//homerseklet szenzor init
	sensors.begin();
	
	//soros monitor init
	Serial.begin(9600);
}

//Beep
void AquaCTRL::beep(int delayms){
  analogWrite(beep_pin, 25);      
  delay(delayms);          
  analogWrite(beep_pin, 0);       
  delay(delayms);          
}


void AquaCTRL::getTemperature(){
	sensors.requestTemperatures(); 
	hom = sensors.getTempCByIndex(0);
	
	if(hom < 24){
		//vízmelegítő elindítása
		duringHeating = heating(1);
	}else{
		//vízmelegítő kikapcsolása
		duringHeating = heating(0);
	}
	
	//Amíg a víz hőmérséklete 34 fok felett van, addig minden rendszer leáll, csak figyelmeztetés megy, újra lekérdezzük a hőmérsékletet minden while körben
	while(hom > 34){
		getTemperature();
		warningDisplay(msg2);
		beep(250);
	}
}

void AquaCTRL::warningDisplay(char * msg){
	lcd.clear();
	lcd.write("Figyelem!");
	lcd.setCursor(0,1);
	lcd.write(msg);
}

void AquaCTRL::doTiming(){
	now = CLK.now();
	//Idő alapú vezérlések elvégzése
	lampCtrl();	//Lámpa vezérlés
	
	//Etetés vezérlés
	if(now.hour() == 10 && now.minute() == 0 || now.hour() == 14 && now.minute() == 0 || now.hour() == 19 && now.minute() == 0){ //Ha az idő 10:00, vagy 14:00, vagy 19:00, akkor
		duringFeeding = feeding(1); //etetés függvény meghívása 1 értékű paraméterrel, ilyenkor a függvény 1-et ad vissza, így a duringFeeding egyenlő lesz 1-el
	}else if(now.hour() == 10 && now.minute() > 30 || now.hour() == 14 && now.minute() > 30 || now.hour() == 19 && now.minute() > 30){ //Ha 10:30, vagy 14:30, vagy 19:30, akkor 
		duringFeeding = feeding(0); //etetés leállítása 0-ás paraméterrel, ilyenkor a duringFeeding egyenlő lesz 0-val
	}
	
	//Oxigén pumpálás vezérlése
	if(now.hour() == 8 && now.minute() == 30 || now.hour() == 17 && now.minute() == 30 || now.hour() == 21 && now.minute() == 30){
		duringPumping = O2Pumping(1);
	}else if(now.hour() == 9 && now.minute() == 0 || now.hour() == 18 && now.minute() == 0 || now.hour() == 22 && now.minute() == 0){
		duringPumping = O2Pumping(0);
	}

}

void AquaCTRL::lampCtrl(){
	//Lámpa vezérlés
	if(now.hour() > 5 && now.hour() < 22){ 	//nappal van
		relayController(3,1); 				//lámpa be
		analogWrite(LCD_backLight, 255);	//LCD fényerő beállítás
	}else{ 									//eset van
		relayController(3,0); 				//lámpa ki
		analogWrite(LCD_backLight, 64);		//LCD fényerő beállítás
	}	
}

//Etetés
int AquaCTRL::feeding(int stat){
  if(stat == 1){            //Ha 1-es érték érkezett paraméterként, akkor etetni kell
	roof();
    while(roofOpen == 0){     //Amíg a fedelet nem nyitják fel, addig 
        alarm(250);         //Emlékeztető zene
        alarmDisplay(msg1); //Emlékeztető kiírás
    }
    relayController(1,0);   //Szürő kikapcsolása
    return 1;               //Visszatérés a LOOP-ba 1-es értékkel
  }else{                    //Ha 0-ás paraméter érkezett, akkor vége az etetési időnek
    relayController(1,1);   //Szürő bekapcsolása
    return 0;
  }
}

//Oxigén pumpálás
int AquaCTRL::O2Pumping(int stat){
    if(stat == 1){
      relayController(2,1); //Oxigén pumpa bekapcsolása
      return 1;  
    }else{
      relayController(2,0); //Oxigén pumpa kikapcsolása
      return 0;
    }
}

//melegítés
int AquaCTRL::heating(int stat){
  if(stat == 1){
      relayController(4,1); //Vízmelegítő bekapcsolása
      return 1;
   }else{
      relayController(4,0); //Víymelegítő kikapcsolása
      return 0;
   }
}

//Relék vezérlése
void AquaCTRL::relayController(int device, int stat){
    switch (device) {
      case 1: //Szürő
          digitalWrite(filterpin, stat);
        break;
      case 2: //Oxigén pumpa
          digitalWrite(o2_pump, stat);
        break;
      case 3: //Villágítás
          digitalWrite(lamp, stat);
        break;
      case 4: //Vízmelegítő
          digitalWrite(heater, stat);
        break;
  }
}

void AquaCTRL::alarmDisplay(char * msg){
  lcd.clear();
  lcd.write("Emlekezteto!");
  lcd.setCursor(0,1);
  lcd.write(msg);
}

void AquaCTRL::filter(){
	//Ha nincs etetés
	if(duringFeeding == 0){
		relayController(1,1); //Szürő bekapcsolása
	}
}	

void AquaCTRL::mainDisplay(){
	//Főképernyő 1 - Dátum/Idő, és hőmérséklet kiíratása
    lcd.clear();
    lcd.print(now.year());
    lcd.print(".");
    lcd.print(now.month());
    lcd.print(".");
    lcd.print(now.day());
    lcd.print(" ");
    lcd.print(now.hour());
    lcd.print(":");
    lcd.print(now.minute());
    lcd.setCursor(0,1);
    lcd.print("Hom: ");
    lcd.print(hom);
    lcd.print(" C");
    lcd.write(1);
    delay(2500);
    
	//Főképernyő 2-es
  if(duringFeeding == 1){  //ha etetés van, akkor etetés üzenet
    lcd.clear();
    lcd.print(now.year());
    lcd.print(".");
    lcd.print(now.month());
    lcd.print(".");
    lcd.print(now.day());
    lcd.print(" ");
    lcd.print(now.hour());
    lcd.print(":");
    lcd.print(now.minute());
    lcd.setCursor(0,1);
    lcd.write("Etetes...");
    delay(2500);  
  }else if(duringPumping == 1){ //ha oxigén pumpálás van, akkor oxigén pumpálás üzenet
    lcd.clear();
    lcd.print(now.year());
    lcd.print(".");
    lcd.print(now.month());
    lcd.print(".");
    lcd.print(now.day());
    lcd.print(" ");
    lcd.print(now.hour());
    lcd.print(":");
    lcd.print(now.minute());
    lcd.setCursor(0,1);
    lcd.write("Oxigen pumpalas");
    delay(2500); 
  }else if(duringHeating == 1){ //ha melegítés van, akkor melegítés üzenet
    lcd.clear();
    lcd.print(now.year());
    lcd.print(".");
    lcd.print(now.month());
    lcd.print(".");
    lcd.print(now.day());
    lcd.print(" ");
    lcd.print(now.hour());
    lcd.print(":");
    lcd.print(now.minute());
    lcd.setCursor(0,1);
    lcd.write("Viz melegites...");
    delay(2500);
  }else if(roofOpen == 1 && duringFeeding == 0){
	 warningDisplay(msg3);
     beep(250);       //Figyelmeztető jelzés 
  }else{   //Ha nincs etetés, vagy nincs oxigén pumpálás, akkor szürés üzenet
    lcd.clear();
    lcd.print(now.year());
    lcd.print(".");
    lcd.print(now.month());
    lcd.print(".");
    lcd.print(now.day());
    lcd.print(" ");
    lcd.print(now.hour());
    lcd.print(":");
    lcd.print(now.minute());
    lcd.setCursor(0,1);
    lcd.print("Szures...");
    delay(2500); 
   }
}

//Fedél ellenőrzése
int AquaCTRL::roof(){
  int sensorValue = analogRead(A0);
  float voltage = sensorValue * (5.0 / 1023.0);
  if(voltage > 2.5){
    roofOpen = 0; //Fedél becsukva
  }else{
    roofOpen = 1; //Fedél nyitva
  }
}

//Alarm
void AquaCTRL::alarm(int delayms){
  for (int thisNote = 0; thisNote < 8; thisNote++) {
    int noteDuration = 1500 / noteDurations[thisNote];
    tone(8, melody[thisNote], noteDuration);
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(delayms);
    noTone(8);
  }
}