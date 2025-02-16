import requests
from bs4 import BeautifulSoup
import json
import firebase_admin
from firebase_admin import credentials, firestore
import time
from requests.exceptions import RequestException, Timeout
import re

cred = credentials.Certificate("creds.json")
firebase_admin.initialize_app(cred)
db = firestore.client()

class QuoteExtractor:
    def __init__(self):
        self.QUOTE_MARKS = {
            '"': '"',    
            '“': '”',       
            "‘": "’",    
        }

        #'"'"’‘“”
    
    def extract_quotes(self, content):
        """Extract clean quotes using multiple quote marks"""
        valid_quotes = []
        
        for start_quote, end_quote in self.QUOTE_MARKS.items():
            start_escaped = re.escape(start_quote)
            end_escaped = re.escape(end_quote)
            pattern = f'{start_escaped}([^{end_escaped}]*){end_escaped}'
            quotes = re.findall(pattern, content)
            
            for quote in quotes:
                quote = quote.strip()
                if (len(quote) > 10 and  # Minimum length
                    len(quote) < 300 and  # Maximum length
                    not quote.startswith(('http', 'www'))):  # No URLs
                    valid_quotes.append(quote)
        
        return valid_quotes

quote_extractor = QuoteExtractor()

def store_in_firebase(article, matched_terms, source):
    """Stores article and its quotes in Firebase using batch write."""
    try:
        batch = db.batch()
        
        # Store article
        article_ref = db.collection("news_articles").document()
        article_id = article_ref.id
        
        batch.set(article_ref, {
            "title": article["title"],
            "url": article["url"],
            "content": article["content"],
            "matched_terms": matched_terms,
            "source": source,
            "timestamp": firestore.SERVER_TIMESTAMP,
            "processed": False,
            "plotted": False
        })
        
        # Extract and store quotes using the QuoteExtractor
        quotes = quote_extractor.extract_quotes(article["content"])
        for quote in quotes:
            quote_ref = db.collection("quotes").document()
            batch.set(quote_ref, {
                "text": quote,
                "article_id": article_id,
                "article_title": article["title"],
                "source": source,
                "timestamp": firestore.SERVER_TIMESTAMP,
                "plotted": False
            })
        
        # Commit the batch
        batch.commit()
        
        print(f"Stored article: {article['title']} with {len(quotes)} quotes")
        return True
        
    except Exception as e:
        print(f"Error storing in Firebase: {e}")
        return False