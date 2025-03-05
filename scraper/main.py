import functions_framework
from scraper import main as run_scraper
from guardian_collector import main as run_guardian
from process_quotes import main as process_quotes

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
        
        results = {
            "scraper": 0,
            "guardian": 0,
            "processed": 0,
            "duplicates_removed": 0,
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
        
        # Run a quick cleanup on recent quotes
        from scraper import clean_recent_duplicates
        results["duplicates_removed"] = clean_recent_duplicates(hours=1)
        
        return {
            "success": True,
            "message": f"Pipeline completed. Processed {results['processed']} quotes, removed {results['duplicates_removed']} duplicates",
            "results": results
        }
        
    except Exception as e:
        error_msg = f"Pipeline error: {str(e)}"
        return {
            "success": False,
            "message": error_msg,
            "results": results
        }
