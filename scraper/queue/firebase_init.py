import firebase_admin
from firebase_admin import credentials
from firebase_admin import firestore

def initialize_firebase():
    try:
        firebase_admin.initialize_app()
        db = firestore.client()
        return db
    except Exception as e:
        print(f"Error initializing Firebase: {e}")
        return None

db = initialize_firebase()
