import firebase_admin
from firebase_admin import credentials, firestore
from quote_extractor import QuoteExtractor
import time

try:
    db = firestore.client()
except:
    cred = credentials.Certificate("creds.json")
    firebase_admin.initialize_app(cred)
    db = firestore.client()

# Initialize quote extractor
quote_extractor = QuoteExtractor()

def process_articles():
    """Process all articles that don't have quotes extracted yet"""
    try:
        # Get all articles
        articles = db.collection("news_articles").stream()
        processed_count = 0
        quote_count = 0
        
        for article in articles:
            article_data = article.to_dict()
            
            # Get article data with defaults for missing fields
            title = article_data.get('title', 'No Title')
            content = article_data.get('content', '')
            source = article_data.get('source', 'Unknown Source')
            
            print(f"\nProcessing article: {title}")
            
            if not content:  # Skip if no content
                print(f"Skipping article with no content: {title}")
                continue
                
            # Create a batch write
            batch = db.batch()
            
            # Extract quotes
            quotes = quote_extractor.extract_quotes(content)
            
            if quotes:
                # Store each quote
                for quote in quotes:
                    quote_ref = db.collection("quotes").document()
                    batch.set(quote_ref, {
                        "text": quote,
                        "article_id": article.id,
                        "article_title": title,
                        "source": source,
                        "timestamp": firestore.SERVER_TIMESTAMP,
                        "plotted": False
                    })
                
                # Commit the batch
                batch.commit()
                
                quote_count += len(quotes)
                processed_count += 1
                print(f"Added {len(quotes)} quotes from: {title}")
                
                # Add small delay to avoid overwhelming Firebase
                time.sleep(1)
            
            else:
                print(f"No quotes found in: {title}")
        
        print(f"\nProcessing complete!")
        print(f"Processed {processed_count} articles")
        print(f"Added {quote_count} quotes total")
            
    except Exception as e:
        print(f"Error processing articles: {e}")
        print(f"Last processed article: {title if 'title' in locals() else 'Unknown'}")

if __name__ == "__main__":
    try:
        process_articles()
    except KeyboardInterrupt:
        print("\nProcessing stopped by user")
    except Exception as e:
        print(f"Unexpected error: {e}")