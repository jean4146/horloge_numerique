/**
 * @email : openprogramming23@gmail.com
 * @Auteur : Exaucé KIMBEMBE
 * @Modifié par : Gemini Collaborator
 * @Date : 2026
 * * @Board : ARDUINO UNO
 */

// Vérification de la carte Arduino 
#ifndef __AVR__
  #error "Ce programme a été testé sur une carte Arduino"
#endif

#define LCD_I2C true
#define COL 16 // Nombre de colonnes de l'écran
#define ROW 2  // Nombre de lignes de l'écran

// Paramètres du moteur pas à pas
#define VITESSE_MAX 120  // 120 tr/min
#define VITESSE_MIN 30   
#define PAS         400  // Nombre de pas pour un tour complet

#include <Stepper.h> // Bibliothèque officielle Arduino

// Déclaration de l'objet LiquidCrystal et du Moteur
#if LCD_I2C == true
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(0x27, COL, ROW);
  Stepper moteur(PAS, 6, 7, 8, 9); 
#else
  #include <LiquidCrystal.h>
  #define RS 6
  #define E  7
  #define D4 8
  #define D5 9
  #define D6 10
  #define D7 11
  LiquidCrystal lcd(RS, E, D4, D5, D6, D7);
  Stepper moteur(PAS, 12, 13, 22, 23);
#endif

// Pins utilisés pour les boutons
#define PIN_BNT_O_F  2 // Bouton marche/arrêt
#define PIN_BNT_OK   3 // Bouton OK
#define PIN_BNT_UP   4 // Bouton up 
#define PIN_BNT_DOWN 5 // Bouton down

// Menu de démarrage (Français)
#define ACCEUIL_l1  "   BIENVENUE CLIENT "
#define ACCEUIL_l2  "COMMANDE VITESSE "

// Menu de sélection (Français)
#define MENU_1  "-> Voir vitesse "
#define MENU_2  "-> Regler vit.  "
#define MENU_3  "-> Voir le sens "
#define MENU_4  "-> Changer sens "

String tab_menu[4] = { MENU_1, MENU_2, MENU_3, MENU_4 };

uint8_t vitesse = VITESSE_MAX;   // vitesse de rotation

bool state_moteur = false; // Indique si le moteur est en marche
bool sens         = true;  // sens par défaut (true = horaire, false = anti-horaire)
uint8_t index_option = 0;  // Index de l'option à afficher
uint8_t tab_af[2] = {0, 1}; // Tableau contenant l'index du menu à afficher
uint8_t ligne_cursor = 0;  // Position du curseur sur l'écran 

#define DELAY 3000 // Pause système setup() (ms)

void menu(void);
bool detectionAppui(uint8_t pin);
void gestionBouton_O_F(void); 
void gestionBouton_OK(void);
void gestionBouton_UP(void);
void gestionBouton_DOWN(void);
void revolution(bool sensDeRotation); 
void setSpeed(void);
void getDirection(void);

void setup(){
  Serial.begin(115200);

  // Configuration de l'écran LCD
  #if LCD_I2C == true
    lcd.init();
    lcd.backlight();
  #else
    lcd.begin(COL, ROW);
  #endif

  lcd.blink();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(ACCEUIL_l1);
  lcd.setCursor(0, 1);
  lcd.print(ACCEUIL_l2);  

  // Configuration des boutons
  pinMode(PIN_BNT_O_F, INPUT_PULLUP);
  pinMode(PIN_BNT_OK,   INPUT_PULLUP);
  pinMode(PIN_BNT_UP,   INPUT_PULLUP);
  pinMode(PIN_BNT_DOWN, INPUT_PULLUP);

  // Configuration initiale de la vitesse du moteur
  moteur.setSpeed(vitesse);

  delay(DELAY);

  // --- PHASE D'INITIALISATION (Moteur tourne pendant 5 secondes) ---
  lcd.clear();
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Initialisation..");
  lcd.setCursor(0, 1);
  lcd.print("Moteur actif 5s ");

  unsigned long tempsDebutInit = millis();
  // Boucle bloquante pendant exactement 5000 millisecondes
  while(millis() - tempsDebutInit < 5000) {
    // Fait tourner le moteur à la vitesse max définie pendant l'initialisation
    moteur.step(10); 
  }
  // -----------------------------------------------------------------

  // Configuration de l'interruption (activée seulement APRES l'initialisation)
  attachInterrupt(digitalPinToInterrupt(PIN_BNT_O_F), gestionBouton_O_F, FALLING);

  lcd.blink();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(tab_menu[tab_af[0]]);
  lcd.setCursor(0, 1);
  lcd.print(tab_menu[tab_af[1]]);
}

