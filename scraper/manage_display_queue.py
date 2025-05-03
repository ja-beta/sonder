#!/usr/bin/env python3
import json
import firebase_admin
from firebase_admin import credentials, firestore
import argparse
import os
import random


def initialize_firebase():
    """Initialize Firebase if not already initialized"""
    if not firebase_admin._apps:
        # Use Application Default Credentials
        cred = credentials.ApplicationDefault()
        firebase_admin.initialize_app(cred, {
            'projectId': 'sonder-2813',
        })
    return firestore.client()

def load_quotes(json_file):
    """Load quotes from JSON file"""
    with open(json_file, 'r', encoding='utf-8') as file:
        quotes = json.load(file)
    return quotes

def clear_display_queue(db):
    """Clear all documents in the display_queue collection"""
    queue_ref = db.collection('display_queue')
    docs = queue_ref.stream()
    
    count = 0
    for doc in docs:
        doc.reference.delete()
        count += 1
    
    return count

def populate_display_queue(db, quotes):
    """Add quotes to the display queue with displayed=False"""
    queue_ref = db.collection('display_queue')
    
    count = 0
    for quote in quotes:
        queue_ref.add({
            'text': quote,
            'displayed': False,
            'created_at': firestore.SERVER_TIMESTAMP
        })
        count += 1
    
    return count

def reset_displayed_status(db):
    """Reset all quotes in the queue to displayed=False"""
    queue_ref = db.collection('display_queue')
    docs = queue_ref.stream()
    
    count = 0
    for doc in docs:
        doc.reference.update({'displayed': False})
        count += 1
    
    return count

def check_and_reset_if_empty(db):
    """Check if there are any undisplayed quotes, reset all if none"""
    queue_ref = db.collection('display_queue')
    undisplayed = list(queue_ref.where(filter=firestore.FieldFilter('displayed', '==', False)).limit(1).stream())
    
    if not undisplayed:
        count = reset_displayed_status(db)
        return True, count
    
    return False, 0

def main():
    parser = argparse.ArgumentParser(description='Manage the display quote queue')
    parser.add_argument('--clear', action='store_true', help='Clear the current display queue')
    parser.add_argument('--populate', action='store_true', help='Populate queue with quotes from good_quotes.json')
    parser.add_argument('--reset', action='store_true', help='Reset displayed status of all quotes')
    parser.add_argument('--check-reset', action='store_true', help='Check if queue is empty and reset if needed')
    parser.add_argument('--json-file', default='good_quotes.json', help='Path to quotes JSON file')
    
    args = parser.parse_args()
    
    db = initialize_firebase()
    
    if args.clear:
        count = clear_display_queue(db)
        print(f"Cleared {count} quotes from the display queue")
    
    if args.populate:
        # Get the directory of the current script
        script_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.join(script_dir, args.json_file)
        
        quotes = load_quotes(json_path)
        
        # Shuffle quotes for randomness
        random.shuffle(quotes)
        
        count = populate_display_queue(db, quotes)
        print(f"Added {count} quotes to the display queue")
    
    if args.reset:
        count = reset_displayed_status(db)
        print(f"Reset {count} quotes to undisplayed status")
    
    if args.check_reset:
        reset_needed, count = check_and_reset_if_empty(db)
        if reset_needed:
            print(f"Display queue was empty. Reset {count} quotes to undisplayed status")
        else:
            print("Display queue still has undisplayed quotes")

if __name__ == "__main__":
    main()
