

//Kameraschlitten Dennis Bodenmüller
//Technikerarbeit
//01.02.2019 - Finale Version

#include <NikonRemote.h>
NikonRemote remote(0);
//NikonRemote remote(27);

#include <StepControl.h>
Stepper stepperX(2,3); // STEP pin: 2, DIR pin: 3
Stepper stepperY(4,5);
Stepper stepperZ(6,7);
StepControl<> cnt1;    // Use default settings 
StepControl<> cnt2;    // Use default settings 
StepControl<> cnt3;    // Use default settings 


//#include <EEPROM.h>

//Library Display
#include <Arduino.h>
#include <U8x8lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
U8X8_SSD1327_MIDAS_128X128_4W_SW_SPI u8x8(/* clock=*/ 9, /* data=*/ 8, /* cs=*/ 10, /* dc=*/ 11, /* reset=*/ U8X8_PIN_NONE);


//Konstantendeklaration
const int encoderCLK =                                  25;   //Encoder Clock-Pin
const int encoderDT =                                   24;   //Encoder Data-Pin
const int encoderSW =                                   12;   //Encoder Taster-Pin
const int endX =                                        1;    //Endlage der X-Achse
const int PotX =                                        A0;   //Poti X-Achse 
const int PotY =                                        A2;   //Poti Drehachse
const int PotZ =                                        A1;   //Poti Schwenkachse
const int Increments =                                  2000; //Maximale Strecke die pro Zyklus gefahren werden kann


//Variablendeklaration
int JoyMid[3];                                                //Variable für Potimittelwerte wenn Joystick nicht betätigt 0-2, X-Z
const int JoyHysterese[]={14,16,14};                          //Wert in Prozent, in dem sich der Joystick in der Mitte nicht bewegt
const int JoyRefresh = 20;                                    //Aktualisierungsrate Poti (50Hz)
const int stepperMaxJoy[]={60000,20000,15000};                //Geschwindigkeit maximal Joystick, Steps/Sekunde
const int speedDefault[]={40000,30000,20000};                 //Geschwindigkeit Standard, Steps/Sekunde
const int speedZeitraffer[]={5000,400,350};                   //Geschwindigkeit Zeitraffer, Steps/Sekunde
const int speedRef[]={20000};                                 //Geschwindigkeit Referenzfahrt, Steps/Sekunde
const int accelerationDefault[]={20000,15000,10000};          //Beschleunigung Standart
const int accelerationVideo[]={90000,45000,25000};            //Beschleunigung Videomodus
const int accelerationRef[]={-60000};                         //Beschleunigung Referenzfahrt
int PullInSpeed = 100000;                                     //Variable dient dazu, dass keine Beschleunigung im Joystickbetrieb ausgeführt wird
int PullIn = 1000;          
int Increments = 2000;                                        //Maximale Strecke die pro Zyklus gefahren werden kann
volatile bool change=0;                                       //Gibt den klick des Encoders weiter
volatile int fall = 0;                                        //Dient dazu mit dem Encoder hoch zu zählen
volatile int fall_n;                                          //Dient dazu mit dem Encoder runter zu zählen
int anzahlBilder = 10;                                        //Standardeinstellung Anzhal der Bilder
int anzahlBildermax = 50000;                                  //Maximale Anzhal der Bilder
int restBilder;                                               //Restliche aufzunehmende Bilder
int intervall = 4;                                            //Standardeinstellung Intervall
int belichtungszeit = 1000;                                   //Standardeinstellung Belichtungszeit
int sicherheitszeit = 2000;                                   //Standardeinstellung Sicherheitszeit
int startpunkt = 1;                                           //Standardeinstellung Auswahl der Startpunktes
int endpunkt = 2;                                             //Standardeinstellung Auswahl der Endpunktes
int gewuenschteH = 0;                                         //Standardeinstellung Videodauer Stunden
int gewuenschteM = 0;                                         //Standardeinstellung Videodauer Minuten
int gewuenschteS = 10;                                        //Standardeinstellung Videodauer Sekunden
float schwarzzeit;            
bool zeitrafferAktiv = 0;
int teachpunkt =1;                                            //Standardeinstellung ausgewählter Teachpunkt
//const int refGeschwX = -2000;                               //Geschwindigkeit bei Referenzfahrt in Steps/sec
const int refOffsetX= 1000;                                   //Offsetschritte die zurueckgefahren werden, sobald endschalter erreicht wurde
bool refXOK = 0;                                              //Variable, dass Referenzfahr ausgeführt wurde
//bool zeitraffer;            
//bool referenzFahren;

//Koordinatenarrays für die Teachpunkte 1-20, Teachpunkt 0 standardmäßig 0,0,0 (Teachpunkt "Ursprung")
//Array von [20] besteht aus 21 Werten, da die 0 dazu gehoert
long teachKoords[3][20]; 

//Arrays Für Koordinaten-Teachpunkte deklarieren
teachKoords[0][0];   
teachKoords[1][0];
teachKoords[2][0]; 

//Verzeichnisanwahl Unterorder3<--Unterordner2<--Unterordner1<--Hauptmenü
//                  0-9XX    |   0-9X        |   1-9      |     0
int root = 000000;






void setup() {
  Serial.begin(9600);
  Serial.println("Start");

  //Eingänge Deklarieren
  pinMode(encoderCLK, INPUT_PULLUP);
  pinMode(encoderDT, INPUT_PULLUP);
  pinMode(encoderSW, INPUT_PULLUP);
  pinMode(endX, INPUT_PULLUP);
  pinMode(PotX, INPUT);
  pinMode(PotY, INPUT);
  pinMode(PotZ, INPUT);

  //Interrupt Service Routine initialisieren
  attachInterrupt(digitalPinToInterrupt(encoderCLK), isrA, LOW);

  //imaginäre Position der Schrittmotoren zurücksetzen
  stepperX.setPosition(0);
  stepperY.setPosition(0);
  stepperZ.setPosition(0);

  //Schwarzzeit Initialisieren
  schwarzzeit = intervall - belichtungszeit/1000.0 - sicherheitszeit/1000.0;

  //Potis Kalibrieren
  JoyMid[0] = analogRead(PotX);       //Sollte im Idealfall 512 sein (1024/2)
  JoyMid[1] = analogRead(PotY);
  JoyMid[2] = analogRead(PotY);

  //Display aktivieren, auf Eingabe warten
  displayStartbild();
  while(change==0);
  change=0;
  
  //Abfrage: Referenzfahrt ausführen? -> ausführen oder überspringen -> in Hauptmenü + Hauptschleife springen
  u8x8.clear();
  displayIniReferenzfahrt();
  while(change == 0){
    if(digitalRead(encoderSW)==0){
      referenzfahrt();
      break;
    }
  }
  u8x8.clear();
  change=1;
}

