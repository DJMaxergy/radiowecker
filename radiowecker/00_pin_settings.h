// Pin-Definitionen für verschiedene Komponenten des Projekts

// Pin für den Lichtsensor (LDR)
#define LDR 34

// Pin für Alarm LED (Grüne LED)
#define ALARM_LED 16

// Audio-Pins
#define AMP_SHUTDOWN 27  // Amplifier IC SHUTDOWN Pin
#define INT_DAC_OUT 26

// Pins für den Touchscreen
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33

// Pins für das TFT-Display
#define TFT_LED 21  // Hintergrundbeleuchtung des TFT-Displays
#define LED_ON 1    // Logikpegel für die Hintergrundbeleuchtung (0 = Ein)

// Pins für SD Slot
#define SD_CS  5

/* LDR-Modus (Helligkeit 0 %): Mindest-PWM 0–255 nach Umrechnung vom ADC.
   Verhindert komplett ausgeschaltete Backlight (Slider/Stop → sonst oft unbedienbar). */
#ifndef LDR_LED_PWM_MIN
#define LDR_LED_PWM_MIN 1  //4
#endif

/* LDR-Modus: Anteil des neuen Messwerts am Soll-PWM (1/DIV). Größer DIV = weicher, langsamer.
   Beim Wechsel feste % → LDR startet von der zuletzt geschriebenen Backlight (kein Sprung). */
#ifndef LDR_PWM_SMOOTH_DIV
#define LDR_PWM_SMOOTH_DIV 1  //2
#endif

/* Optionale Hardware-Tasten (nach GND, INPUT_PULLUP): Pin < 0 = aus / nicht angeschlossen (Default).
   Stop: Radio/Wecker wie Software-Stop abschalten; wenn nichts läuft (kein Schlummer) → Radio einschalten.
   Schlummer: nur ausgewertet, wenn alarmActionsVisible (Wecker-Overlay). */
#ifndef ALARM_HW_BTN_STOP_PIN
#define ALARM_HW_BTN_STOP_PIN -1
#endif
#ifndef ALARM_HW_BTN_SNOOZE_PIN
#define ALARM_HW_BTN_SNOOZE_PIN -1
#endif