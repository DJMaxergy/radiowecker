void DrawFooterButtons_Power_Sleep_Alarm(void);
bool startUrl(const char *url, bool useAlarmGain = false);

#include <math.h>

// Aktiviert den internen DAC und nutzt Kanal 2 (GPIO26) für den CYD-Lautsprecher
Audio audio(true);

// Funktion für die gesamte erforderliche Einrichtung
void setup_audio() {
  pinMode(AMP_SHUTDOWN, OUTPUT);
  digitalWrite(AMP_SHUTDOWN, HIGH);
  // Für den internen DAC des CYD-Boards werden keine extra Pinouts benötigt.
  audio.forceMono(true);
  // Standard-Startlautstärke setzen (Wertebereich bei schreibfaul1 ist 0 bis 21)
  audio.setVolume(12); 
}

// Startet den lauten MP3-Wecksound von der Micro-SD-Karte
void alarm_beep_start(void) {
  stopPlaying(); // Beendet ein eventuell laufendes Radio sauber

  // 1. Prüfen, ob die SD-Karte einsatzbereit ist
  if (SD.cardSize() == 0) {
    RADIO_SERIAL(Serial.println("Alarm abgebrochen: Keine SD-Karte im Slot!"));
    return;
  }

  // 2. Den Onboard-Verstärker aufwecken (Active-LOW -> LOW zum Einschalten)
  digitalWrite(AMP_SHUTDOWN, LOW);
  delay(10); 

  // 3. Wecklautstärke (alarmGain) auf die Audio-Pipeline anwenden
  setGain((float)alarmGain); 

  // 4. KORREKTUR: Lädt und spielt die MP3-Datei direkt von der SD-Karte
  bool success = audio.connecttoFS(SD, "/alarm.mp3"); 
  
  if (success) {
    alarmBeepActive = true; 
    audio.setBalance(1); // Sound fest auf GPIO 26 zwingen (DAC2)
    RADIO_SERIAL(Serial.println("SD-MP3-Wecker gestartet!"));
  } else {
    RADIO_SERIAL(Serial.println("alarm.mp3 wurde auf der SD-Karte nicht gefunden!"));
  }
}

// Diese Funktion überprüft, ob der Decoder läuft
void audio_loop() {
  // Wenn die Bibliothek meldet, dass sie im Hintergrund Daten empfängt/decodiert,
  // führen wir die Schleife aus, völlig unabhängig von den Variablen des Haupt-Tabs!
  if (audio.isRunning()) {
    audio.loop();
  }
}

/* ICY/Shoutcast: StreamTitle oft ISO-8859-1 (ESP8266Audio: isUnicode=false). Display/Web brauchen UTF-8. */
static bool metadata_is_valid_utf8(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    if (*p < 0x80) {
      ++p;
      continue;
    }
    if ((*p & 0xE0) == 0xC0) {
      if (!p[1] || (p[1] & 0xC0) != 0x80) return false;
      p += 2;
      continue;
    }
    if ((*p & 0xF0) == 0xE0) {
      if (!p[1] || !p[2] || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false;
      p += 3;
      continue;
    }
    if ((*p & 0xF8) == 0xF0) {
      if (!p[1] || !p[2] || !p[3] || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
        return false;
      p += 4;
      continue;
    }
    return false;
  }
  return true;
}

static void metadata_latin1_to_utf8(const char *in, char *out, size_t outsz) {
  size_t o = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 3 < outsz; ++p) {
    if (*p < 0x80)
      out[o++] = (char)*p;
    else {
      out[o++] = (char)(0xC0 | ((*p) >> 6));
      out[o++] = (char)(0x80 | ((*p) & 0x3F));
    }
  }
  if (outsz == 0) return;
  if (o >= outsz) o = outsz - 1;
  out[o] = 0;
}