void loop() {

  //Unterprogramme

  HardwareEndlage();
  hauptmenu();
  hauptmenu_handbetrieb();
  hauptmenu_videomodus();
  hauptmenu_videomodus_dauerDerFahrt();
  hauptmenu_zeitraffer();
  hauptmenu_zeitraffer_setup();
  hauptmenu_referenz();
  
  //Joystick aktivieren wenn in Modus Handbetrieb
  if(root==001){
    JoystickControl();
  }



}

void hauptmenu(){
  //Hauptmenu Display initialisieren und Pfeilanwahl
  if(change == 1 && root == 0){
    change=0;
    display000();
    displayClearColumn(0);

    //Curseranwahl
    switch(fall){
      case 1: //Handbetrieb
            u8x8.drawString(0,0, "\x008d");
            break;
      case 2: //Automatik
            u8x8.drawString(0,1, "\x008d");
            break;
      case 3: //Zeitraffer
            u8x8.drawString(0,2, "\x008d");
            break;
      case 4: //Referenz
            u8x8.drawString(0,3, "\x008d");
            break;
    }
  }

  //Hauptmenu Durchwahl
  if(root== 0 && digitalRead(encoderSW) == 0){
    entprellroutine(encoderSW);
    u8x8.clear();   //LCD für nächsten Ordner leeren
    root=fall;      //Navigation setzen
    change=1;       //etwas hat sich geändert!
    fall=1;         //Curser auf nächster Seite an Stelle 1
    Serial.println("a");
  }
}
//Keine Aktionen im Hauptmenü vorhanden

void hauptmenu_handbetrieb(){
  //Handbetrieb Display initialisieren und Pfeilanwahl
  if(change == 1 && root == 001){
    change=0;
    display001();
    displayClearColumn(0);

    //Curseranwahl
    switch(fall){
      case 1: //Zurueck
            u8x8.drawString(0,0, "\x008d");
            break;
      case 2: //Teachpunkt auswaehlen
            u8x8.drawString(0,1, "\x008d");
            break;
      case 3: //Teachpunkt setzen
            u8x8.drawString(0,2, "\x008d");
            break;
      case 4: //GOTO Teachpunkt
            u8x8.drawString(0,5, "\x008d");
            break;
    }
  }

  //Handbetrieb Durchwahl
  if(root == 001 && digitalRead(encoderSW) == 0){
    entprellroutine(encoderSW);
    u8x8.clear();           //LCD für nächsten Ordner leeren
    root=fall*10 + root;    //Navigation setzen, multiplizieren mit 10 für 1. Unterordner
    change=1;               //etwas hat sich geändert!

    //Encoderzählwert auf die gespeicherten Werte abgleichen:
    //Teachpunkte:
    if(root == 21){
      fall_n = teachpunkt;
    }
  }

  //Aktionen bei Durchwahl
  //Zurück 011:
    if(change == 1 && root == 11){
      entprellroutine(encoderSW);
      root=000;     //Zurück zum Hauptmenü schicken
    }

  //Teachpunkt wählen 021
    //Teachpunkt auswaehlen und Wert im Display aktualisieren
    //Encoderwert aktualisierten
    if(change == 1 && root == 21){
      change=0;
      teachpunkt = fall_n;
      display021();
    }
    //Zurück zum übergeordneten Menü wenn Encoder gedrückt wurde
    if(root==21 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW);
      u8x8.clear();     
      root=001;         //Menüanwahl
      fall = 2;         //Cursor zurück an Stelle 2
      change=1;
    }

  //Teachpunkt teachen 031
  if(change == 1 && root == 31){
    change=0;
    //Koordinaten in das dementsprechende Feld schreiben
    teachKoords[0][teachpunkt] = stepperX.getPosition();
    teachKoords[1][teachpunkt] = stepperY.getPosition();
    teachKoords[2][teachpunkt] = stepperZ.getPosition();
    //LCD Meldung "Teachpunkt wurde gesetzt", auf Encoderdrehung warten, dann zurück zum übergeordneten Menü
    display031();
    while(change == 0){}
    root=001;
    u8x8.clear();
    fall=3;
  }

  //GOTO Teachpunkt
  if(change == 1 && root == 41){
    change=0;
    display041();
    //Motoren Stoppen
    while(
      cnt1.isRunning() == 1 ||
      cnt2.isRunning() == 1 ||
      cnt3.isRunning() == 1){
        cnt1.stopAsync();
        cnt2.stopAsync();
        cnt3.stopAsync();
    }
    //Geschindigkeit und Beschleinigung für nicht Manuelle fahrt setzen
    stepperX.setMaxSpeed(speedDefault[0]);
    stepperX.setAcceleration(accelerationDefault[0]);
    stepperY.setMaxSpeed(speedDefault[1]);
    stepperY.setAcceleration(accelerationDefault[1]);
    stepperZ.setMaxSpeed(speedDefault[2]);
    stepperZ.setAcceleration(accelerationDefault[2]);

    //Zielkoordinaten festlegen
    stepperX.setTargetAbs(teachKoords[0][teachpunkt]);
    stepperY.setTargetAbs(teachKoords[1][teachpunkt]);
    stepperZ.setTargetAbs(teachKoords[2][teachpunkt]);

    //Bewegung ausführen lassen
    cnt1.move(stepperX, stepperY, stepperZ);

    //Meldung Angekommen, zurueck wenn Encoder gedreht
    u8x8.clear();
    display041_1();
    delay(20);
    while(change == 0){}
    u8x8.clear();
    root=1;
    fall=4;
  }
}

