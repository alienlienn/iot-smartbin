import serial
import json
import math
import time
import requests
import threading
from collections import defaultdict

# Define known beacon locations
BEACON_LOCATIONS = {
    "B1": (7, -3.5),    # (x, y) coordinates of Beacon1
    "B2": (2, 5),    # (x, y) coordinates of Beacon2
    "B3": (-6, -3)    # (x, y) coordinates of Beacon3
}

# COM port settings
MESH_COM_PORT = 'COM4'  # Port for mesh network communication
BEACON_COM_PORT = 'COM5'  # Port for beacon communication
BAUD_RATE = 9600

# Server settings
DJANGO_SERVER_URL = "http://172.20.10.5:8000/websocket/dashboard"

# RSSI to distance conversion parameters
RSSI_REF = -120  # RSSI at 4 meter distance
N = 2.5          # Path loss exponent

# Shared data structures
beacon_data = defaultdict(dict)
routing_table = {
    1: {"status": "OFFLINE", "last_seen": 0, "coord": None},
    2: {"status": "OFFLINE", "last_seen": 0, "coord": None},
    3: {"status": "OFFLINE", "last_seen": 0, "coord": None}
}

# Locks for thread safety
beacon_data_lock = threading.Lock()
routing_table_lock = threading.Lock()

def rssi_to_distance(rssi):
    """Convert RSSI to estimated distance in meters"""
    return 4 * (10 ** ((RSSI_REF - rssi) / (10 * N)))

def trilateration(distances):
    """
    Calculate position using trilateration from distances to three beacons
    Returns (x, y) coordinates
    """
    # Get beacon positions and distances
    (x1, y1) = BEACON_LOCATIONS["B1"]
    (x2, y2) = BEACON_LOCATIONS["B2"]
    (x3, y3) = BEACON_LOCATIONS["B3"]
    
    r1 = distances["B1"]
    r2 = distances["B2"]
    r3 = distances["B3"]
    
    # Calculate position using trilateration formulas
    A = 2 * (x2 - x1)
    B = 2 * (y2 - y1)
    C = r1**2 - r2**2 - x1**2 + x2**2 - y1**2 + y2**2
    
    D = 2 * (x3 - x2)
    E = 2 * (y3 - y2)
    F = r2**2 - r3**2 - x2**2 + x3**2 - y2**2 + y3**2
    
    # Solve the system of equations
    x = (C * E - F * B) / (E * A - B * D)
    y = (C * D - A * F) / (B * D - A * E)
    
    return (x, y)

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

def find_nearest_available_bin(routing_table, current_bin_id):
    """Find nearest available bin that isn't FULL"""
    current_bin = routing_table.get(current_bin_id, {})
    current_coord = current_bin.get("coord")
    
    if not current_coord:
        return None, None
    
    nearest_bin_id = None
    min_distance = float('inf')
    
    for bin_id, bin_data in routing_table.items():
        # Skip the current bin and any FULL or offline bins
        if (bin_id == current_bin_id or 
            bin_data.get("status") == "FULL" or 
            bin_data.get("status") == "OFFLINE"):
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
            routing_table[nearest_bin_id]["coord"]
        )
        return nearest_bin_id, direction
    
    return None, None

def send_data_to_server(routing_table):
    """Send the current routing table data to the Django server"""
    try:
        response = requests.post(
            DJANGO_SERVER_URL,
            json=routing_table,
            headers={"Content-Type": "application/json"},
            timeout=5  # Add timeout to prevent hanging
        )
        return True
    except requests.exceptions.ConnectionError as e:
        print(f"Connection error: {e}")
        return False
    except Exception as e:
        print(f"Error sending data to server: {e}")
        return False

