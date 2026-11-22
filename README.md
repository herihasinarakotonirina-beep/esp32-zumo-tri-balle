# Robot Zumo Trieur de Balles (ESP32 & Bras Robotisé)

Projet réalisé durant le **Semestre 2 à l'IUT de Cachan**. L'objectif était de concevoir et programmer un robot mobile autonome capable de suivre une ligne, d'identifier la couleur de balles (rouges ou bleues) et de dialoguer avec un automate pilotant un bras robotisé en groupe de 4.

## Ce que j'ai développé
- **Architecture en machine à états (`switch / case`)** gérant l'ensemble du cycle du robot (initialisation, attente de balle, analyse de couleur, suivi de ligne, dépôt/rejet et retour à la station).
- **Pilotage embarqué sur ESP32** (PlatformIO / Framework Arduino) d'un châssis robotique type Zumo (commande PWM des moteurs à 20 kHz).
- **Suivi de ligne avec régulateur PD** (`Co`, `Kd`) utilisant 4 capteurs infrarouges analogiques et une routine de calibration automatique (`min` / `max`) au démarrage.
- **Détection et tri par couleur** grâce au capteur I2C **Adafruit TCS34725**.
- **Interface homme-machine locale** sur écran Grove LCD RGB 16x2 et boutons poussoirs (réglage des gains, calibration et choix de la couleur cible).
- **Communication industrielle sans fil (Modbus TCP)** en Wi-Fi permettant au robot de synchroniser ses actions avec le bras robotisé .

## Matériel utilisé
- Microcontrôleur **ESP32**
- Châssis mobile **Zumo**
- Capteur de couleur **TCS34725**
- Écran **Grove LCD RGB Backlight**
- 4 capteurs infrarouges CNY70
- Automate programmable + **bras robotisé** (liaison Wi-Fi / Modbus TCP)