void hauptmenu_videomodus(){
  //Videomodus Display initialisieren und Pfeilanwahl
  if(change == 1 && root == 2){
    change=0;
    display002();
    displayClearColumn(0);

    //Curseranwahl
    switch(fall){
      case 1: //Zurueck
            u8x8.drawString(0,0, "\x008d");
            break;
      case 2: //Startpunk
            u8x8.drawString(0,1, "\x008d");
            break;
      case 3: //Endpunkt
            u8x8.drawString(0,2, "\x008d");
            break;
      case 4: //Dauer der Fahrt
            u8x8.drawString(0,3, "\x008d");
            break;
      case 5: //Start
            u8x8.drawString(0,4, "\x008d");
            break;
    }
  }

  //Videomodus Durchwahlen
  if(root == 002 && digitalRead(encoderSW) == 0){
    entprellroutine(encoderSW);
    u8x8.clear();           //LCD für nächsten Ordner leeren
    root=fall*10 + root;    //Navigation setzen, multiplizieren mit 10 für 1. Unterordner
    fall=1;                 //Curser auf nächster Seite auf Stelle 1
    fall_n=1;               //Cursor für gegensätzliches Zählen an Stelle 1
    change=1;

    //Encoderzählwert auf die gespeicherten Werte abgleichen:
    //Startpunkt
    if(root == 22){         //Startpunkt
      fall_n = startpunkt;
    }
    //Endpunkt
    else if (root == 32){   //Endpunkt
      fall_n = endpunkt;
    }
  }

  //Aktionen bei Durchwahl
  //Zurück 012
    if(change == 1 && root ==12){
      entprellroutine(encoderSW);
      root=000;       //Zurück zum Hauptmenü schicken
      fall=2;         //Cursor zurück an stelle 2
    }

  //Startpunkt 022
    //Encoderwerte aktualisieren
    if(root== 22 && change == 1){
      change = 0;
      startpunkt = fall_n;
      display022();
    }
    //Zurück wenn encoder gedrückt
    if(root == 22 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW);
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root=002;         //zurück zum Unterordner Handbetrieb
      fall = 2;         //Cursor zurück an stelle 2
      change=1;
    }

  //Endpunkt 032
    //Encoderwerte aktualisieren
    if(change == 1 && root == 32){
      change = 0;
      endpunkt = fall_n;
      display032();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 32 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW);
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root=002;         //zurück zum Unterordner Handbetrieb
      fall = 3;         //Cursor zurück an stelle 3
      change=1;
    }

  //Start 052
    if(change == 1 && root == 52){
      change=0;
      //Berechnungen für Geschindigkeiten in Steps/sek
      long geschVid[3];
      long videoZeit;
      long deltaSteps[3];
      //Dauer der Fahrt berechnen
      videoZeit = gewuenschteS
                  +gewuenschteM * 60
                  +gewuenschteH * 3600;
      //Neue Geschwindigkeit Berechnen
      deltaSteps[0] = teachKoords[0][endpunkt] - teachKoords[0][startpunkt];
      deltaSteps[1] = teachKoords[1][endpunkt] - teachKoords[1][startpunkt];
      deltaSteps[2] = teachKoords[2][endpunkt] - teachKoords[2][startpunkt];
      geschVid[0] = deltaSteps[0] / videoZeit;
      geschVid[1] = deltaSteps[1] / videoZeit;
      geschVid[2] = deltaSteps[2] / videoZeit;
      //Displaymeldung initialisieren
      display052_0();
      //Motoren Stoppen
      while(
        cnt1.isRunning() == 1 ||
        cnt2.isRunning() == 1 ||
        cnt3.isRunning() == 1){
          cnt1.stopAsync();
          cnt2.stopAsync();
          cnt3.stopAsync();
      }
      //Geschindigkeit und Beschleinigung für nicht Manuelle fahrt setzen
      stepperX.setMaxSpeed(speedDefault[0]);
      stepperX.setAcceleration(accelerationDefault[0]);
      stepperY.setMaxSpeed(speedDefault[1]);
      stepperY.setAcceleration(accelerationDefault[1]);
      stepperZ.setMaxSpeed(speedDefault[2]);
      stepperZ.setAcceleration(accelerationDefault[2]);
      //Koordinaten des Startpunktes ansteuern
      stepperX.setTargetAbs(teachKoords[0][startpunkt]);
      stepperY.setTargetAbs(teachKoords[1][startpunkt]);
      stepperZ.setTargetAbs(teachKoords[2][startpunkt]);
      //Startpunkt anfahren
      cnt1.move(stepperX, stepperY, stepperZ);
      //Geschindigkeit und Beschleunigung für Videomodus setzen (Nur wenn auch eine Strecke zu fahren ist, sonst hängt ctr1 in endlosschleife fest!)
      stepperX.setMaxSpeed(geschVid[0]);
      stepperX.setAcceleration(accelerationVideo[0]);
      stepperY.setMaxSpeed(geschVid[1]);
      stepperY.setAcceleration(accelerationVideo[1]);
      stepperZ.setMaxSpeed(geschVid[2]);
      stepperZ.setAcceleration(accelerationVideo[2]);
      //Koordinaten des Endpunktes ansteuern
      stepperX.setTargetAbs(teachKoords[0][endpunkt]);
      stepperY.setTargetAbs(teachKoords[1][endpunkt]);
      stepperZ.setTargetAbs(teachKoords[2][endpunkt]);
      //Bewegung ausführen
      cnt1.moveAsync(stepperX);
      cnt2.moveAsync(stepperY);
      cnt3.moveAsync(stepperZ);
      while(cnt1.isRunning() == 1||
      cnt2.isRunning() == 1 ||
      cnt3.isRunning() == 1){
        delay(20);
      }
      //Meldung fahrt zuende, zurück zum übergeordneten Menü wenn Encoder gedreht wurde
      u8x8.clear();
      display052_2();
      while(change=0);
      root = 2;
      fall = 5;
    }
}

void hauptmenu_videomodus_dauerDerFahrt(){
  //Dauer der Fahrt Display initialisieren und Pfeilanwahl
  if(change == 1 && root == 42){
    change=0;
    display042();
    displayClearColumn(0);
    displayClearRow(5);

    //Curseranwahl
    switch(fall){
      case 1: //Zurueck
            u8x8.drawString(0,0, "\x008d");
            break;
      case 2: //Stunden auswaehlen, andere Pfeilart
            u8x8.drawString(2,5, "\x008e");
            break;
      case 3: //Minuten auswaehlen, andere Pfeilart
            u8x8.drawString(6,5, "\x008e");
            break;
      case 4: //Sekunden auswaehlen
            u8x8.drawString(9,5, "\x008e");
            break;
    }
  }

  //Handbetrieb Durchwahl
  if(root== 42 &&  digitalRead(encoderSW) == 0){
    entprellroutine(encoderSW);
    u8x8.clear();           //LCD für nächsten Ordner leeren
    root=fall*100 + root;    //Navigation setzen, multiplizieren mit 100 für 2. Unterordner
    change=1;

    //Encoderzählwert auf die gespeicherten Werte abgleichen:
    if(root == 242){
      fall_n = gewuenschteH;
    }
    else if (root == 342){
      fall_n = gewuenschteM;
    }
    else if (root == 442){
      fall_n = gewuenschteS;
    }
  }

  //Aktionen bei Durchwahl
  //Zurueck 011:
    if(root==142 && change == 1){
      entprellroutine(encoderSW);
      root=002; 
      fall = 4;
    }

  //Stunden stellen
    if(root==242 && change == 1){
      change = 0;
      gewuenschteH = fall_n;
      display042();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 242 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW); 
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root= 42;         //zurück zum Unterordner Handbetrieb
      fall= 2;         //Cursor zurück an stelle 3
      change=1;
    }

  //Minuten stellen
    if(root==342 && change == 1){
      change = 0;
      gewuenschteM = fall_n;
      display042();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 342 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW); 
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root= 42;         //zurück zum Unterordner Handbetrieb
      fall= 3;         //Cursor zurück an stelle 3
      change=1;
    }

  //Sekunden stellen
    if(root==442 && change == 1){
      change = 0;
      gewuenschteS = fall_n;
      display042();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 442 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW); 
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root= 42;         //zurück zum Unterordner Handbetrieb
      fall= 4;         //Cursor zurück an stelle 3
      change=1;
    }
}