def handle_mesh_serial(mesh_ser):
    """Thread function to handle mesh serial communication"""
    print(f"Mesh serial thread started on {MESH_COM_PORT}")
    
    while True:
        try:
            if mesh_ser.in_waiting > 0:
                # Read line from serial port
                line = mesh_ser.readline().decode('utf-8', errors='ignore').strip()
                
                if line.startswith("Routing Table:"):
                    # Parse routing table data
                    while mesh_ser.in_waiting > 0:
                        line = mesh_ser.readline().decode('utf-8', errors='ignore').strip()
                        if line.startswith("Node"):
                            parts = line.split()
                            print(f"Mesh: {parts}")
                            if len(parts) >= 9 and "Last" in parts and "Status" in parts:
                                try:
                                    # Extract node ID
                                    node_id = int(parts[1])
                                    
                                    # Find the last seen value
                                    last_seen = 0
                                    for i in range(len(parts)):
                                        if i < len(parts) - 1 and parts[i+1] == "ago" and parts[i].endswith("s"):
                                            try:
                                                # Extract just the number before "s"
                                                last_seen_str = parts[i].replace("s", "").strip()
                                                last_seen = int(float(last_seen_str))
                                                print(f"Mesh: Last seen for Node {node_id}: {last_seen} seconds ago")
                                                break
                                            except ValueError:
                                                print(f"Mesh: Error converting last seen value: {parts[i]}")
                                                pass
                                    
                                    # Find status
                                    status = "UNKNOWN"
                                    for i in range(len(parts)):
                                        if parts[i] == "Status:" or parts[i] == "Status":
                                            if i + 1 < len(parts):
                                                status = parts[i + 1]
                                                break
                                    
                                    # Update routing table with thread safety
                                    with routing_table_lock:
                                        if node_id not in routing_table:
                                            routing_table[node_id] = {
                                                "last_seen": last_seen,
                                                "status": status,
                                                "coord": None
                                            }
                                        else:
                                            current_status = routing_table[node_id].get("status")
                                            routing_table[node_id].update({
                                                "last_seen": last_seen,
                                                "status": status if status != "OFFLINE" else current_status
                                            })
                                    
                                    print(f"Mesh: Routing Table Updated for Node {node_id}: {status}")
                                except (ValueError, IndexError) as e:
                                    print(f"Mesh: Error parsing line: {line} - {e}")
                else:
                    # Just print other lines from mesh serial
                    if line:
                        print(f"Mesh: {line}")
        except Exception as e:
            print(f"Mesh serial error: {e}")
        
        # Small delay to prevent high CPU usage
        time.sleep(0.05)

def handle_beacon_serial(beacon_ser):
    """Thread function to handle beacon serial communication"""
    print(f"Beacon serial thread started on {BEACON_COM_PORT}")
    
    while True:
        try:
            if beacon_ser.in_waiting > 0:
                # Read line from serial port
                line = beacon_ser.readline().decode('utf-8', errors='ignore').strip()
                
                try:
                    if line.startswith("{"):
                        # Parse JSON data
                        data = json.loads(line)
                        
                        # Check if this is a beacon data message
                        if "id" in data and "b" in data and "r" in data:
                            # Extract information
                            beacon_id = data.get("id")  # B1, B2, B3
                            bin_id = data.get("b")      # bin ID
                            rssi = data.get("r")        # RSSI value
                            
                            print(f"Beacon: Received: Beacon {beacon_id}, Bin: {bin_id}, RSSI: {rssi}")
                            
                            # Store the data with thread safety
                            if beacon_id and bin_id is not None and rssi is not None:
                                # Convert RSSI to distance
                                distance = rssi_to_distance(rssi)
                                
                                # Check if this is a new reading or an update
                                with beacon_data_lock:
                                    is_new_reading = beacon_id not in beacon_data[bin_id]
                                    beacon_data[bin_id][beacon_id] = distance
                                
                                # Check if we have data from all three beacons for this bin
                                with beacon_data_lock:
                                    have_all_beacons = all(beacon in beacon_data[bin_id] for beacon in ["B1", "B2", "B3"])
                                
                                if have_all_beacons:
                                    # Calculate position
                                    try:
                                        with beacon_data_lock:
                                            x, y = trilateration(beacon_data[bin_id])
                                        
                                        # Store the calculated coordinates in the routing table with thread safety
                                        with routing_table_lock:
                                            if bin_id not in routing_table:
                                                routing_table[bin_id] = {"status": "active"}
                                            routing_table[bin_id]["coord"] = (x, y)
                                        
                                        # Output result
                                        print(f"Beacon: Position calculated for bin {bin_id}: ({x:.2f}, {y:.2f})")
                                    except Exception as e:
                                        print(f"Beacon: Error in trilateration: {e}")
                    else:
                        # Print non-JSON lines from beacon serial
                        if line:
                            print(f"Beacon: {line}")
                except json.JSONDecodeError:
                    pass
                except Exception as e:
                    print(f"Beacon: Error processing data: {e}")
        except Exception as e:
            print(f"Beacon serial error: {e}")
        
        # Small delay to prevent high CPU usage
        time.sleep(0.05)

