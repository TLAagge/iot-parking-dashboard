# IoT Parking System

Ett IoT-baserat parkeringssystem som mäter om en parkeringsplats är upptagen eller ledig. Systemet använder en **HC-SR04 ultraljudssensor**, **ESP32** och **AWS** för att samla in, lagra och visualisera mätdata.

Projektet är utvecklat som ett individuellt portfolioprojekt inom utbildningen **IoT Mjukvaruutvecklare på Nackademin**.

---

## 1. Projektbeskrivning

Utgångspunkten är ett gemensamt parkeringsgarage med totalt 40 parkeringsplatser. Det finns inga fasta platser och kunden vill kunna se hur många platser som är upptagna respektive lediga.

Systemet ska:

- mäta avståndet till ett fordon
- skicka sensordata till AWS
- lagra mätdata i en databas
- hämta extern information från ett API
- visualisera insamlad data i ett webbgränssnitt
- uppdatera informationen nära realtid

---


## 2. Arkitektur

```text
                    ┌──────────────┐
                    │   SMHI API   │
                    └──────┬───────┘
                           │
                           │ Temperatur
                           ▼
┌──────────────┐     ┌──────────────┐
│   HC-SR04    │────►│    ESP32     │
│ Ultraljud    │     │              │
└──────────────┘     └──────┬───────┘
                            │
                       Wi-Fi / MQTT
                            │
                            ▼
                    ┌────────────────┐
                    │ AWS IoT Core   │
                    └───────┬────────┘
                            │
                       IoT Rule
                            │
                            ▼
                    ┌────────────────┐
                    │   DynamoDB     │
                    │ ParkingMeasurements
                    └───────┬────────┘
                            │
                            ▼
                    ┌────────────────┐
                    │ AWS Lambda     │
                    │ Dashboard API  │
                    └───────┬────────┘
                            │
                            ▼
                    ┌────────────────┐
                    │ AWS Amplify    │
                    │   Dashboard    │
                    └────────────────┘
```

### Dataflöde

1. HC-SR04 mäter avståndet till fordonet.
2. ESP32 läser av sensorn.
3. ESP32 hämtar aktuell temperatur från SMHI:s API.
4. ESP32 skickar informationen via MQTT till AWS IoT Core.
5. En AWS IoT Rule skickar informationen till DynamoDB.
6. Lambda läser de senaste mätningarna från DynamoDB.
7. Dashboardet hämtar data från Lambda.
8. AWS Amplify används för att publicera webbgränssnittet.

---

## 3. Hårdvara

### ESP32

ESP32 fungerar som den lokala IoT-enheten och ansvarar för att:

- läsa HC-SR04
- ansluta till Wi-Fi
- ansluta till AWS IoT Core
- publicera MQTT-meddelanden
- hämta temperatur från SMHI

<img width="640" height="640" alt="image" src="https://github.com/user-attachments/assets/7a974591-67b2-4d0b-b798-84152985a588" />


### HC-SR04

HC-SR04 används för att mäta avståndet till ett fordon.

<img width="350" height="202" alt="image" src="https://github.com/user-attachments/assets/5a11b711-b496-4e82-b857-e23614e3935b" />


Anslutningar:

| HC-SR04 | ESP32 |
|---|---|
| VCC | VIN / 5V |
| GND | GND |
| TRIG | GPIO 5 |
| ECHO | GPIO 18 via spänningsdelare |

Eftersom HC-SR04 använder en högre signalnivå på ECHO används en spänningsdelare innan signalen går till ESP32:s GPIO.

### Parkeringslogik

För demonstrationen används följande gränsvärde:

```text
Avstånd < 10 cm  → Upptagen
Avstånd ≥ 10 cm → Ledig
```

Gränsvärdet kan ändras beroende på den fysiska placeringen av sensorn.

---

## 4. Kommunikation med AWS IoT Core

ESP32 kommunicerar med AWS IoT Core med:

- MQTT
- TLS
- enhetscertifikat
- privat nyckel
- Root CA

AWS IoT Core används som ingångspunkt för sensordatan.