void hauptmenu_referenz(){
  //Referenz Display initialisieren und Pfeilanwahl
  if(change == 1 && root == 4){
    change=0;
    display004();
    displayClearColumn(0);
    entprellroutine(encoderSW);

    //Curseranwahl
    switch(fall){
      case 1: //Zurueck
            u8x8.drawString(0,0, "\x008d");
            break;
      case 2: //X Referenz fahren
            u8x8.drawString(0,1, "\x008d");
            break;
      case 3: //Y-Achse Referenz setzen
            u8x8.drawString(0,2, "\x008d");
            break;
      case 4: //Z-Achse Referenz setzen
            u8x8.drawString(0,3, "\x008d");
            break;
    }
  }

  //Referenz Durchwahl
  if(root== 4 && digitalRead(encoderSW) == 0){
    entprellroutine(encoderSW);
    u8x8.clear();           //LCD für nächsten Ordner leeren
    root=fall*10 + root;    //Navigation setzen, multiplizieren mit 10 für 1. Unterordner
    change=1;               //etwas hat sich geändert!
  }

  //Aktionen bei Durchwahl
  //Zurueck 0014
    if(change == 1 && root == 14){
      entprellroutine(encoderSW);
      root=0;
      fall=4;             //Cursor zurück auf Stelle 4
    }

  //X Achse Referenz Fahren 024
    if(change == 1 && root == 24){
      change=0;
      display024_0();       //Displaymeldung RefFahrt aktiv
      referenzfahrt();      //Funktion Motor Referehnzfahrt ausführen
      display024_1();       //Displaymeldung RefFahrt wurde ausgeführt
      while(change == 0){}  //Warten bis Encoder gedreht wird
      root=4;               //Zurück zum Unterordner
      u8x8.clear();       
      fall =2;              //Cursor an stelle 2
      }

  //Y Achse Referenz Setzen 034
    if(change == 1 && root == 34){
      change=0;
      display034();
      stepperY.setPosition(0);  //Koordinaten der Achsen YZ jetzt auf 0 setzen
      while(change == 0){}      //Warten bis Encoder gedreht wird
      root=4;                   //Zurück zum Unterordner
      u8x8.clear();
      fall=3;                   //Cursor an stelle 3
    }

  //Z Achse Referenz Setzen 044
    if(change == 1 && root == 44){
      change=0;
      display044();
      stepperZ.setPosition(0);  //Koordinaten der Achsen YZ jetzt auf 0 setzen
      while(change == 0){}      //Warten bis Encoder gedreht wird
      root=4;                   //Zurück zum Unterordner
      u8x8.clear();
      fall=4;                   //Cursor an stelle 4
    }
}

void hauptmenu_zeitraffer(){
  //Zeitraffer Display initialisieren und Pfeilanwahl
  if(change == 1 && root == 3){
    change=0;
    display003();
    displayClearColumn(0);

    switch(fall){
      case 1: //Zurueck
            u8x8.drawString(0,0, "\x008d");
            break;
      case 2: //Setup
            u8x8.drawString(0,1, "\x008d");
            break;
      case 3: //Startpunkt
            u8x8.drawString(0,2, "\x008d");
            break;
      case 4: //Endpunkt
            u8x8.drawString(0,3, "\x008d");
            break;
      case 5: //Start
            u8x8.drawString(0,4, "\x008d");
            break;
    }
  }

  //Handbetrieb Durchwahl
  if(root== 003 &&  digitalRead(encoderSW) == 0){
    entprellroutine(encoderSW);
    u8x8.clear();           //LCD für nächsten Ordner leeren
    root=fall*10 + root;    //Navigation setzen, multiplizieren mit 10 für 1. Unterordner
    fall=1;                 //Cursor auf Stelle 1
    change=1;               //etwas hat sich geändert!

    //Encoderzählwert auf die gespeicherten Werte abgleichen:
    if(root==33){
      fall_n = startpunkt;
    }
    else if(root==43){
      fall_n = endpunkt;
    }
  }

  //Aktionen bei Durchwahl
  //Zurueck 013:
    if(change == 1 && root == 13){
      entprellroutine(encoderSW);
      root=000;
      fall = 3;         //Cursor an Stelle 3
    }

  //Startpunk 033
    if(change == 1 && root == 33){
      change=0;
      startpunkt = fall_n;
      display022();     //Gleicher Text wie bei Atomatik
    }
    //Zurück wenn encoder gedrückt
    if(root==33 && digitalRead(encoderSW)== 0){  
      entprellroutine(encoderSW);
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root=003;         //zurück zum Unterordner Handbetrieb
      fall = 3;         //Cursor zurück an stelle 2
      change=1;
    }

  //Endpunkt 043
    if(change == 1 && root == 43){
      change=0;
      endpunkt = fall_n;
      display032();     //Gleicher Text wie bei Atomatik
    }
    //Zurück wenn encoder gedrückt
    if(root==43 && digitalRead(encoderSW)== 0){  
      entprellroutine(encoderSW);  
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root=003;         //zurück zum Unterordner Handbetrieb
      fall = 4;         //Cursor zurück an stelle 2
      change=1;
    }

  //Start 053
    if(root==53 && change == 1){
      change=0;
      display031();
      //Fehlerausgabe falls Schwarzzeit <=0, danach urück zum übergeordneten Menü
      if (schwarzzeit <=0){
        display053_3();
        change=0;
        while(change == 0);
        u8x8.clear();
        fall=5;
        root=3;
      }
      else{
        //Zeitraffer beginnt
        //LCD Meldung
        display052_0();
        //Benötigte Variablen initialisieren
        long deltaStepsZR[3];
        //Motoren Stoppen
        while(
          cnt1.isRunning() == 1 ||
          cnt2.isRunning() == 1 ||
          cnt3.isRunning() == 1){
            cnt1.stopAsync();
            cnt2.stopAsync();
            cnt3.stopAsync();
        }
        //Geschindigkeit und Beschleinigung für nicht Manuelle fahrt setzen
        stepperX.setMaxSpeed(speedDefault[0]);
        stepperX.setAcceleration(accelerationDefault[0]);
        stepperY.setMaxSpeed(speedDefault[1]);
        stepperY.setAcceleration(accelerationDefault[1]);
        stepperZ.setMaxSpeed(speedDefault[2]);
        stepperZ.setAcceleration(accelerationDefault[2]);
        //Koordinaten des Startpunktes ansteuern
        stepperX.setTargetAbs(teachKoords[0][startpunkt]);
        stepperY.setTargetAbs(teachKoords[1][startpunkt]);
        stepperZ.setTargetAbs(teachKoords[2][startpunkt]);
        //Startpunkt anfahren
        cnt1.move(stepperX, stepperY, stepperZ);
        //Zu fahrende Schritte pro Intervall berechnen
        deltaStepsZR[0] = (teachKoords[0][endpunkt] - teachKoords[0][startpunkt]) / (anzahlBilder-1);
        deltaStepsZR[1] = (teachKoords[1][endpunkt] - teachKoords[1][startpunkt]) / (anzahlBilder-1);
        deltaStepsZR[2] = (teachKoords[2][endpunkt] - teachKoords[2][startpunkt]) / (anzahlBilder-1);
        //Zeitrafferschleife beginnen
        //Zeiten initialisieren
        long currentTime = millis();
        long prevTime= millis();  //initialisieren mit sicherheitszeit
        restBilder = anzahlBilder;
        u8x8.clear();
        //Schleife beginnen
        while(restBilder > 1){
          //Aktuelle zeit aktualisieren
          currentTime = millis();
          if(currentTime - prevTime >= intervall){
            //Bild aufnehmen
            remote.click();
            restBilder = restBilder -1;
            //Anzeige Restliche Bilder
            display053_1();
            //Abwarten bis Belichtungszeit und Sicherheitszeit vorbei ist
            while(currentTime - prevTime <= belichtungszeit + sicherheitszeit){
              currentTime=millis();
            }
            //Motoren um deltaStepsZR weiterfahren:
            //Zielposition setzen
            stepperX.setTargetRel(deltaStepsZR[0]);
            stepperY.setTargetRel(deltaStepsZR[1]);
            stepperZ.setTargetRel(deltaStepsZR[2]);
            //Bewegung ausführen
            cnt1.move(stepperX, stepperY, stepperZ);
            //Abwarten bis Schwarzzeit zuende
            while(currentTime - prevTime < intervall){
              currentTime = millis();
            }
            prevTime = currentTime;
          }
        }
        //letztes Bild des Zeitraffers aufnehmen
        remote.click();
        delay(sicherheitszeit+schwarzzeit);
        //Zurück zum Menu wenn Encoder gedreht
        u8x8.clear();
        display053_2();
        change=0;
        while(change==0){};
        u8x8.clear();
        fall=5;
        root=3;
      }
    }
}

