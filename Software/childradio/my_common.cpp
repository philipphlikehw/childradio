#include <cstring>
#include <regex>
#include <string>
#include <Arduino.h>


bool isValidHostname(const char* hostname) {
    // Ein gültiger Hostname:
    // - Kann nur Buchstaben (a-z, A-Z), Ziffern (0-9) und Bindestriche (-) enthalten
    // - Darf nicht mit einem Bindestrich beginnen oder enden
    // - Muss zwischen 1 und 253 Zeichen lang sein
    // - Jedes Label (Segmente, die durch Punkte getrennt sind) darf nicht mehr als 63 Zeichen enthalten

    // Wandelt den C-String in std::string um
    std::string hostnameStr(hostname);

    // Berechne die Länge des Hostnamens
    size_t length = hostnameStr.length();  // Verwende std::string::length() statt std::strlen

    // Überprüfe die Länge
    if (length < 1 || length > 253) {
        Serial.println("Hostname-Check: Length does not match");
        return false;
    }

    // Überprüfe, ob der Hostname mit einem Bindestrich beginnt oder endet
    if (hostnameStr.front() == '-' || hostnameStr.back() == '-') {
        Serial.println("Hostname-Check: Hostname must not start or end with a hyphen");
        return false;
    }

    // Regex zur Validierung des gesamten Hostnamens
    std::regex hostnameRegex_old(
        R"((^([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*
            ([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])$)"
    );

     std::regex hostnameRegex(R"(^[a-zA-Z0-9\-]+$)");  // Nur alphanumerische Zeichen und Bindestriche erlaubt

    // Überprüfe mit Regex
    if (!std::regex_match(hostnameStr, hostnameRegex)) {
        Serial.println("Hostname-Check: Invalid character");
        return false;
    }

    // Zusätzliche Überprüfung: Jedes Label darf höchstens 63 Zeichen lang sein
    size_t start = 0;
    size_t end;
    while ((end = hostnameStr.find('.', start)) != std::string::npos) {
        if (end - start > 63) {
            Serial.println("Hostname-Check: Label too long");
            return false;
        }
        start = end + 1;
    }

    // Überprüfe das letzte Label
    if (hostnameStr.length() - start > 63) {
        Serial.println("Hostname-Check: Wrong last label");
        return false;
    }

    return true;
}
