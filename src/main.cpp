#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include "rgb_lcd.h"
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>

rgb_lcd lcd;
// ---------------configuration des pin ---------------
#define bpbleu 27
#define bpvert 26
#define pot 32
#define capteur_ExG 35
#define capteur_InG 34
#define capteur_ExD 36
#define capteur_InD 39
#define GPIO_D 18
#define GPIO_G 16
#define pwmD 19
#define pwmG 17
//---------------------------------------
//------------------configuration pour le pwm------------------
#define frequence 20000
#define resolution 10
#define canal_D 1
#define canal_G 0
//---------------------------------------


void initialisation(void); // pour configurer les paramètres et faire la recherche de min et max 
void suivi_de_ligne(int vitesse, float Cp, float KD); // pour le suivi de ligne 
void analyse_couleur(void); // pour analyser les couleurs des balles 
void calib(int num_capteur,int capteur); // recherche de min et max 
  int normalisation(int capteur,int num_capteur); // normalisation entre 0 et 100

Adafruit_TCS34725 MonCapteur = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);
//------------------Modbus-----------------------------
uint16_t reg = 0;
ModbusIP mb;
IPAddress plcIP(192, 168, 0, 102);
//-----------------------------------------------------

void setup()
{
  Serial.begin(115200);
  lcd.begin(16, 2); // initialisation de l'ecran lcd
  pinMode(bpvert, INPUT_PULLDOWN); // mettre bp vert en entre en pull_down
  pinMode(bpbleu, INPUT_PULLDOWN);// mettre bp bleu en entre en pull_down 
  pinMode(GPIO_G, OUTPUT); // mettre le pin enable du hacheur en sortie 
  pinMode(GPIO_D, OUTPUT);// mettre le pin enable du hacheur en sortie
  ledcSetup(canal_D, frequence, resolution); // configuration du pwm
  ledcSetup(canal_G, frequence, resolution); // configuration du pwm 
  ledcAttachPin(pwmD, canal_D);
  ledcAttachPin(pwmG, canal_G);
  digitalWrite(GPIO_G, HIGH); // activer le GPIO_G 

  //-------------------capteur de couleur----------------------
  if (MonCapteur.begin())
  {
    // Initialisation du capteur de couleurs
    Serial.println("Capteur connecté"); // Affichage d'un message de succès si le capteur est trouvé
  }
  else
  {
    Serial.println("Problème de connexion au capteur de couleurs"); // Sinon affichage d'un message d'erreur
    while (1)
      ; // Arrêter le programme si le capteur n'est pas trouvé
  }
  //----------------------------------------------------------

  //------------------------partie wifi---------------------------
  WiFi.begin("TP-Link_B822", "64409092");
  Serial.print("Connexion WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32 IP :");
  Serial.println(WiFi.localIP());
  //------------------------------------------------------------------
  //--------------------MODBUS----------------------------------------
  mb.server();
  mb.addCoil(40000, 0); // devient true lorsque le bras  finit une  action
  mb.addCoil(40001, 0); //  false : mauvaise couleur , true : bonne couleur
}

int val_bpb; // lecture du bouton bleu
int val_bpv; // lecture du bouton vert 
int potar; // lecture du potentiomètre
// ------------------lecture des 4 capteurs----------------//
int c_Exg;
int c_Ing;
int c_Exd;
int c_Ind;
//---------------------------------------------------------
int etape; // machine à état géneral du projet 
int PWMG, PWMD; 

int debut = 0; // machine à état pour l'initialisation 
//-----------------La partie suiveur de ligne-------------
int v_base; // vitesse 
float Co, Kd; 
int pos_Z, pos_Z_prec = 0, derive;
float erreur_prop;
float erreur_derive;
float Ec;
//--------------------------------------------------
int temps; // pour faire uun timer 
bool choix_couleur = false; // false =rouge , true = bleu

//-------------partie calibration---------------------
int c_max[4]={0,0,0,0}; 
int c_min[4]={4095,4095,4095,4095};
//-----------------------------------------------


uint16_t r, g, b, c; // variables pour récupérer les valeurs du capteur de couleur

