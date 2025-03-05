"""
One-time script to populate display queue with qualifying quotes from an existing collection.
Used for testing the e-paper display functionality without running the full pipeline.
"""
from firebase_init import db
from google.cloud import firestore
import argparse
from display_queue import add_to_queue

def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Populate display queue with qualifying quotes')
    parser.add_argument('--collection', type=str, default='quotes_v6', 
                        help='Source collection name (default: quotes_v6)')
    parser.add_argument('--min-score', type=float, default=0.67,
                        help='Minimum score threshold (default: 0.67)')
    args = parser.parse_args()
    
    print(f"Finding quotes with score >= {args.min_score} in collection '{args.collection}'")
    
    # Reuse the function from display_queue.py
    added = add_to_queue(args.collection, args.min_score)
    
    print(f"Done! Added {added} quotes to the display queue.")
    
    # Additional stats
    if added > 0:
        # Count total quotes in display queue
        queue_size = len(list(db.collection("display_queue").get()))
        print(f"Display queue now contains {queue_size} total quotes")
        
        # Count undisplayed quotes
        undisplayed = len(list(db.collection("display_queue").where("displayed", "==", False).get()))
        print(f"Of these, {undisplayed} have not been displayed yet")
    
if __name__ == "__main__":
    main() 