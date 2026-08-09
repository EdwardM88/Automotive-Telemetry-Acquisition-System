import serial
import json
import time
from pymongo import MongoClient
import os

myFile = 'istoric_masina.json'

savedFile = os.path.abspath(myFile);

PORT_COM = 'COM8' # portul COM la care e conenctat ESP32-ul (poate fi diferit pe calculatorul tău) 
BAUD = 115200 # viteza de comunicare serială (trebuie să fie aceeași cu cea din codul ESP32)
FISIER_LOCAL = 'istoric_masina.json'
print("=========================================")
print(f"ATENȚIE: Datele se salvează fizic aici:")
print(f"--> {savedFile} <--")
print("=========================================")

# <-- SCHIMBĂ cu link-ul tău real de la MongoDB (pune parola ta reală în loc de <password>)
MONGO_URL = "mongodb+srv://userul_tau:parola_ta@cluster0.mongodb.net/?retryWrites=true&w=majority"

print("Conectare la MongoDB Atlas...")
try:
    client = MongoClient(MONGO_URL, serverSelectionTimeoutMS=5000)
    db = client["MasinaTelemetrie"]
    colectie = db["Curse"]

    client.admin.command('ping')
    print("[OK] MongoDB Conectat cu succes!")
except Exception as e:
    print(f"[EROARE CRITICĂ] Nu m-am putut conecta la MongoDB: {e}")
    print("Sistemul va salva datele DOAR local.")
    
    colectie = None

#  CONEXIUNE SERIALĂ (Cablul USB)

print(f"Deschidere port {PORT_COM}...")
try:
    ser = serial.Serial(PORT_COM, BAUD, timeout=1)
    print("[OK] Port deschis. Ascult date de la ESP32...\n")
except Exception as e:
    print(f"[EROARE CRITICĂ] Nu pot deschide {PORT_COM}.")
    print("Verifică dacă ai închis Serial Monitor-ul din Arduino IDE!")
    exit()

#  BUCLA PRINCIPALĂ DE ACHIZIȚIE

while True:
    try:
        # Verificăm dacă a venit ceva pe cablu
        if ser.in_waiting > 0:
            # Citim linia, o decodăm și tăiem spațiile libere
            linie = ser.readline().decode('utf-8', errors='ignore').strip()
            
            # Dacă arată a pachet JSON valid, îl procesăm
            if linie.startswith("{") and linie.endswith("}"):
                date = json.loads(linie)
                
                if "latitude" in date["nodes"]["master_telemetry"]["data"]:
                    date["nodes"]["master_telemetry"]["data"]["latitude"] = "CENZURAT"
                    date["nodes"]["master_telemetry"]["data"]["longitude"] = "CENZURAT"
                


                # SALVARE LOCALĂ
                # Deschidem fișierul în modul 'a' (append) și adăugăm linia la final
                with open(FISIER_LOCAL, 'a') as f:
                    f.write(json.dumps(date) + '\n')
                
                # TRIMITERE ÎN CLOUD
                if colectie is not None:
                    # insert_one trimite tot dicționarul exact așa cum e în baza de date
                    colectie.insert_one(date)
                
                # AFIȘARE PE ECRAN
                # Extragem valorile pentru a le printa (folosim .get() ca să nu dea eroare dacă lipsesc)
                rpm = date["nodes"]["slave_simulator"]["data"].get("engine_rpm", 0)
                vit = date["nodes"]["master_telemetry"]["data"].get("gps_speed_kmh", 0)
                lat = date["nodes"]["master_telemetry"]["data"].get("latitude", 0.0)
                lng = date["nodes"]["master_telemetry"]["data"].get("longitude", 0.0)
                g_force = date["nodes"]["master_telemetry"]["data"].get("g_force_x", 0.0)
                
                print(f"[SALVAT] RPM: {rpm} | Vit: {vit} km/h | GPS: {lat}, {lng} | G_force_x: {g_force} -> Fișier & Cloud")
                
    except json.JSONDecodeError:
        # Ignorăm pachetele stricate sau mesajele de eroare de la ESP32
        pass 
    except KeyboardInterrupt:
        # Dacă apeși Ctrl+C în terminal, oprim totul curat
        print("\n[!] Sistem oprit manual de utilizator.")
        ser.close()
        break
    except Exception as e:
        print(f"[EROARE BUCLE] A apărut o problemă: {e}")
        time.sleep(1)