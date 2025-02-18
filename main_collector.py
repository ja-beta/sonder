from scraper import main as run_scraper
from nyt_collector import main as run_nyt
from guardian_collector import main as run_guardian

def collect_all_quotes(event, context):
    """Cloud Function to run all collectors"""
    try:
        run_scraper()
        # run_nyt()
        run_guardian()
        return {"success": True}
    except Exception as e:
        print(f"Error in quote collection: {e}")
        return {"success": False, "error": str(e)}
