import requests
import time
import random
import math

API_URL = "http://127.0.0.1:8000/websocket/dashboard"

def calculate_distance(coord1, coord2):
    """Calculate Euclidean distance between two coordinates"""
    if not coord1 or not coord2:
        return float('inf')
    return math.sqrt((coord1[0] - coord2[0])**2 + (coord1[1] - coord2[1])**2)

def calculate_direction(from_coord, to_coord):
    """Calculate direction from one coordinate to another"""
    if not from_coord or not to_coord:
        return "Unknown"
    
    dx = to_coord[0] - from_coord[0]
    dy = to_coord[1] - from_coord[1]
    
    # Calculate angle in radians
    angle = math.atan2(dy, dx)
    
    # Convert to degrees
    angle_deg = math.degrees(angle)
    
    # Normalize to 0-360 degrees
    if angle_deg < 0:
        angle_deg += 360
    
    # Convert angle to cardinal direction
    directions = ["E", "NE", "N", "NW", "W", "SW", "S", "SE"]
    index = round(angle_deg / 45) % 8
    
    return directions[index]

def find_nearest_available_bin(bins_data, current_bin_id):
    """Find nearest available bin that isn't FULL"""
    current_bin = bins_data.get(current_bin_id, {})
    current_coord = current_bin.get("coord")
    
    if not current_coord:
        return None, None
    
    nearest_bin_id = None
    min_distance = float('inf')
    
    for bin_id, bin_data in bins_data.items():
        # Skip the current bin and any FULL or offline bins
        if (bin_id == current_bin_id or 
            bin_data.get("status") == "FULL" or 
            bin_data.get("status") == "offline"):
            continue
        
        bin_coord = bin_data.get("coord")
        if bin_coord:
            distance = calculate_distance(current_coord, bin_coord)
            if distance < min_distance:
                min_distance = distance
                nearest_bin_id = bin_id
    
    if nearest_bin_id:
        direction = calculate_direction(
            current_coord, 
            bins_data[nearest_bin_id]["coord"]
        )
        return nearest_bin_id, direction
    
    return None, None

while True:
    # Generate random bin data
    payload = {
        "1": {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK"]),
            "coord": [1.23, 4.56]  # Note: Using lists instead of tuples for JSON serialization
        },
        "2": {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK"]),
            "coord": [3.45, 6.78]  # Added coordinates for better testing
        },
        "3": {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK", "offline"]),  # Added "offline" as a possible status
            "coord": [7.89, 0.12]
        },
        "4": {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK"]),
            "coord": [2.34, 5.67]
        },
        "5": {
            "last_seen": random.randint(1000, 20000),
            "status": random.choice(["FULL", "EMPTY", "OK"]),
            "coord": [8.90, 1.23]
        }
    }
    
    # Calculate nearest available bin for FULL bins
    for bin_id, bin_data in payload.items():
        if bin_data["status"] == "FULL":
            nearest_bin_id, direction = find_nearest_available_bin(payload, bin_id)
            if nearest_bin_id:
                bin_data["next_nearest"] = nearest_bin_id
                bin_data["next_nearest_direction"] = direction
    
    # Send data to the server
    try:
        print(f"Sending data: {payload}")
        response = requests.post(API_URL, json=payload)
        print(f"Response: {response.status_code}")
        print(f"Response content: {response.text}")
    except Exception as e:
        print(f"Error: {e}")
    
    time.sleep(5)