### MQTT-topic

```text
esp32/parking/test
```

Ett exempel på ett publicerat meddelande:

```json
{
  "device": "ESP32-Parking",
  "distance_cm": 3.28,
  "temperature_c": 0.70
}
```

---

## 5. IoT-säkerhet

Projektet använder grundläggande IoT-säkerhet genom AWS IoT Core.

### Enhetsautentisering

ESP32 använder:

- device certificate
- private key
- Amazon Root CA

Kommunikationen med AWS IoT Core sker över TLS.

### AWS IoT Policy

ESP32 har en specifik IoT-policy som begränsar åtkomsten.

Exempel:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:eu-north-1:*:client/${iot:Connection.Thing.ThingName}"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Publish",
      "Resource": "arn:aws:iot:eu-north-1:*:topic/esp32/parking/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Subscribe",
      "Resource": "arn:aws:iot:eu-north-1:*:topicfilter/esp32/parking/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Receive",
      "Resource": "arn:aws:iot:eu-north-1:*:topic/esp32/parking/*"
    }
  ]
}
```

Känsliga uppgifter som Wi-Fi-lösenord, certifikat och privata nycklar ska **inte** läggas upp på GitHub.

ESP32-projektet använder därför en separat `secrets.h` som inte ska committas.

---

## 6. AWS IoT Rule

En IoT Rule används för att ta emot MQTT-meddelandet och skicka relevanta värden till DynamoDB.

### Rule

```text
ParkingToDynamoDB
```

### SQL

```sql
SELECT device, distance_cm, temperature_c,
       cast(timestamp() AS String) AS timestamp
FROM 'esp32/parking/test'
```

Regeln tar ut:

- device
- distance_cm
- temperature_c
- timestamp

och skickar informationen till DynamoDB.

---

## 7. DynamoDB

Data lagras i tabellen:

```text
ParkingMeasurements
```

Region:

```text
eu-north-1
```

### Primärnycklar

```text
Partition key: device
Sort key: timestamp
```

### Exempel på lagrad data

| Fält | Exempel |
|---|---|
| device | ESP32-Parking |
| timestamp | 1795539927730 |
| distance_cm | 3.28 |
| temperature_c | 0.70 |

DynamoDB används för att lagra historiska mätningar som senare kan hämtas av dashboardets API.

<img width="610" height="202" alt="image" src="https://github.com/user-attachments/assets/b0270089-c36e-4559-80e1-d4a1c01c0919" />


---

## 8. Externt API – SMHI

Projektet använder SMHI:s öppna API för att hämta aktuell temperatur.

ESP32 hämtar temperaturdata från SMHI och inkluderar temperaturen i MQTT-meddelandet.

Dataflödet blir:

```text
SMHI API
   ↓
ESP32
   ↓
AWS IoT Core
   ↓
DynamoDB
```

SMHI, Sveriges meteorologiska och hydrologiska institut, tillhandahåller väderdata som jag hämtar via deras API.

---

## 9. AWS Lambda

Lambda-funktionen heter:

```text
ParkingDashboardAPI
```

Lambda läser data från:

```text
ParkingMeasurements
```

och returnerar de senaste mätningarna som JSON.

Exempel:

```json
[
  {
    "device": "ESP32-Parking",
    "timestamp": "1795539927730",
    "distance_cm": 3.28,
    "temperature_c": 0.7
  }
]
```

Lambda använder en **Function URL** som dashboardet kan anropa.

---

## 10. AWS Amplify

Dashboardet är publicerat med AWS Amplify.

Webbgränssnittet hämtar data från Lambda och visar:

- parkeringsstatus
- avstånd
- temperatur
- tidigare mätningar
- diagram
- automatisk uppdatering

### Dashboard

```text
AWS Amplify
     ↓
Parkeringsdashboard
     ↓
Lambda Function URL
     ↓
DynamoDB
```

Dashboardet uppdateras automatiskt och behöver därför inte laddas om manuellt.

---

## 11. Exempel på dashboard

Dashboardet visar exempelvis:

```text
Parkeringsstatus: UPPTAGEN

