import firebase_admin
from firebase_admin import credentials, firestore
import csv
import time
from datetime import datetime

try:
    db = firestore.client()
except:
    cred = credentials.Certificate("creds.json")
    firebase_admin.initialize_app(cred)
    db = firestore.client()

def export_quotes_to_csv():
    """Export all quotes to a CSV file with relevant fields"""
    try:
        quotes = db.collection("quotes_v1").stream()
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"quotes_export_{timestamp}.csv"
        
        fields = ['quote_id', 'text', 'article_title', 'source', 'manual_score', 'notes']
        
        with open(filename, 'w', newline='', encoding='utf-8') as file:
            writer = csv.DictWriter(file, fieldnames=fields)
            writer.writeheader()
            
            quote_count = 0
            for quote in quotes:
                quote_data = quote.to_dict()
                
                writer.writerow({
                    'quote_id': quote.id,
                    'text': quote_data.get('text', ''),
                    'article_title': quote_data.get('article_title', ''),
                    'source': quote_data.get('source', ''),
                    'manual_score': '',  
                    'notes': ''        
                })
                
                quote_count += 1
                
                if quote_count % 100 == 0:
                    print(f"Exported {quote_count} quotes...")
        
        print(f"\nExport complete!")
        print(f"Exported {quote_count} quotes to {filename}")
        
    except Exception as e:
        print(f"Error exporting quotes: {e}")

if __name__ == "__main__":
    try:
        export_quotes_to_csv()
    except KeyboardInterrupt:
        print("\nExport stopped by user")
    except Exception as e:
        print(f"Unexpected error: {e}")