void loop(){
  if(state_moteur == true){
    Serial.println("moteur en marche");
    revolution(sens);
  }
  else{
    menu();
    gestionBouton_OK();
    gestionBouton_DOWN();
    gestionBouton_UP();
  }
}

//---------------- Gestion des boutons
void gestionBouton_O_F(void){
  state_moteur = !state_moteur;
  if(state_moteur == false){
    Serial.println("moteur off");
  }
}

void gestionBouton_OK(void){
  if (detectionAppui(PIN_BNT_OK) == true){
    Serial.println("Le bouton ok est cliqué");

    // Affiche la vitesse actuelle
    if(index_option == 0){
      lcd.setCursor(0, 0);
      lcd.print("Vitesse actuelle");
      lcd.setCursor(0, 1);
      lcd.print("N = " + String(vitesse) + " tr/min     ");
      while(true){
        if (detectionAppui(PIN_BNT_OK) == true){ break; }
      }
    }
    // Modifie la vitesse actuelle
    else if(index_option == 1){ setSpeed(); }
    // Affiche le sens de rotation du moteur
    else if(index_option == 2){
      getDirection();
      while(true){
        if (detectionAppui(PIN_BNT_OK) == true){ break; }
      }
    }
    // Modifie le sens de rotation du moteur
    else if(index_option == 3){
      sens = !sens; 
      getDirection();
      delay((int)DELAY / 2);
    }
  }
}

void gestionBouton_DOWN(void){
  if (detectionAppui(PIN_BNT_UP) == true){
    if(index_option >= 1){
      --index_option;
      Serial.println(index_option);   
    }
  }
}

void gestionBouton_UP(void){
  if (detectionAppui(PIN_BNT_DOWN) == true){
    if(index_option < ((int)(sizeof(tab_menu) / sizeof(tab_menu[0]))) - 1){
      ++index_option;
      Serial.println(index_option);
    }
  }
}

bool detectionAppui(uint8_t pin){
  if(!digitalRead(pin) == true){
    delayMicroseconds(200);
    while(!digitalRead(pin) == true){}
    if(!digitalRead(pin) == false){
      return true;
    }
  }
  return false;
}

void menu(void){
  static unsigned long _init = millis();

  if(index_option == 0 || index_option == 1){
    tab_af[0] = 0;
    tab_af[1] = 1;
  }
  else if(index_option == 2 || index_option == 3){
    tab_af[0] = 2;
    tab_af[1] = 3;
  }
  
  if((millis() - _init) >= 500){
    lcd.setCursor(0, 0);
    lcd.print(tab_menu[tab_af[0]]);
    lcd.setCursor(0, 1);
    lcd.print(tab_menu[tab_af[1]]);
    _init = millis();
  }

  if(index_option == 0 || index_option == 2)
    lcd.setCursor(1, 0);
  else
    lcd.setCursor(1, 1);
}

void revolution(bool sensDeRotation){
  if(sensDeRotation == true){
    moteur.step(10); 
  } else {
    moteur.step(-10); 
  }
}

void setSpeed(void){
  unsigned long _init = millis();
  lcd.setCursor(0, 0);
  lcd.print("Regl -> Vitesse ");
  
  while(true){ 
    if(detectionAppui(PIN_BNT_UP) == true){
      if(vitesse < VITESSE_MAX)
       ++vitesse;
    }
    if(detectionAppui(PIN_BNT_DOWN) == true){
      if(vitesse > VITESSE_MIN)
       --vitesse;
    }

    if((millis() - _init) >= 500){
      lcd.setCursor(0, 1);
      lcd.print("N = " + String(vitesse) + " tr/min    ");
      _init = millis();
    }

    if(detectionAppui(PIN_BNT_OK) == true){
      moteur.setSpeed(vitesse); 
      break;
    }
  }
}

void getDirection(void){
  lcd.setCursor(0, 0);
  lcd.print(" Sens actuel    ");
  lcd.setCursor(0, 1);
  if(sens == true)
    lcd.print("-> Horair(Clock)");
  else
    lcd.print("-> Anti-Horaire ");
}