Avstånd: 1.99 cm

Temperatur: 0.7 °C
```

samt ett diagram över tidigare avståndsmätningar.

## Dashboard

<img width="955" height="444" alt="image" src="https://github.com/user-attachments/assets/ca1f40d3-b560-4ae5-913d-cd7d5d029541" />


---

## 12. Projektstruktur

```text
iot-parking-dashboard/
│
├── arduino/
│   ├── ESP32-Parking-AWS.ino
│   └── secrets.h
│
├── index.html
└── README.md
```

`ESP32-Parking-AWS.ino` innehåller programmet för ESP32 och HC-SR04-sensorn.

`secrets.h` innehåller platshållare för Wi-Fi- och AWS-uppgifter. Vid lokal användning måste platshållarna ersättas med användarens egna uppgifter.

---

## 13. Installation och konfiguration

### ESP32

ESP32-projektet kräver:

- Arduino IDE
- ESP32 board support
- Wi-Fi
- PubSubClient
- WiFiClientSecure
- HTTPClient
- AWS IoT Core-certifikat

Följande information behöver konfigureras lokalt:

```text
Wi-Fi SSID
Wi-Fi password
AWS IoT endpoint
Device certificate
Private key
Root CA
```

Dessa uppgifter ska sparas lokalt och inte publiceras.

### AWS

Följande AWS-resurser används:

```text
AWS IoT Core
├── Thing: ESP32-Parking
├── Certificate
└── Policy: ESP32-Parking-Policy

IoT Rule
└── ParkingToDynamoDB

DynamoDB
└── ParkingMeasurements

Lambda
└── ParkingDashboardAPI

AWS Amplify
└── IoT Parking Dashboard
```

---

## 14. Testning

Systemet har testats genom hela dataflödet:

```text
HC-SR04
   ↓
ESP32
   ↓
AWS IoT Core
   ↓
IoT Rule
   ↓
DynamoDB
   ↓
Lambda
   ↓
AWS Amplify
   ↓
Dashboard
```

ESP32 har även testats med mobil hotspot för att verifiera att enheten kan kommunicera med AWS över internet.

Dashboardet har testats genom att kontrollera att aktuella mätvärden och historiska mätningar visas.

---

## 15. Resultat

Det färdiga systemet kan:

- läsa avstånd från HC-SR04
- skicka sensordata från ESP32 till AWS
- kommunicera med AWS IoT Core via MQTT
- använda TLS och enhetscertifikat
- bearbeta MQTT-data med en IoT Rule
- lagra mätdata i DynamoDB
- hämta temperatur från SMHI
- läsa data genom AWS Lambda
- visualisera data i ett AWS Amplify-dashboard
- uppdatera dashboardet automatiskt

Systemet fungerar som en komplett IoT-kedja från fysisk sensor till molnbaserad visualisering.

---

## 16. Uppfyllda projektkrav

| Krav | Implementering |
|---|---|
| Sensor ansluten via ESP32 | HC-SR04 + ESP32 |
| Skicka data till AWS | AWS IoT Core |
| Lagra data i databas | DynamoDB |
| Externt API | SMHI API |
| Visualisera data | AWS Amplify |
| IoT-kommunikation | MQTT |
| IoT-säkerhet | TLS, certifikat och IoT-policy |
| Dokumentation | GitHub README |

---

## 17. Tekniker

- ESP32
- HC-SR04
- Arduino IDE
- C/C++
- MQTT
- AWS IoT Core
- AWS DynamoDB
- AWS Lambda
- AWS Amplify
- SMHI Open Data API
- HTML
- CSS
- JavaScript
- GitHub

---

## 18. Projektets syfte

Projektet visar en enkel IoT-arkitektur där sensordata samlas in från en fysisk enhet, skickas till molnet, lagras och visualiseras.

Fokus ligger på att visa hela dataflödet:

**Sensor → ESP32 → AWS IoT Core → DynamoDB → Lambda → Amplify → Dashboard**
