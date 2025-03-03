import functions_framework
from scraper import main as run_scraper
from guardian_collector import main as run_guardian
from process_quotes import main as process_quotes

@functions_framework.http
def run_pipeline(request):
    """HTTP Cloud Function that runs the complete quote collection pipeline."""
    try:
        results = {
            "scraper": 0,
            "guardian": 0,
            "processed": 0,
            "errors": []
        }
        
        print("\n=== Starting Quote Collection ===")
        results["scraper"] = run_scraper()
        
        results["guardian"] = run_guardian()
        
        results["processed"] = process_quotes()
        
        return {
            "success": True,
            "message": f"Pipeline completed. Processed {results['processed']} quotes",
            "results": results
        }
        
    except Exception as e:
        error_msg = f"Pipeline error: {str(e)}"
        return {
            "success": False,
            "message": error_msg,
            "results": results
        }