void hauptmenu_zeitraffer_setup(){
  //Zeitraffer Setup Display initialisieren und Pfeilanwahl
  if(change == 1 && root == 23){
    change=0;
    //Sonstige Berechnungen zur visuellen Anzeige ausführen
    schwarzzeit= intervall - (belichtungszeit/1000.0) - (sicherheitszeit/1000.0);
    display023();
    displayClearColumn(0);
    displayClearRow(5);

    //Curseranwahl
    switch(fall){
      case 1: //Zurueck
            u8x8.drawString(0,0, "\x008d");
            break;
      case 2: //Anzahl Bilder
            u8x8.drawString(0,1, "\x008d");
            break;
      case 3: //Intervall
            u8x8.drawString(0,2, "\x008d");
            break;
      case 4: //Belichtungszeit
            u8x8.drawString(0,3, "\x008d");
            break;
      case 5: //Sicherheitszeit
            u8x8.drawString(0,4, "\x008d");
            break;
    }
  }

  //Handbetrieb Durchwahl
  if(root== 23 &&  digitalRead(encoderSW) == 0){
    entprellroutine(encoderSW);
    u8x8.clear();             //LCD für nächsten Ordner leeren
    root=fall*100 + root;     //Navigation setzen, multiplizieren mit 10 für 1. Unterordner
    change=1;

    //Encoderzählwert auf die gespeicherten Werte abgleichen:
    if(root == 223){
      fall_n = anzahlBilder/10;
    }
    else if(root== 323){
      fall_n = intervall;
    }
    else if (root == 423){
      fall_n = belichtungszeit/100;
    }
    else if (root == 523){
      fall_n = sicherheitszeit/100;
    }
  }

  //Aktionen bei Durchwahl
  //Zurueck 123:
    if(root==123 && change == 1){
      entprellroutine(encoderSW);
      root=  3;
      fall = 2;
    }

  //Anzahl Bilder 223
    if(root==223 && change == 1){
      change = 0;
      anzahlBilder = fall_n*10;
      display223();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 223 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW); 
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root= 23;         //zurück zum Unterordner Handbetrieb
      fall= 2;          //Cursor zurück an stelle 3
      change=1;
    }

  //Intervall 323
    if(root==323 && change == 1){
      change = 0;
      intervall = fall_n;
      display323();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 323 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW); 
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root= 23;         //zurück zum Unterordner Handbetrieb
      fall= 3;          //Cursor zurück an stelle 3
      change=1;
    }

  //Belichtungszeit 423
    if(root==423 && change == 1){
      change = 0;
      belichtungszeit = fall_n*100;
      display423();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 423 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW); 
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root= 23;         //zurück zum Unterordner Handbetrieb
      fall= 4;          //Cursor zurück an stelle 3
      change=1;
    }

  //Sicherheitszeit 523
    if(root==523 && change == 1){
      change = 0;
      sicherheitszeit = fall_n*100;
      display523();
    }
    //Zurueck wenn encoder gedrückt
    if(root == 523 && digitalRead(encoderSW)== 0){
      entprellroutine(encoderSW); 
      u8x8.clear();     //LCD für nächsten Ordner leeren  
      root= 23;         //zurück zum Unterordner Handbetrieb
      fall= 5;          //Cursor zurück an stelle 3
      change=1;
    }
}

