#include <MySomfy.h>

MySomfy MySomfy(A5); //Instanciation de MySomfy avec le port TX utilisé
int rcTelecommande[3] = {97, 42, 856};
int telecommande;

void setup() {
  Serial.begin(9600);
  MySomfy.telecommande(0xBC, 0xDB, 0xCD);
  pinMode(4, OUTPUT);
}

void loop() {

}
//*
void serialEvent() {
  if(Serial.available()){
    String entreeSerie = Serial.readString();
    String commande = entreeSerie.substring(0,3);
    String valeur = entreeSerie.substring(3);
    
    if(commande == "tel"){
      cfgTel(valeur.toInt());
    }
    if(commande == "act"){
      digitalWrite(4, HIGH);
      char retvaleur = valeur[0];
      rcTelecommande[telecommande]++;
      MySomfy.action(retvaleur, rcTelecommande[telecommande]);
      digitalWrite(4, LOW);
    }
    if(commande == "lst"){
      menu();
    }
  }
}


void menu(){
  Serial.println("1 - Couloir");
  Serial.println("2 - Chambre 1");
  Serial.println("3 - Chambre 2");
}

void cfgTel(int valeur){
  switch (valeur) {
    case 1:
      MySomfy.telecommande(0xBC, 0xDB, 0x01);
      telecommande = valeur;
      break;
    case 2:
      MySomfy.telecommande(0xBC, 0xDB, 0x02);
      telecommande = valeur;
      break;
    case 3:
      MySomfy.telecommande(0xBC, 0xDB, 0x03);
      telecommande = valeur;
      break;
    default :
      Serial.println("Telecommande inexistante reportez vous aux menu ci-dessous : ");
      menu();
    break;
  }
  
}
//*//
