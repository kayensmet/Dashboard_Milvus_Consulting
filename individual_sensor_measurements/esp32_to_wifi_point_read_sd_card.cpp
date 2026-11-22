#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>

// Gegevens voor het Wi-Fi Access Point van de ESP32
const char* ssid = "ESP32-Logger";
const char* password = "password123"; // Minimaal 8 tekens

WebServer server(80);

// Pinout voor de SD-kaart op jouw ESP32-S3 (pas aan indien nodig)
const int SD_CS = 10; 

// HTML pagina met zowel de downloadknop als de wis-knop
void handleRoot() {
    String html = "<!DOCTYPE html><html>";
    html += "<head><meta charset='UTF-8'><title>ESP32 S3 Data Logger</title></head>";
    html += "<body style='font-family: Arial; text-align: center; margin-top: 50px;'>";
    html += "<h1>ESP32-S3 Sensor Logger</h1>";
    html += "<p>Beheer hier jouw sensorlogbestanden.</p>";
    
    // Download knop
    html += "<div style='margin: 20px;'>";
    html += "<a href='/download'><button style='font-size: 18px; padding: 12px 25px; background-color: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer;'>Download CSV</button></a>";
    html += "</div>";
    
    // Wis knop met JavaScript bevestigingspop-up
    html += "<div style='margin: 20px;'>";
    html += "<button onclick='confirmDelete()' style='font-size: 18px; padding: 12px 25px; background-color: #f44336; color: white; border: none; border-radius: 5px; cursor: pointer;'>SD-Kaart Leegmaken</button>";
    html += "</div>";

    // JavaScript voor de pop-up waarschuwing
    html += "<script>";
    html += "function confirmDelete() {";
    html += "  if (confirm('WAARSCHUWING: Weet je zeker dat je alle opgeslagen data wilt wissen?')) {";
    html += "    window.location.href = '/clear';";
    html += "  }";
    html += "}";
    html += "</script>";

    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// Bestand downloaden
void handleDownload() {
    File file = SD.open("/sensor_data.csv", FILE_READ);
    if (!file) {
        server.send(404, "text/plain", "Fout: Geen logbestand gevonden om te downloaden.");
        return;
    }

    Serial.println("Start download naar laptop...");
    server.sendHeader("Content-Type", "text/csv");
    server.sendHeader("Content-Disposition", "attachment; filename=sensor_data.csv");
    server.streamFile(file, "text/csv");
    file.close();
    Serial.println("Download voltooid!");
}

// SD-kaart / bestand wissen
void handleClear() {
    Serial.println("Wis-verzoek ontvangen...");

    // Controleer of het bestand bestaat en verwijder het
    if (SD.exists("/sensor_data.csv")) {
        if (SD.remove("/sensor_data.csv")) {
            Serial.println("Bestand succesvol gewist.");
        } else {
            Serial.println("Fout bij wissen van bestand!");
            server.send(500, "text/plain", "Fout bij het wissen van het bestand.");
            return;
        }
    }

    // Optioneel: direct een nieuw leeg bestand aanmaken met optionele headers
    File file = SD.open("/sensor_data.csv", FILE_WRITE);
    if (file) {
        file.println("timestamp,tijd_ms,waarde1,waarde2"); // Voorbeeld headers
        file.close();
    }

    // Stuur een nette melding terug naar de browser en stuur terug naar de hoofdpagina
    String html = "<script>alert('SD-kaart is succesvol geleegd!'); window.location.href='/';</script>";
    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialiseer de SD-kaart
    if (!SD.begin(SD_CS)) {
        Serial.println("Waarschuwing: SD-kaart initialisatie mislukt!");
    } else {
        Serial.println("SD-kaart succesvol geÃ¯nitialiseerd.");
        
        // Zorg dat er altijd een bestand klaarstaat als het er nog niet is
        if (!SD.exists("/sensor_data.csv")) {
            File file = SD.open("/sensor_data.csv", FILE_WRITE);
            if (file) {
                file.println("timestamp,tijd_ms,waarde1,waarde2");
                file.close();
            }
        }
    }

    // Start het Wi-Fi Access Point
    WiFi.softAP(ssid, password);
    Serial.println("Wi-Fi Access Point gestart!");
    Serial.print("Verbind met Wi-Fi: ");
    Serial.println(ssid);
    Serial.print("Ga in je browser naar: http://");
    Serial.println(WiFi.softAPIP()); // Standaard IP: 192.168.4.1

    // Definieer de webserver routes
    server.on("/", handleRoot);
    server.on("/download", handleDownload);
    server.on("/clear", handleClear);
    
    // Start de webserver
    server.begin();
    Serial.println("Webserver is actief.");
}

void loop() {
    server.handleClient();

    // Voorbeeld van doorlopend data schrijven naar de SD-kaart in je loop:
    /*
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 1000) { // elke 1 seconde
        lastLog = millis();
        File dataFile = SD.open("/sensor_data.csv", FILE_APPEND);
        if (dataFile) {
            dataFile.print(millis());
            dataFile.print(",23.5,45.2\n"); // Voorbeeld data
            dataFile.close();
        }
    }
    */
}