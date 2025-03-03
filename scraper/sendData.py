import socket
import time

def test_connection(ip_address="10.23.8.215", port=23):
    """Simple test to verify two-way communication with FluidNC"""
    try:
        print(f"Connecting to {ip_address}:{port}")
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((ip_address, port))
        print("Connected successfully!")
        
        # Send a simple version query command
        print("\nSending command: $I")
        s.send(b"$I\n")
        time.sleep(0.1)
        
        # Read response
        response = s.recv(1024).decode()
        print(f"Response received:\n{response}")
        
        s.close()
        print("\nConnection test completed!")
        return True
    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    test_connection()
