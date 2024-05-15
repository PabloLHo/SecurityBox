#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <StreamString.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include "LiquidCrystal_I2C.h"

#define countof(a) (sizeof(a) / sizeof(a[0]))


// SERVIDOR //
const char* ssid = "OnePlus 8";
const char* pass = "k84wggeb";
IPAddress IP(192, 168, 104, 190);
IPAddress gateway(192, 168, 1, 1);
IPAddress mask(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);

ESP8266WebServer Server(80);

String passphrase = "c3c0ee1dc1fd04a053fa255f484837df94004a7a";
bool permisoConcedido;
bool puertaAbierta = false;

char ultimoAcceso[20];
String acceso;
bool accesoFraudulento; // Cuando se han abierto la caja
bool intentoAccesoFraudulento; // Cuando se ha intentado abrir la caja pero no se ha podido --> acelerometro activado

// PANTALLA //
LiquidCrystal_I2C LCD(0x27, 8, 10);
byte KEY_ICON[] = {
  B01110,
  B10001,
  B01110,
  B01110,
  B00100,
  B01100,
  B00100,
  B01100
};

byte UNLOCKED[] = {
  B01110,
  B01010,
  B01000,
  B01000,
  B11111,
  B11011,
  B11011,
  B11111
};

byte LOCKED[] = {
  B01110,
  B01010,
  B01010,
  B01010,
  B11111,
  B11011,
  B11011,
  B11111
};

// RELOJ //
const int CLK = 12;	// D6
const int DAT = 13;	// D7
const int RST = 15;	// D8
ThreeWire wire(DAT, CLK, RST);
RtcDS1302<ThreeWire> rtc(wire);

// LED PERMISO //
const int LED = 0;

void accesoConcedido() {
	LCD.clear();
	LCD.setCursor(5, 0); LCD.print("Abierto");
	LCD.setCursor(3, 1); LCD.print("Bienvenido!!!");
	LCD.setCursor(0, 0); LCD.write(0);
	LCD.setCursor(1, 0); LCD.write(2);
}

void actualizarUltimoAcceso() {
	RtcDateTime _now = rtc.GetDateTime();
	// char _date[20];
	snprintf_P(ultimoAcceso, countof(ultimoAcceso), PSTR("%02u/%02u/%04u %02u:%02u:%02u"), _now.Day(), _now.Month(), _now.Year(), _now.Hour(), _now.Minute(), _now.Second());
	acceso = ultimoAcceso;
  printDateTime(_now);
}

void actualizarPantallaAcceso(){
	LCD.clear();
	LCD.setCursor(1, 0); LCD.print("Ultimo acceso");
	LCD.setCursor(3, 1); LCD.print(ultimoAcceso);
	LCD.setCursor(0, 1); LCD.write(1);
}

void handlePermiso() {
	StreamString salida;
	salida.reserve(500);
	salida.printf("\
		<html>\
		  <head>\
			<title>Acceso</title>\
			<style>\
			  body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
			</style>\
		  </head>\
		  <body>\
			<h2>Clave secreta</h2>\
			<form action=\"acceso\" method=\"post\">\
				<input type=\"text\" name=\"clave\">\
				<input type=\"submit\" value=\"ENVIAR\">\
			</form>\
		  </body>\
		</html>"
	);
	Server.send(200, "text/html", salida.c_str());
}

void handleAcceso() {
	String clave = Server.arg("clave");
	if (clave == "KIT10") {
		Serial.println("Clave aceptada!");
    permisoConcedido = true;
    digitalWrite(LED, HIGH);
	} else {
    Serial.println("Clave rechazada!");
  }
	handlePermiso();
}

void handlePermisoConcedido(){
  if(permisoConcedido)
    Server.send(200,"text/plain", "O");
  else 
    Server.send(200,"text/plain", "X");
}