void JoystickControl(){
  //Funktion für den Handbetrieb mit 3-Achs-Joystick
  static int JoyCurrent[3];   //Momentanwert des Joysticks
  static int Hysterese[3];    //Array für zu berechnende Hysterese
  static long Zeit;           //Zeitwert für Timer
  static long prevZeit;       //vorheriger Zeitwert für Timer


  Zeit = millis();            //Timer aktualisieren

  //Folgende Zeilen nur alle x ms ausführen (x=JoyRefresh)
  if(Zeit - prevZeit >= JoyRefresh){
    prevZeit = Zeit;          //vorherigen Zeitwert "mitnehmen/aktualisieren"
    //Analogwerte lesen
    JoyCurrent[0] = analogRead(PotX);
    JoyCurrent[1] = analogRead(PotY);
    JoyCurrent[2] = analogRead(PotZ);
    //Hysteresenwert/2 Berechnen
    for(int i=0;i<=2;i++){       
      Hysterese[i] = 1024*JoyHysterese[i]/200;
    }

    //Motorbewegung für Achse X Linkslauf
    if(JoyCurrent[0] <= JoyMid[0] - Hysterese[0]) {
      stepperX.setMaxSpeed(map(JoyCurrent[0], 0, JoyMid[0]-Hysterese[0], stepperMaxJoy[0]*-1, 0));
      stepperX.setPullInSpeed(PullInSpeed);
      stepperX.setTargetRel(-Increments);
      cnt1.moveAsync(stepperX);
    }
    //Motorbewegung für Achse X Rechtslauf
    else if(JoyCurrent[0] >= JoyMid[0] + Hysterese[0]) {
      stepperX.setMaxSpeed(map(JoyCurrent[0], JoyMid[0]+Hysterese[0],1023, 0, stepperMaxJoy[0]));
      stepperX.setPullInSpeed(PullInSpeed);
      stepperX.setTargetRel(Increments);
      cnt1.moveAsync(stepperX);
    }
    //Keine Motorbewegung wenn Josytick in Ruheposition steht
    else{
      stepperX.setTargetAbs(stepperX.getPosition());
      stepperX.setPullInSpeed(PullIn);
    }

    //Motorbewegung für Achse Y Linkslauf
    if(JoyCurrent[1] <= JoyMid[1] - Hysterese[1]) {
      stepperY.setMaxSpeed(map(JoyCurrent[1], 0, JoyMid[1]-Hysterese[1], stepperMaxJoy[1]*-1, -4));
      stepperY.setPullInSpeed(PullInSpeed);
      stepperY.setTargetRel(Increments);
      cnt2.moveAsync(stepperY);
    }
    //Motorbewegung für Achse Y Rechtslauf
    else if(JoyCurrent[1] >= JoyMid[1] + Hysterese[1]) {
      stepperY.setMaxSpeed(map(JoyCurrent[1], JoyMid[1]+Hysterese[1],1023, 4, stepperMaxJoy[1]));
      stepperY.setPullInSpeed(PullInSpeed);
      stepperY.setTargetRel(-Increments);
      cnt2.moveAsync(stepperY);
    }
    //Keine Motorbewegung wenn Josytick in Ruheposition steht
    else{
      stepperY.setTargetAbs(stepperY.getPosition());
      stepperY.setPullInSpeed(PullIn);
    }

    //Motorbewegung für Achse Z Linkslauf
    if(JoyCurrent[2] <= JoyMid[2] - Hysterese[2]) {
      stepperZ.setMaxSpeed(map(JoyCurrent[2], 0, JoyMid[2]-Hysterese[2], stepperMaxJoy[2]*-1, 0));
      stepperZ.setPullInSpeed(PullInSpeed);
      stepperZ.setTargetRel(Increments);
      cnt3.moveAsync(stepperZ);
    }
    //Motorbewegung für Achse Z Rechtslauf
    else if(JoyCurrent[2] >= JoyMid[2] + Hysterese[2]) {
      stepperZ.setMaxSpeed(map(JoyCurrent[2], JoyMid[2]+Hysterese[2],1023, 0, stepperMaxJoy[2]));
      stepperZ.setPullInSpeed(PullInSpeed);
      stepperZ.setTargetRel(-Increments);
      cnt3.moveAsync(stepperZ);
    }
    //Keine Motorbewegung wenn Josytick in Ruheposition steht
    else{
      stepperZ.setTargetAbs(stepperZ.getPosition());
      stepperZ.setPullInSpeed(PullIn);
    }
  }
}

void HardwareEndlage(){
  //Funktion für das Erreichen der Endlage

  //Motoren unverzüglich ohne Rampe stoppen
  while(digitalRead(endX) == 1){
    cnt1.emergencyStop();
    cnt2.emergencyStop();
    cnt3.emergencyStop();

    //Meldung Endlage erreicht
    u8x8.clear();
    displayEndlage();

    //Auf eingabe am Encoder warten, solange motoren anhalten
    while(digitalRead(encoderSW) == 1);
    //Entprellen
    delay(20);

    //Abwarten bis Encoder losgelassen wurde, entprellen
    while(digitalRead(encoderSW) == 0){}
    delay(20);
    u8x8.clear();
    displayFreifahren();

    //Funktion zum freifahren des Motors, freigefahrene Endlage mit drücken des Encoders bestätigen
    while(digitalRead(encoderSW) == 1){
      JoystickControl();
    }

    //Abwarten bis Encoder wieder losgelassen, dann zurueck zum Hauptmenu
    while(digitalRead(encoderSW) == 0){
    delay(20);
    u8x8.clear();
    change=1;
    root=0;
    }
  }
  
}

void referenzfahrt(){
  //Funktion für die Automatische Referenzfahrt

  //Solange richtung Endschalter fahren, bis dieser betätigt wurde
  stepperX.setMaxSpeed(speedRef[0]);
  stepperX.setAcceleration(accelerationRef[0]);
  cnt1.rotateAsync(stepperX);
  
  //Anhalten sobald Endlage betätigt
  while(digitalRead(endX) == 0);
  cnt1.stop();
  stepperX.setPosition(0);

  //Motor freifahren
  stepperX.setMaxSpeed(speedDefault[0]);
  stepperX.setAcceleration(accelerationDefault[0]);
  stepperX.setTargetAbs(-refOffsetX);
  cnt1.move(stepperX);
  stepperX.setPosition(0);
  refXOK = 1;
}

