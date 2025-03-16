import requests
import time
import random

API_URL = "http://127.0.0.1:8000/websocket/dashboard"

while True:
    payload = {
        1: {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK"]),
            "coord": (1.23, 4.56)
        },
        2: {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK"]),
            "coord": None,
        },
        3: {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK"]),
            "coord": (7.89, 0.12),
        },
    }
    try:
        response = requests.post(API_URL, json=payload)
        print(response.json())
    except Exception as e:
        print(f"Error: {e}")
    
    time.sleep(5)