void handleHistorial() {
	StreamString salida;
	salida.reserve(500);
	
	String accesoS;
	if (accesoFraudulento) {
		accesoS = "<h3 style=\"color:red\">Acceso no autorizado: " + acceso + "</h3>";
	} else if (intentoAccesoFraudulento) {
		accesoS = "<h3 style=\"color:red\">Intento de acceso no autorizado: " + acceso + "</h3>";
	} else {
		accesoS = "<h3>Ultimo acceso: " + acceso + "</h3>";
	}


  salida.printf("<html>\
    <head>\
      <title>Ultimo acceso</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
      </style>\
    </head>\
    <body>\
      %s\
    </body>\
  </html>", accesoS.c_str());

	Server.send(200, "text/html", salida.c_str());

}

void handleApertura() {
	if (Server.method() != HTTP_POST) {
		Server.send(405, "text/plain", "Metodo no soportado");
	} else if (!passphrase.compareTo(Server.arg(0))) {
		Server.send(405, "text/plain", "Operacion denegada");
	} else {
		actualizarUltimoAcceso();
    puertaAbierta = true;
		if (!permisoConcedido) {
			// Activar alarmas
			accesoFraudulento = true;
			Server.send(200, "plain/text", "X");
		} else {
      accesoConcedido();
			Server.send(200, "plain/text", "O");
		}
	}
}

void handleCierre() {
	if (Server.method() != HTTP_POST) {
		Server.send(405, "text/plain", "Metodo no soportado");
	} else if (!passphrase.compareTo(Server.arg(0))) {
		Server.send(405, "text/plain", "Operacion denegada");
	} else {
    if (puertaAbierta) {
      digitalWrite(LED, LOW);
      actualizarPantallaAcceso();
		  permisoConcedido = false;
      puertaAbierta = false;
    }
		Server.send(200, "plain/text", "O");
	}
}

void handleIntentoApertura() {
	if (Server.method() != HTTP_POST) {
		Server.send(405, "text/plain", "Metodo no soportado");
	} else if (!passphrase.compareTo(Server.arg(0))) {
		Server.send(405, "text/plain", "Operacion denegada");
	} else {
		if (!permisoConcedido) { // Activar alarmas
			actualizarUltimoAcceso();
      actualizarPantallaAcceso();
			intentoAccesoFraudulento = true;
			Server.send(200, "plain/text", "X");
		} else {
			// Si el permiso esta concedido puede pasarse como manipulación por parte del dueño y se ignora
			Server.send(200, "plain/text", "O");
		}
	}
}

void handleNotFound() {
	Server.send(404, "text/plain", "Pagina no encontrada");
}

void setup() {
	// SERIAL //
	Serial.begin(9600);

  Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);

    // Inicialización.
    rtc.Begin();

    RtcDateTime _compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(_compiled);
    Serial.println();

    if (!rtc.IsDateTimeValid()) 
    {
        Serial.println("RTC lost confidence in the DateTime!");
        rtc.SetDateTime(_compiled);
    }

    if (rtc.GetIsWriteProtected())
    {
        Serial.println("RTC was write protected, enabling writing now");
        rtc.SetIsWriteProtected(false);
    }

    if (!rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        rtc.SetIsRunning(true);
    }

    RtcDateTime _now = rtc.GetDateTime();
    if (_now < _compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        rtc.SetDateTime(_compiled);
    }
    else if (_now > _compiled) 
        Serial.println("RTC is newer than compile time. (this is expected)");
    else if (_now == _compiled) 
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");

  pinMode(LED, OUTPUT);
  digitalWrite(LED,LOW);

	// SERVIDOR //
	WiFi.mode(WIFI_STA);
	WiFi.hostname("Servidor-Kit10");
	WiFi.config(IP, gateway, mask, dns);
	WiFi.begin(ssid, pass);
	
	Serial.print("Conectando a la red");
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
  Serial.println(WiFi.localIP());

	if (!MDNS.begin("Kit10")) {
		Serial.println("MDNS no se ha iniciado correctamente");
		while(true);
	}
	
	// Para la interfaz web
	Server.on("/", handlePermiso);
	Server.on("/acceso", handleAcceso);
	Server.on("/historial", handleHistorial);
	
	// Para la otra placa
	Server.on("/apertura", handleApertura);
	Server.on("/cierre", handleCierre);
	Server.on("/act_sospechosa", handleIntentoApertura);
  Server.on("/permiso",handlePermisoConcedido);
	
	Server.onNotFound(handleNotFound);
	Server.begin();
	
	permisoConcedido = false;
	accesoFraudulento = false;

	
	// PANTALLA //
	LCD.init(); LCD.backlight(); LCD.home();
	LCD.createChar(0, KEY_ICON);
	LCD.createChar(1, LOCKED);
	LCD.createChar(2, UNLOCKED);

  LCD.setCursor(1, 0);
  LCD.print("Sistema");
  LCD.setCursor(3, 1);
  LCD.print("Iniciado");
  LCD.setCursor(0,1);
  LCD.write(1);
}

void loop() {
	Server.handleClient();
	MDNS.update();
}

void printDateTime(const RtcDateTime& _dt)
{
    char _datestring[20];

    snprintf_P(_datestring, 
            countof(_datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            _dt.Day(),
            _dt.Month(),
            _dt.Year(),
            _dt.Hour(),
            _dt.Minute(),
            _dt.Second() );
    Serial.println("Fecha:");
    Serial.println(_datestring);
}