//Interruptroutine für Encoder
void isrA ()  {
  static unsigned long lastInterruptTime;
  unsigned long interruptTime = millis();

  // If interrupts come faster than 5ms, assume it's a bounce and ignore
  if (interruptTime - lastInterruptTime > 5) {
    change=1;

    if (digitalRead(encoderDT) == 1){
      fall  ++;
      fall_n--;}
    else {
      fall  --;
      fall_n++;} 


    //Encoderwerte werden hier Limitiert
    //Hauptmenu
    switch (root){
      case 0: //Hauptmenu
            fall = min(4, max(1, fall));
            break;
        case 1: //Handbetrieb
              fall = min(4, max(1, fall));
              break;
          case 21: //Teachpunkt auswählen
                fall_n = min(20, max(1, fall_n));
                break;
        case 2: //Videomodus
              fall = min(5, max(1, fall));
              break;
          case 22: //Startpunkt
                fall_n = min(20, max(0, fall_n));
                break;
          case 32: //Endpunkt
                fall_n = min(20, max(0, fall_n));10
                break;
          case 42: //Dauer der Fahrt
                fall = min(4, max(1, fall));
                break;
            case 242: //Dauer der Fahrt Stundeneingabe
                  fall_n = min(24, max(0, fall_n));
                  break;
            case 342: //Dauer der Fahrt Minuteneingabe
                  fall_n = min(60, max(0, fall_n));
                  break;
            case 442: //Dauer der Fahrt Sekundeneingabe
                  fall_n = min(60, max(0, fall_n));
                  break;
        case 3: //Zeitraffer
                fall = min(5, max(1, fall));
                break;
          case 23: // Setup
                fall = min(5, max(1, fall));
                break;
            case 223: //Anzahl Bilder
                  fall_n = min(anzahlBildermax, max(0, fall_n));
                  break;
        case 4: //Referenz Fahren-Setzen
              fall = min(4, max(1, fall));
              break;
    }
  }
  lastInterruptTime = interruptTime;
}

//Die Taste wird zuerst entprellt und der Code wird erst weiter abgearbeitet, sobald die Taste losgelassen wurde.
void entprellroutine(int taste){
  delay(20);
  while(digitalRead(taste) == 0);
}

void displayClearColumn(int spalte){
  for(int i=0;i<=8;i++){
    u8x8.drawString(spalte,i,"\x0020"); 
  }
}

void displayClearRow(int reihe){
  for(int i=0;i<=16;i++){
    u8x8.drawString(i,reihe,"\x0020"); 
  }
}

void displayStartbild(){
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);
  u8x8.begin();
  u8x8.setPowerSave(0);     //Energiesparmodus deaktivieren
  u8x8.drawString(0,0, "Willkommen");
  u8x8.drawString(0,1, "Kameraschlitten");
  u8x8.drawString(0,2, "Dennis");
  u8x8.drawString(0,3, "Bodenm\x00fcller");
  u8x8.drawString(0,5, "Encoder drehen");
}

void displayIniReferenzfahrt(){
  u8x8.drawString(0,0, "Eine Referenz-");
  u8x8.drawString(0,1, "fahrt ist not-");
  u8x8.drawString(0,2, "wendig.");
  u8x8.drawString(0,4, "Encoder d\x00fc\cken");
  u8x8.drawString(0,7, "Zum Abbrechen");
  u8x8.drawString(0,8, "Encoder drehen.");
}

void display000(){
  u8x8.drawString(1,0, "Handbetrieb");
  u8x8.drawString(1,1, "Videomodus");
  u8x8.drawString(1,2, "Zeitraffer");
  u8x8.drawString(1,3, "Referenz");
}

void display001(){
  u8x8.drawString(1,0, "Zur\x00fc\ck");
  u8x8.drawString(1,1, "TP anw\x00e4\hlen");
  u8x8.drawString(1,2, "TP");
  u8x8.setCursor(4,2);
  u8x8.print(teachpunkt);
  u8x8.drawString(7,2, "setzen");
  u8x8.drawString(1,5, "GoTo TP");
  u8x8.setCursor(10,5);
  u8x8.print(teachpunkt);

}

void display021(){
  u8x8.drawString(1,0, "Setze deinen");
  u8x8.drawString(1,1, "Teachpunkt auf");
  u8x8.drawString(1,2, "die Nummer:");
  displayClearRow(4);
  u8x8.setCursor(1,4);
  u8x8.print(teachpunkt);
}

void display041(){
  u8x8.drawString(1,0, "Dein Teachpunkt");
  u8x8.drawString(1,1, "wird angefahren.");
  u8x8.drawString(1,3, "Bitte warten...");
}

void display052_0(){
  u8x8.drawString(1,0, "Dein Startpunkt");
  u8x8.drawString(1,1, "wird angefahren. ");
  u8x8.drawString(0,3, "Bitte warten.");
}

void display052_1(){
  u8x8.drawString(1,0, "Deine Fahrt");
  u8x8.drawString(1,1, "wird");
  u8x8.drawString(1,1, "ausgef\x00fc\rt.");
  u8x8.drawString(1,3, "Bitte warten...");
}

void display052_2(){
  u8x8.drawString(1,0, "Deine Fahrt");
  u8x8.drawString(1,1, "wurde");
  u8x8.drawString(1,2, "ausgef\x00fc\rt. ");
  u8x8.drawString(0,7, "Encoder drehen..");
}

void display041_1(){
  u8x8.drawString(1,0, "Dein Teachpunkt");
  u8x8.drawString(1,1, "wurde erreicht.");
  u8x8.drawString(1,7, "Encoder drehen..");
}

void display031(){
  u8x8.clear();
  u8x8.drawString(1,0, "Dein Teachpunkt");
  u8x8.setCursor(1,1);
  u8x8.print(teachpunkt);
  u8x8.drawString(1,2, "wurde gesetzt!");
  u8x8.drawString(0,7, "Encoder drehen..");
}

void display002(){
  u8x8.drawString(1,0, "Zur\x00fc\ck");
  u8x8.drawString(1,1, "Startpunkt");
  u8x8.drawString(1,2, "Endpunkt");
  u8x8.drawString(1,3, "Dauer d. Fahrt");
  u8x8.drawString(1,4, "Start");
  u8x8.setCursor(13,1);
  u8x8.print(startpunkt);
  u8x8.setCursor(13,2);
  u8x8.print(endpunkt);
}

void display022(){
  u8x8.drawString(1,0, "Setze deinen");
  u8x8.drawString(1,1, "Startpunkt auf");
  u8x8.drawString(1,2, "die Nummer:");
  displayClearRow(4);
  u8x8.setCursor(1,4);
  u8x8.print(startpunkt);
}

void display032(){
  u8x8.drawString(1,0, "Setze deinen");
  u8x8.drawString(1,1, "Endpunkt auf");
  u8x8.drawString(1,2, "die Nummer:");      //ändern auf den "teachpunk"
  displayClearRow(4);
  u8x8.setCursor(1,4);
  u8x8.print(endpunkt);
}

void display042(){
  u8x8.drawString(1,0, "Zur\x00fc\ck ");
  u8x8.drawString(1,1, "Gebe die Dauer");
  u8x8.drawString(1,2, "der Fahrt ein");      
  u8x8.drawString(1,3, "Std:Min:Sek");
  u8x8.drawString(1,6, "   :   :   ");
  u8x8.setCursor(2,6);
  u8x8.print(gewuenschteH);
  u8x8.setCursor(6,6);
  u8x8.print(gewuenschteM);
  u8x8.setCursor(9,6);
  u8x8.print(gewuenschteS);
}

