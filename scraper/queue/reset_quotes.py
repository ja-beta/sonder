"""
Simple script to reset all quotes in the display queue to undisplayed state.
"""
import os
from google.cloud import firestore
import firebase_admin
from firebase_admin import credentials
from firebase_admin import firestore as admin_firestore

def reset_all_quotes():
    """Reset all quotes in display queue to undisplayed state"""
    try:
        # Try to initialize using Firebase Admin first
        try:
            # Try with application default credentials
            app = firebase_admin.initialize_app()
            db = admin_firestore.client()
            print("Initialized with application default credentials")
        except Exception as e:
            print(f"Could not initialize with default credentials: {e}")
            # Fallback to direct firestore client
            db = firestore.Client()
            print("Initialized with direct Firestore client")
        
        print("Resetting all quotes in display queue...")
        # Get all quotes in display queue
        quotes_ref = db.collection('display_queue')
        batch = db.batch()
        count = 0
        
        # Update each quote to reset displayed status
        for doc in quotes_ref.stream():
            batch.update(doc.reference, {
                'displayed': False,
                'display_timestamp': None,
                'display_device_id': None
            })
            count += 1
            # Commit in batches of 450
            if count % 450 == 0:
                batch.commit()
                print(f"Reset {count} quotes...")
                batch = db.batch()
        
        # Commit any remaining updates
        if count % 450 != 0:
            batch.commit()
        
        print(f"Successfully reset {count} quotes")
        return count
        
    except Exception as e:
        print(f"Error resetting quotes: {e}")
        print("\nTips to fix this error:")
        print("1. Make sure you're authenticated with gcloud CLI: gcloud auth application-default login")
        print("2. Set the GOOGLE_CLOUD_PROJECT environment variable: export GOOGLE_CLOUD_PROJECT='sonder-2813'")
        print("3. Or run with project ID explicitly: GOOGLE_CLOUD_PROJECT='sonder-2813' python reset_quotes.py")
        return 0

if __name__ == "__main__":
    reset_all_quotes() 