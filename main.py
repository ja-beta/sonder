from scraper import main as run_scraper
from guardian_collector import main as run_guardian
from process_quotes import main as process_quotes
import time

def run_all():
    """Run the complete pipeline with detailed reporting"""
    results = {
        "scraper": 0,
        "guardian": 0,
        "processed": 0,
        "errors": []
    }
    
    try:
        print("\n=== Starting Quote Collection ===")
        
        print("\n--- Running Web Scraper ---")
        results["scraper"] = run_scraper()
        print(f"Scraped quotes: {results['scraper']}")
        time.sleep(2)
        
        print("\n--- Running Guardian Collector ---")
        results["guardian"] = run_guardian()
        print(f"Guardian quotes: {results['guardian']}")
        time.sleep(2)
        
        print("\n--- Processing Quotes ---")
        results["processed"] = process_quotes()
        print(f"Processed quotes: {results['processed']}")
        
        print("\n=== Pipeline Summary ===")
        print(f"Scraped quotes: {results['scraper']}")
        print(f"Guardian quotes: {results['guardian']}")
        print(f"Processed quotes: {results['processed']}")
        
        return {"success": True, "results": results}
        
    except Exception as e:
        error_msg = f"Pipeline error: {str(e)}"
        results["errors"].append(error_msg)
        print(f"\nError: {error_msg}")
        return {"success": False, "results": results}

def main():
    """Main entry point with argument handling"""
    import sys
    
    if len(sys.argv) > 1:
        command = sys.argv[1]
        if command == "scrape":
            run_scraper()
        elif command == "guardian":
            run_guardian()
        elif command == "process":
            process_quotes()
        else:
            print("Invalid command. Use: scrape, guardian, or process")
    else:
        run_all()

if __name__ == "__main__":
    main()