import os
from dotenv import load_dotenv
from config import QUOTES_COLLECTION, KEYWORDS
from firebase_init import db
from quote_extractor import QuoteExtractor
import requests
from google.cloud import firestore
import hashlib
import re

load_dotenv()

COLLECTION_NAME = QUOTES_COLLECTION

# Get API key but don't fail immediately
GUARDIAN_API_KEY = os.environ.get("GUARDIAN_API_KEY")

class GuardianCollector:
    def __init__(self):
        self.quote_extractor = QuoteExtractor()
        self.base_url = "https://content.guardianapis.com/search"
        
    def get_articles(self, hours=24):
        """Get articles from the last n hours"""
        try:
            params = {
                'api-key': GUARDIAN_API_KEY,
                'section': 'world',
                'show-fields': 'bodyText,headline',  
                'page-size': 5, 
                'order-by': 'newest',
                'show-tags': 'keyword'
            }
            
            response = requests.get(self.base_url, params=params)
            response.raise_for_status()
            
            data = response.json()
            articles = data.get('response', {}).get('results', [])
            print(f"Retrieved {len(articles)} articles from The Guardian API")
            return articles
            
        except Exception as e:
            print(f"Error fetching Guardian articles: {e}")
            return []
    
    def process_article(self, article):
        """Process a single Guardian article"""
        try:
            fields = article.get('fields', {})
            title = fields.get('headline', '')
            content = fields.get('bodyText', '')
            url = article.get('webUrl', '')
            
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
            print(f"Error processing Guardian article: {e}")
            return None, None
    
    def normalize_quote(self, text):
        text = text.lower().strip()
        text = re.sub(r'\s+', ' ', text)
        #text = re.sub(r'[^\w\s]', '', text)
        return text

    def quote_id(self, text):
        return hashlib.sha256(text.encode('utf-8')).hexdigest()

    def store_quotes(self, quotes, article_info):
        """Store quotes in Firebase with duplicate prevention using hash-based document IDs"""
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
                "source": "The Guardian",
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
        max_quotes = 50
        
        articles = self.get_articles()
        for article in articles:
            if quotes_processed >= max_quotes:
                print(f"\nReached maximum quote limit ({max_quotes})")
                break
                
            quotes, article_info = self.process_article(article)
            if quotes and article_info:
                quotes_processed += self.store_quotes(quotes, article_info)
                
        print(f"\nGuardian collection completed. Processed {quotes_processed} new quotes.")
        return quotes_processed

def main():
    # Only check for API key when this function is actually called
    if not GUARDIAN_API_KEY:
        raise ValueError("GUARDIAN_API_KEY not found in environment variables")
    
    collector = GuardianCollector()
    collector.run()

if __name__ == "__main__":
    main()