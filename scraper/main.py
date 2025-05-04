import functions_framework
from scraper import main as run_scraper
from guardian_collector import main as run_guardian
from process_quotes import main as process_quotes
from sonder_queue.display_queue import add_to_queue
from firebase_init import db

@functions_framework.http
def run_pipeline(request):
    """HTTP Cloud Function that runs the complete quote collection pipeline."""
    try:
        # Get collection name from query parameters or use environment variable
        if request.method == 'POST' and request.is_json:
            data = request.get_json()
            collection_name = data.get('collection')
        else:
            collection_name = request.args.get('collection')
            
        # Update the collection name in config if specified
        if collection_name:
            print(f"Using custom collection: {collection_name}")
            from config import update_collection
            update_collection(collection_name)
            collection_name = update_collection(collection_name)
        else:
            from config import QUOTES_COLLECTION
            collection_name = QUOTES_COLLECTION
            
        print(f"Quote collection: {collection_name}")
        
        # Print information about existing data
        try:
            quotes_count = len(list(db.collection(collection_name).limit(1000).get()))
            print(f"Current quotes in collection: approximately {quotes_count} (limited to 1000)")
            
            processed_urls_count = len(list(db.collection("processed_urls").limit(1000).get()))
            print(f"Current processed URLs: approximately {processed_urls_count} (limited to 1000)")
        except Exception as e:
            print(f"Error checking collection stats: {e}")
        
        results = {
            "scraper": 0,
            "guardian": 0,
            "processed": 0,
            "duplicates_removed": 0,
            "display_queue_added": 0,
            "errors": []
        }
        
        print("\n=== Starting Quote Collection ===")
        results["scraper"] = run_scraper()
        
        # Guardian collection
        try:
            results["guardian"] = run_guardian()
        except Exception as e:
            results["errors"].append(f"Guardian collection error: {str(e)}")
        
        # Process quotes
        results["processed"] = process_quotes()
        
        
        # Add eligible quotes to display queue
        results["display_queue_added"] = add_to_queue(collection_name)
        
        # Print stats after run
        try:
            new_quotes_count = len(list(db.collection(collection_name).limit(1000).get()))
            print(f"Quotes after run: approximately {new_quotes_count} (limited to 1000)")
            
            new_processed_urls_count = len(list(db.collection("processed_urls").limit(1000).get()))
            print(f"Processed URLs after run: approximately {new_processed_urls_count} (limited to 1000)")
            print(f"New processed URLs: approximately {new_processed_urls_count - processed_urls_count} (based on estimate)")
        except Exception as e:
            print(f"Error checking post-run stats: {e}")
        
        return {
            "success": True,
            "message": f"Pipeline completed. Processed {results['processed']} quotes, removed {results['duplicates_removed']} duplicates, added {results['display_queue_added']} to display queue",
            "results": results
        }
        
    except Exception as e:
        error_msg = f"Pipeline error: {str(e)}"
        return {
            "success": False,
            "message": error_msg,
            "results": results
        }