void loop()
{
  mb.task();
//------------------------lecture des etats des boutons---------------------
  val_bpb = digitalRead(bpbleu); 
  val_bpv = digitalRead(bpvert);
  //-----------------------------------
  potar = analogRead(pot);// lecutre des valeurs du potentiometre

//------------------------normalisation des capteurs----------------
 if (etape!=0) // l'etape 0 correspond à la calibration
 {
  c_Exd=normalisation(capteur_ExD,0);
  c_Ing=normalisation(capteur_InG,3);
  c_Ind=normalisation(capteur_InD,2);
  c_Exg=normalisation(capteur_ExG,1);
 }
 //-------------------------------------------------------
 //---------------------------machine à etat principal-----------------------
  switch (etape)
  {
  case 0: // configuration des paramètres
    ledcWrite(canal_D, 0);// pour éteindre les moteurs 
    ledcWrite(canal_G, 0);// pour éteindre les mmoteurs 

    initialisation(); //reglave vitesse , c0,kd , calibration des capteurs et le choix de la bonne couleur de balle 

    break;
  case 1: // attente d'une balle
    lcd.setColorWhite(); // mettre la couleur de l'ecran en blanc 
    ledcWrite(canal_D, 0);
    ledcWrite(canal_G, 0);
    analyse_couleur(); // analyse de la couleur 
    lcd.setCursor(0, 0);
    lcd.printf("Attente");

     if (millis() - temps > 5000) // attente de 5s 
    {
      if (c < 400) // la balle est sur le support donc moins de lumière
      {
        temps = millis();
        lcd.clear();
        etape = 2; // vérification de la couleur de la balle 
      }
    }

    break;
  case 2: // verification de la couleur de la balle
    analyse_couleur();

      if (choix_couleur == false) // bonne couleur: rouge
      {
        if ((b > r) && (b > g)) // mauvaise couleur de balle
        {

          lcd.setColor(BLUE); // mettre la couleur de l'ecran en bleu 
          lcd.setCursor(0, 0);
          lcd.printf("bleu  ");
          mb.Coil(40001, false);
          etape = 3;
        }
        else if ((r > b) && (r > g)) // bonne couleur de balle
        {

          lcd.setColor(RED);
          lcd.setCursor(0, 0);
          lcd.printf("rouge  ");
          mb.Coil(40001, true);
          etape = 5;
        }
      }
      else // bonne couleur:bleu
      {
        if ((b > r) && (b > g)) // bonne couleur de balle
        {

          lcd.setColor(BLUE);
          lcd.setCursor(0, 0);
          lcd.printf("bleu  ");
          mb.Coil(40001, true); // pour dire à l'automate que c'est la bonne couleur 
          etape = 5; 
        }
        else if ((r > b) && (r > g)) // mauvaise couleur de balle
        {

          lcd.setColor(RED);
          lcd.setCursor(0, 0);
          lcd.printf("rouge  ");
          mb.Coil(40001, false);
          etape = 3;
        }
      }
    break;
  case 3: // attente pour que les capteurs ne soient pas sur le trait 
    suivi_de_ligne(v_base, Co, Kd);
    if ((c_Exd < 20) && (c_Exg < 20))
    {
      etape = 4;
    }

    break;

  case 4: // attente du prochain trait
    suivi_de_ligne(v_base, Co, Kd);

    if ((c_Exd > 65) && (c_Exg > 65)) // les capteurs extérieurs capttent ensemble 
    {
      temps = millis();
      lcd.clear();
      etape = 6;
    }
    break;

  case 5:// le  bras prend la balle
    analyse_couleur(); // à enlever lorsque ca fonctionne vrmt
    lcd.setCursor(0, 0);
    lcd.printf("bras");

    // if ((c > 500)/*à enlever lorsque l'automate fonctionne*/ ) // l'automate met le mb.Coil(40001) à true apres que le bras ait fini de prendre la balle
    if (mb.Coil(40000)==true)
    {
      lcd.clear();
      mb.Coil(40000, false);
      temps=millis();
      etape = 1;
    }
    break;
  case 6: // le zumo s'arrête et le bras prend la balle
    ledcWrite(canal_D, 0);
    ledcWrite(canal_G, 0);
    lcd.setCursor(0, 0);
    lcd.printf("bras");
    analyse_couleur();
    if (millis() - temps > 3000) // attente de 3 s aavant de lire les variables du capteur des couleurs 

    {
      if (c > 500) // la balle a été prise
      {
        lcd.clear();
        etape = 7;
      }
    }
    break;
  case 7: // on repart 
    suivi_de_ligne(v_base, Co, Kd);
    if ((c_Exd < 20) && (c_Exg < 20))
    {
      etape = 8;
    }

    break;
  case 8: // le zumo revient sur notre staion de tri
    suivi_de_ligne(v_base, Co, Kd);
    if ((c_Exd > 65) && (c_Exg > 65)) // les capteurs extérieurs captent 
    {
      lcd.setColorWhite();
      lcd.clear();
      etape = 1; // on retourne dans l'etape pour attenre la balle 
    }
    break;


  default:
    etape = 0;
    break;
  }
  if ((val_bpb == HIGH) && (val_bpv == LOW)) // arret 
  {
    etape = 0;
  }
  lcd.setCursor(0, 1);
  lcd.printf("etape=%1d", etape);
}
//----------------------------------------