// Wird von schreibfaul1 automatisch aufgerufen, wenn ein neuer Songtitel empfangen wird
void audio_showstreamtitle(const char *info) {
  // CRASH-SCHUTZ: Wenn das Radio im Standby ist
  if (!audio.isRunning()) return;

  if (info && *info) {
    char utf8_buf[128]; // Puffer für die Textkonvertierung

    // Prüfen, ob der Sender sauberes UTF-8 schickt, andernfalls nach UTF-8 konvertieren
    if (metadata_is_valid_utf8(info)) {
      strncpy(utf8_buf, info, sizeof(utf8_buf) - 1);
      utf8_buf[sizeof(utf8_buf) - 1] = '\0';
    } else {
      metadata_latin1_to_utf8(info, utf8_buf, sizeof(utf8_buf));
    }

    // Direkt in die globalen Variablen des Radiowecker-Projekts schreiben
    strncpy(title, utf8_buf, sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';
    
    // WICHTIG: Das Projekt nutzt 'newTitle', um der Loop zu sagen, dass das Display aktualisiert werden muss
    newTitle = true; 
    
    RADIO_SERIAL(Serial.printf("Metadata Title direkt: %s\n", title));
  }
}

// Stoppt das Abspielen und räumt den Arbeitsspeicher auf
void stopPlaying() {
  alarmBeepActive = false;

  // Schließt den Song und gibt alle internen Decoder- und Dateipuffer im RAM frei
  audio.stopSong(); 
  
  yield(); // Speicherbereinigung im Hintergrund abwarten

  // Verstärker-Chip komplett stumm schalten gegen das Standby-Rauschen
  digitalWrite(AMP_SHUTDOWN, HIGH);
  
  RADIO_SERIAL(Serial.printf("Audio gestoppt. Freier Heap danach: %u Bytes\n", (unsigned)ESP.getFreeHeap()));
}

// Startet das Abspielen eines Streams von der angegebenen Station (URL aus stationlist[].url o. Ä.)
bool startUrl(const char *url, bool useAlarmGain) {
  stopPlaying();  // Zuerst bestehende Streams und Beeps schließen

  if (!url || !*url) {
    RADIO_SERIAL(Serial.println(F("startUrl: leere URL")));
    return false;
  }

  RADIO_SERIAL(Serial.printf("Active station %s\n", url));
  yield();

  // Startet den Webstream. schreibfaul1 übernimmt ICY-Stream, Pufferung und MP3-Decoder automatisch.
  bool success = audio.connecttohost(url);

  if (!success) {
    RADIO_SERIAL(Serial.println(F("startUrl: connecttohost fehlgeschlagen")));
    return false;
  }

  digitalWrite(AMP_SHUTDOWN, LOW);

  // Lautstärke basierend auf dem Weck- oder Normalmodus setzen
  setGain(useAlarmGain ? alarmGain : curGain);

  RADIO_SERIAL(Serial.printf_P(PSTR("MP3 ok running=%d\n"), audio.isRunning() ? 1 : 0));
  return success;
}

// Ändert die Lautstärke auf den aktuellen Gain-Wert
void setGain(float gain) {
  // Eingabewert begrenzen (Sicherheitshalber, falls Werte außerhalb von 0-100 kommen)
  if (gain < 0.0f) gain = 0.0f;
  if (gain > 100.0f) gain = 100.0f;

  // Der Slider liefert 0 bis 100. schreibfaul1/ESP32-audioI2S benötigt eine Ganzzahl von 0 bis 21.
  // Wir nutzen map() für die lineare Umrechnung:
  int new_volume = map((int)gain, 0, 100, 0, AUDIO_GAIN_MAX);

  // Lautstärke in der neuen Bibliothek setzen
  audio.setVolume(new_volume);

  RADIO_SERIAL(Serial.printf("New volume (Percent: %4.2f%%) -> Internal DAC Volume: %d\n", gain, new_volume));
}

// Optional: Falls du auch den Sendernamen abfangen willst
void audio_showstation(const char *info) {
  // Hier könnte Code stehen, um den Sendernamen anzuzeigen
}

// Wird von der Bibliothek automatisch aufgerufen, wenn die MP3-Datei fertig abgespielt wurde
void audio_eof_mp3(const char *info){
  if (alarmBeepActive) {
    RADIO_SERIAL(Serial.println("Wecksound beendet -> Speicher freigeben & Loop neu starten."));
    
    // WICHTIG GEGEN HEAP-FRAGMENTIERUNG: 
    // Stoppt den alten Datei-Task sauber und gibt alle belegten RAM-Puffer restlos frei,
    // bevor ein neuer Dateizeiger geöffnet wird!
    audio.stopSong(); 
    
    yield(); // Dem ESP32-Betriebssystem eine Mikrosekunde Zeit geben, den Heap aufzuräumen

    // Startet die MP3-Datei von der SD-Karte sicher von vorn
    audio.connecttoFS(SD, "/alarm.mp3"); 
    audio.setBalance(1);
  }
}
