import functions_framework
import firebase_admin
from firebase_admin import credentials, firestore
import traceback
import json

# Initialize Firebase
if not firebase_admin._apps:
    cred = credentials.ApplicationDefault()
    firebase_admin.initialize_app(cred, {
        'projectId': 'sonder-2813',
    })
db = firestore.client()

@functions_framework.http
def serve_quote_api(request):
    """Serves quotes to e-paper displays with robust error handling."""
    try:
        # Get device ID from request
        if request.method == 'GET':
            device_id = request.args.get('device_id', 'unknown')
        else:
            try:
                request_json = request.get_json(silent=True)
                device_id = request_json.get('device_id', 'unknown')
            except:
                device_id = 'unknown'
        
        print(f"Quote requested by device: {device_id}")
        
        # Simplified query - remove the order_by which requires a composite index
        # Just get any undisplayed quote
        quotes_ref = db.collection('display_queue')
        query = quotes_ref.where(filter=firestore.FieldFilter('displayed', '==', False)).limit(1)
        
        # First check if we have any matches without a transaction
        docs = list(query.stream())
        if not docs:
            # No undisplayed quotes found - reset all quotes to undisplayed
            print("Queue empty - resetting all quotes to undisplayed")
            reset_count = 0
            all_quotes = quotes_ref.stream()
            for quote_doc in all_quotes:
                quote_doc.reference.update({'displayed': False})
                reset_count += 1
            
            print(f"Reset {reset_count} quotes to undisplayed")
            
            # Try again to get an undisplayed quote
            docs = list(query.stream())
            if not docs:
                return {'success': False, 'message': 'No quotes available even after reset'}
        
        # Get the document
        doc = docs[0]
        doc_id = doc.id
        doc_data = doc.to_dict()
        
        # Update it as a separate operation
        doc.reference.update({
            'displayed': True,
            'display_timestamp': firestore.SERVER_TIMESTAMP,
            'display_device_id': device_id
        })
        
        # Return the quote
        return {
            'success': True,
            'quote': doc_data.get('text', ''),
            'quote_id': doc_id
        }
        
    except Exception as e:
        # Log the full error
        error_details = traceback.format_exc()
        print(f"Error serving quote: {str(e)}")
        print(error_details)
        
        return {
            'success': False,
            'message': f"Server error: {str(e)}",
            'error_type': type(e).__name__
        }, 500 