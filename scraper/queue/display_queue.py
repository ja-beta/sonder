"""
Module for managing the display queue of quotes for e-paper displays.
"""
from firebase_init import db
from google.cloud import firestore

def add_to_queue(quotes_collection, min_score=0.67):
    """
    Add high-scoring quotes to the display queue.
    
    Works with recently processed quotes in the main collection.
    Prevents duplicates with existing queue entries.
    """
    DISPLAY_QUEUE = "display_queue"
    print(f"\n=== Adding high-scoring quotes to display queue ===")
    
    # Get all high-scoring quotes 
    eligible_quotes = (
        db.collection(quotes_collection)
        .where("score", ">=", min_score)
        .get()
    )
    
    # Get existing queue entries to avoid duplicates
    existing_source_ids = set()
    display_queue_docs = db.collection(DISPLAY_QUEUE).get()
    for doc in display_queue_docs:
        source_id = doc.to_dict().get("source_id")
        if source_id:
            existing_source_ids.add(source_id)
    
    # Add new quotes to display queue
    batch = db.batch()
    added = 0
    
    for doc in eligible_quotes:
        doc_id = doc.id
        
        # Skip if already in queue
        if doc_id in existing_source_ids:
            continue
            
        # Create display queue entry
        queue_ref = db.collection(DISPLAY_QUEUE).document()
        data = doc.to_dict()
        
        # Add display tracking fields
        data.update({
            "displayed": False,
            "display_timestamp": None,
            "display_device_id": None,
            "source_id": doc_id
        })
        
        # Add to batch
        batch.set(queue_ref, data)
        added += 1
        
        # Commit in batches of 500 (Firestore limit)
        if added % 500 == 0:
            batch.commit()
            print(f"Added {added} quotes to display queue")
            batch = db.batch()
    
    # Commit any remaining adds
    if added % 500 != 0:
        batch.commit()
    
    print(f"Added {added} new quotes to display queue")
    return added

def serve_quote(device_id):
    """
    Get the next undisplayed quote and mark it as displayed.
    
    Args:
        device_id: Identifier of the requesting device
        
    Returns:
        Dictionary with quote data or None if no quotes available
    """
    # Get the next undisplayed quote
    quotes_ref = db.collection('display_queue')
    query = quotes_ref.where('displayed', '==', False).order_by('timestamp').limit(1)
    
    # Run as transaction to prevent race conditions
    transaction = db.transaction()
    
    @firestore.transactional
    def get_next_quote(transaction, query):
        quotes = list(query.get())
        if not quotes:
            return None
            
        # Get the first undisplayed quote
        quote_doc = quotes[0]
        quote_data = quote_doc.to_dict()
        
        # Mark it as displayed
        transaction.update(quote_doc.reference, {
            'displayed': True,
            'display_timestamp': firestore.SERVER_TIMESTAMP,
            'display_device_id': device_id
        })
        
        return {
            'id': quote_doc.id,
            'text': quote_data.get('text', ''),
            'source': quote_data.get('source', 'Unknown')
        }
    
    # Execute the transaction
    return get_next_quote(transaction, query) 