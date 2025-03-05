import os
from google.cloud import firestore
from dotenv import load_dotenv
from config import QUOTES_COLLECTION

load_dotenv()  # Load environment variables from .env file

# Initialize Firebase with explicit credentials for local testing
def initialize_firestore():
    try:
        # Use credentials from environment or default credentials
        db = firestore.Client(project="sonder-2813")
        print("Firebase initialized successfully")
        return db
    except Exception as e:
        print(f"Error initializing Firebase: {e}")
        return None

def remove_duplicates(db):
    """Remove duplicate quotes from the database"""
    quotes = {}
    COLLECTION = QUOTES_COLLECTION
    
    # Get all quotes
    print(f"Fetching all quotes from collection: {COLLECTION}")
    all_quotes = db.collection(COLLECTION).get()
    
    # Count total quotes
    total_quotes = len(list(all_quotes))
    print(f"Found {total_quotes} total quotes")
    
    # Find duplicates
    for doc in all_quotes:
        quote_text = doc.get("text")
        if quote_text not in quotes:
            quotes[quote_text] = []
        quotes[quote_text].append(doc.id)
    
    # Count duplicates
    duplicate_texts = sum(1 for text, doc_ids in quotes.items() if len(doc_ids) > 1)
    print(f"Found {duplicate_texts} quotes with duplicates")
    
    # Delete duplicates (keep the first one of each)
    batch = db.batch()
    deleted = 0
    
    for text, doc_ids in quotes.items():
        if len(doc_ids) > 1:
            print(f"Found {len(doc_ids)} duplicates for: {text[:30]}...")
            # Skip the first one, delete the rest
            for doc_id in doc_ids[1:]:
                batch.delete(db.collection(COLLECTION).document(doc_id))
                deleted += 1
                
                # Commit in batches of 500 (Firestore limit)
                if deleted % 500 == 0:
                    batch.commit()
                    print(f"Committed batch of {deleted} deletes")
                    batch = db.batch()
    
    # Commit any remaining deletes
    if deleted % 500 != 0:
        batch.commit()
        
    print(f"Removed {deleted} duplicate quotes")

if __name__ == "__main__":
    print("Starting duplicate removal process...")
    
    # Authenticate with gcloud if needed
    os.system("gcloud auth application-default login")
    
    # Initialize Firestore
    db = initialize_firestore()
    
    if db:
        remove_duplicates(db)
        print("Process completed successfully")
    else:
        print("Failed to initialize Firestore. Cannot proceed.") 