void display242(){
  u8x8.drawString(1,0, "Zur\x00fc\ck ");
  u8x8.drawString(1,1, "Gebe die Dauer");
  u8x8.drawString(1,2, "der Fahrt ein");      
  u8x8.drawString(1,6, "   :   :   ");
  u8x8.setCursor(2,6);
  u8x8.print(gewuenschteH);
  u8x8.setCursor(6,6);
  u8x8.print(gewuenschteM);
  u8x8.setCursor(9,6);
  u8x8.print(gewuenschteS);
  u8x8.drawString(1,7, "Std:Min:Sek");
}

void display003(){
  u8x8.drawString(1,0, "Zur\x00fc\ck");
  u8x8.drawString(1,1, "Setup");
  u8x8.drawString(1,2, "Startpunkt");
  u8x8.setCursor(12,2);
  u8x8.print(startpunkt);
  u8x8.drawString(1,3, "Endpunkt");
  u8x8.setCursor(12,3);
  u8x8.print(endpunkt);
  u8x8.drawString(1,4, "Start");
}

void display023(){
  u8x8.drawString(1,0, "Zur\x00fc\ck");
  u8x8.drawString(1,1, "Anzahl Bilder");
  u8x8.drawString(1,2, "Intervall");
  u8x8.drawString(1,3, "Verschlussz.");
  u8x8.drawString(1,4, "Sicherheitsz.");
  u8x8.drawString(1,7, "SwZeit:");
  u8x8.setCursor(12,7);
  u8x8.print(schwarzzeit);
}

void display223(){
  u8x8.drawString(1,0, "Zahl der auf-");
  u8x8.drawString(1,1, "zunehmenden");
  u8x8.drawString(1,2, "Bilder:");
  displayClearRow(4);
  u8x8.setCursor(1,4);
  u8x8.print(anzahlBilder);
}

void display323(){
  u8x8.drawString(1,0, "Intervall");
  u8x8.drawString(1,1, "in Sekunden:");
  u8x8.drawString(1,2, "");
  displayClearRow(4);
  u8x8.setCursor(1,4);
  u8x8.print(intervall);
}

void display423(){
  u8x8.drawString(1,0, "Belichtungszeit");
  u8x8.drawString(1,1, "in Milli-:");
  u8x8.drawString(1,2, "Sekunden");
  displayClearRow(4);
  u8x8.setCursor(1,4);
  u8x8.print(Belichtungszeit);
}

void display523(){
  u8x8.drawString(1,0, "Sicherheitszeit");
  u8x8.drawString(1,1, "in Milli-:");
  u8x8.drawString(1,2, "Sekunden");
  displayClearRow(4);
  u8x8.setCursor(1,4);
  u8x8.print(sicherheitszeit);
}

void display004(){
  u8x8.drawString(1,0, "Zur\x00fc\ck");
  u8x8.drawString(1,1, "X-Ref. fahren");
  u8x8.drawString(1,2, "Y-Ref. setzen");
  u8x8.drawString(1,3, "Z-Ref. setzen");
}

void display034(){
  u8x8.drawString(1,0, "Die Y-Achse");
  u8x8.drawString(1,1, "wurde soeben");
  u8x8.drawString(1,2, "referenziert!");
  u8x8.drawString(0,7, "Encoder drehen..");
}

void display044(){
  u8x8.drawString(1,0, "Die Z-Achse");
  u8x8.drawString(1,1, "wurde soeben");
  u8x8.drawString(1,2, "referenziert!");
  u8x8.drawString(0,7, "Encoder drehen..");
}

void display024_0(){
  u8x8.drawString(1,0, "Eine Referenz-");
  u8x8.drawString(1,1, "fahrt der X-");
  u8x8.drawString(1,2, "Achse wird aus-");
  u8x8.drawString(1,3, "geführt");
  u8x8.drawString(1,5, "Bitte warten.");
}
void display024_1(){
  u8x8.clear();
  u8x8.drawString(1,0, "Referenzfahrt");
  u8x8.drawString(1,1, "erfolgreich!");
  u8x8.drawString(1,7, "Encoder drehen...");
}

void display053_1(){
  u8x8.drawString(1,0, "Zeitrafferauf-");
  u8x8.drawString(1,1, "nahme begonnen.");
  u8x8.drawString(1,3, "Restliche Bil-");
  u8x8.drawString(1,4, "der");
  displayClearRow(5);
  u8x8.setCursor(4,5);
  u8x8.print(restBilder);
}
void display053_2(){
  u8x8.drawString(1,0, "Zeitrafferauf-");
  u8x8.drawString(1,1, "nahme beendet.");
  u8x8.drawString(1,7, "Encoder drehen..");
}

void display053_3(){
  u8x8.drawString(1,0, "Fehler!");
  u8x8.drawString(1,1, "Schwarzzeit <=0");
  u8x8.drawString(0,3, "Encoder drehen..");
  u8x8.drawString(1,7, "SwZeit:");
  u8x8.setCursor(12,7);
  u8x8.print(schwarzzeit);
  u8x8.drawString(1,9, "Vorsicht!");
  u8x8.drawString(1,11, "Bei einer Sw-");
  u8x8.drawString(1,12, "Zeit <=0 werden");
  u8x8.drawString(1,13, "keine Bewegung-");
  u8x8.drawString(1,14, "en ausge-");
  u8x8.drawString(1,15, "f\x00fc\hrt!");
}

void displayEndlage(){
  u8x8.drawString(0,0, "Achtung! End-");
  u8x8.drawString(0,1, "lage erreicht.");
  u8x8.drawString(0,2, "Bitte mit Joy-");
  u8x8.drawString(0,3, "stick frei-");
  u8x8.drawString(0,4, "fahren.");

  u8x8.drawString(0,6, "Joystick durch");
  u8x8.drawString(0,7, "dr\x00fc\cken des En-");
  u8x8.drawString(0,8, "coders akti-");
  u8x8.drawString(0,9, "vieren.");
}

void displayFreifahren(){
  u8x8.drawString(0,0, "Achtung! End-");
  u8x8.drawString(0,1, "lage erreicht.");
  u8x8.drawString(0,2, "Bitte mit Joy-");
  u8x8.drawString(0,3, "stick frei-");
  u8x8.drawString(0,4, "fahren");
  u8x8.drawString(0,6, "Joystick ");
  u8x8.drawString(0,7, "aktiv!");
  u8x8.drawString(0,9, "Wenn freigefahren");
  u8x8.drawString(0,10, "mit Encoder be-");
  u8x8.drawString(0,11, "st\x00e4\tigen.");
}