def main_processing_thread():
    """Main processing thread that handles routing table updates and server communication"""
    print("Main processing thread started")
    
    # Timeout threshold
    OFFLINE_THRESHOLD = 120 # 2 minutes
    last_sent_time = 0
    
    while True:
        try:
            # Make a copy of the routing table to avoid holding the lock too long
            with routing_table_lock:
                local_routing_table = routing_table.copy()
            
            # Check for offline bins and update their status
            with routing_table_lock:
                for bin_id, bin_data in routing_table.items():
                    # Skip bins that are already offline
                    if bin_data.get("status") == "OFFLINE":
                        continue
                    
                    last_seen = bin_data.get("last_seen", 0)
                    if last_seen > OFFLINE_THRESHOLD:
                        bin_data["status"] = "OFFLINE"
                        print(f"Processing: Bin {bin_id} marked as OFFLINE due to inactivity")
            
            # Update next_nearest for FULL bins
            with routing_table_lock:
                for bin_id, bin_data in routing_table.items():
                    bin_id_int = int(bin_id) if isinstance(bin_id, str) else bin_id
                    
                    # Check if status changed from FULL to not FULL
                    was_full = bin_data.get("was_full", False)
                    is_full_now = bin_data.get("status") == "FULL"
                    
                    # If bin was full but is now OK
                    if was_full and not is_full_now:
                        if "next_nearest" in bin_data:
                            del bin_data["next_nearest"]
                        if "next_nearest_direction" in bin_data:
                            del bin_data["next_nearest_direction"]
                        print(f"Processing: Bin {bin_id} is no longer FULL")
                    
                    # If bin is FULL, find nearest available bin
                    if is_full_now:
                        nearest_bin_id, direction = find_nearest_available_bin(routing_table, bin_id_int)
                        if nearest_bin_id:
                            bin_data["next_nearest"] = nearest_bin_id
                            bin_data["next_nearest_direction"] = direction
                    
                    # Track current status for next iteration
                    bin_data["was_full"] = is_full_now
            
            # Send data to server every 5 seconds
            current_time = time.time()
            if current_time - last_sent_time >= 5:
                # Make a copy of the routing table for serialization
                with routing_table_lock:
                    local_routing_table = routing_table.copy()
                
                # Convert tuple coordinates to lists for JSON serialization
                serializable_routing_table = {}
                for bin_id, bin_data in local_routing_table.items():
                    serializable_bin_data = bin_data.copy()
                    
                    # Convert coordinate tuple to list if it exists
                    if serializable_bin_data.get("coord"):
                        if isinstance(serializable_bin_data["coord"], tuple):
                            serializable_bin_data["coord"] = list(serializable_bin_data["coord"])
                    
                    # Ensure bin_id is a string
                    serializable_routing_table[str(bin_id)] = serializable_bin_data
                
                send_data_to_server(serializable_routing_table)
                last_sent_time = current_time
        except Exception as e:
            print(f"Processing thread error: {e}")
        
        # Small delay to prevent high CPU usage
        time.sleep(0.5)

def main():
    try:
        # Open serial connections
        mesh_ser = serial.Serial(MESH_COM_PORT, BAUD_RATE, timeout=1)
        beacon_ser = serial.Serial(BEACON_COM_PORT, BAUD_RATE, timeout=1)
        
        # Give the serial connections time to initialize
        time.sleep(2)
        
        print(f"Mesh serial opened on {MESH_COM_PORT}")
        print(f"Beacon serial opened on {BEACON_COM_PORT}")
        
        # Create and start threads
        mesh_thread = threading.Thread(target=handle_mesh_serial, args=(mesh_ser,), daemon=True)
        beacon_thread = threading.Thread(target=handle_beacon_serial, args=(beacon_ser,), daemon=True)
        processing_thread = threading.Thread(target=main_processing_thread, daemon=True)
        
        mesh_thread.start()
        beacon_thread.start()
        processing_thread.start()
        
        # Keep the main thread alive
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nProgram terminated by user")
    except serial.SerialException as e:
        print(f"Serial connection error: {e}")
    finally:
        # Close serial ports
        try:
            mesh_ser.close()
            print("Mesh serial connection closed")
        except:
            pass
        
        try:
            beacon_ser.close()
            print("Beacon serial connection closed")
        except:
            pass

if __name__ == "__main__":
    main()
