import os
from dotenv import load_dotenv
from datetime import datetime, timedelta
from firebase_init import db
from quote_extractor import QuoteExtractor
import requests
from google.cloud import firestore
import hashlib
import re

load_dotenv()

# Constants
COLLECTION_NAME = "quotes_v1"  
NYT_API_KEY = os.getenv('NYT_API_KEY')
if not NYT_API_KEY:
    raise ValueError("NYT_API_KEY not found in environment variables")

KEYWORDS = ["war", "fight", "fighting", "hostage", "hostages", "prisoner", "prisoners", "conflict", "battle", "survivor", "survivors"]

class NYTCollector:
    def __init__(self):
        self.quote_extractor = QuoteExtractor()
        self.base_url = "https://api.nytimes.com/svc/topstories/v2/world.json"
        
    def get_articles(self):
        try:
            params = {
                'api-key': NYT_API_KEY
            }
            
            response = requests.get(self.base_url, params=params)
            response.raise_for_status()
            
            articles = response.json().get('results', [])
            print(f"Retrieved {len(articles)} articles from NYT Top Stories")
            return articles
            
        except Exception as e:
            print(f"Error fetching NYT articles: {e}")
            return []
    
    def process_article(self, article):
        try:
            title = article.get('title', '')
            url = article.get('url', '')
            # Get full article content
            content = (article.get('abstract', '') + ' ' + 
                      ''.join([m.get('caption', '') for m in article.get('multimedia', [])]))
            
            content_lower = content.lower()
            matches = {kw for kw in KEYWORDS if kw.lower() in content_lower}
            
            if matches:
                print(f"Found keywords {matches} in article: {title}")
                
                quotes = self.quote_extractor.extract_quotes(content)
                if quotes:
                    article_info = {
                        "url": url,
                        "title": title
                    }
                    return quotes, article_info
                    
            return None, None
            
        except Exception as e:
            print(f"Error processing NYT article: {e}")
            return None, None
    
    def normalize_quote(self, text):
        text = text.lower().strip()
        text = re.sub(r'\s+', ' ', text)
        #text = re.sub(r'[^\w\s]', '', text)
        return text

    def quote_id(self, text):
        return hashlib.sha256(text.encode('utf-8')).hexdigest()

    def store_quotes(self, quotes, article_info):
        batch = db.batch()
        stored_count = 0

        for quote in quotes:
            norm_quote = self.normalize_quote(quote)
            if not norm_quote:
                continue
            doc_id = self.quote_id(norm_quote)
            quote_ref = db.collection(COLLECTION_NAME).document(doc_id)
            if quote_ref.get().exists:
                continue  # Already exists, skip
            batch.set(quote_ref, {
                "text": quote.strip(),
                "article_url": article_info["url"],
                "article_title": article_info["title"],
                "source": "NYT",
                "timestamp": firestore.SERVER_TIMESTAMP,
                "score": None,
                "processed": False
            })
            stored_count += 1

        if stored_count > 0:
            batch.commit()
            print(f"Stored {stored_count} new quotes from: {article_info['title']}")
        
        return stored_count

    def run(self):
        """Main execution function"""
        quotes_processed = 0
        
        articles = self.get_articles()
        for article in articles:
            quotes, article_info = self.process_article(article)
            if quotes and article_info:
                quotes_processed += self.store_quotes(quotes, article_info)
                
        print(f"\nNYT collection completed. Processed {quotes_processed} new quotes.")
        return quotes_processed

def main():
    collector = NYTCollector()
    collector.run()

if __name__ == "__main__":
    main()
