import firebase_admin
from firebase_admin import credentials, firestore

def initialize_firebase():
    """Initialize Firebase if not already initialized"""
    if not firebase_admin._apps:
        cred = credentials.Certificate("creds.json")
        firebase_admin.initialize_app(cred)
    return firestore.client()

db = initialize_firebase()