void initialisation(void)
{

  switch (debut)
  {
  case 0: // réglage de C0
    Co = 4.55;
    lcd.setCursor(0, 0);
    lcd.printf("C0=%.2f", Co);
    if (val_bpb == HIGH)
    {
      lcd.clear();
      debut = 1;
    }
    break;
  case 1: // reglage de kd
    Kd = 1.05;
    lcd.setCursor(0, 0);
    lcd.printf("Kd=%.2f", Kd);

    if (val_bpv == HIGH)
    {
      lcd.clear();
      debut = 2;
    }
    break;
  case 2: // reglage de la vitesse 
    v_base = 400;
        calib(0,capteur_ExD);
    calib(1,capteur_ExG);
    calib(2,capteur_InD);
    calib(3,capteur_InG);
    lcd.setCursor(0, 0);
    lcd.printf("vbase=%4d",  v_base);
        lcd.setCursor(1, 0);
    lcd.printf("calib");

    if (val_bpb == HIGH)
    {
      lcd.clear();
      // etape = 10;
      debut = 3;
    }

    break;
  case 3: // réglage du choix des couleurs 
    if (val_bpb == HIGH)
    {
      choix_couleur = !choix_couleur;

      delay(500);
    }
    if (choix_couleur == false) 
    {
      lcd.setCursor(0, 0);
      lcd.printf("balle rouge", choix_couleur);
    }
    else
    {
      lcd.setCursor(0, 0);
      lcd.printf(" balle bleu", choix_couleur);
    }

    if (val_bpv == HIGH)
    {
      lcd.clear();
      temps = millis();
      etape = 1; // on va directement dans l'etape pour attendre une balle
      debut = 4;
    }
    break;

  default:
    debut = 0;
    break;
  }
}
void suivi_de_ligne(int vitesse, float Cp, float KD) // fonction pour le suivi de ligne 
{
  pos_Z = c_Ing - c_Ind;
  derive = pos_Z - pos_Z_prec;
  erreur_prop = (float)(pos_Z)*Cp;
  erreur_derive = KD * (float)(derive);
  Ec = erreur_prop + erreur_derive;
  pos_Z_prec = pos_Z;

  PWMG = (int)(vitesse - Ec);
  PWMD = (int)(vitesse + Ec);
  //-----------------pour ne pas dépasser la valeur max et la valeur min
  if (PWMG > 1023)
  {
    PWMG = 1023;
  }
  if (PWMD > 1023)
  {
    PWMD = 1023;
  }
  if (PWMD < 0)
  {
    PWMD = 0;
  }
  if (PWMG < 0)
  {
    PWMG = 0;
  }
  //------------------------------------------------------------

  ledcWrite(canal_G, PWMG);
  ledcWrite(canal_D, PWMD);
}

void analyse_couleur(void) // fonction pour analyser les couleurs 
{
  MonCapteur.getRawData(&r, &g, &b, &c); // Lecture des données brutes du capteur de couleurs
  Serial.printf("R: %d G: %d B: %d C: %d \r", r, g, b, c); //  pour afficher les valeurs sur le moniteur de Vs studio 
}
void calib(int num_capteur, int capteur) // recherche de min et max pour chaque capteur
{
int v = analogRead(capteur);
  if (v > c_max[num_capteur]) c_max[num_capteur] = v;
  if (v < c_min[num_capteur]) c_min[num_capteur] = v;
}
int normalisation(int capteur,int num_capteur) // normalisation entre 0 et 100
{
  return  100 - (float)((analogRead(capteur) - c_min[num_capteur]) * 100.0) / (c_max[num_capteur] - c_min[num_capteur]);

}
