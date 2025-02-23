from firebase_init import db
from google.cloud import firestore
from config import QUOTES_COLLECTION, KEYWORDS
from quote_extractor import QuoteExtractor
import requests
from bs4 import BeautifulSoup
import time
from requests.exceptions import RequestException, Timeout

COLLECTION = QUOTES_COLLECTION

quote_extractor = QuoteExtractor()

NEWS_SITES = {
    "BBC": {
        "url": "https://www.bbc.com/news",
        "article_link_pattern": "/news/",
        "base_url": "https://www.bbc.com"
    },
    "NPR": {
        "url": "https://www.npr.org/sections/world/",
        "article_link_pattern": "/sections/",
        "base_url": "https://www.npr.org"
    },
    "AP News": {
        "url": "https://apnews.com/hub/world-news",
        "article_link_pattern": "/article/",
        "base_url": "https://apnews.com"
    },
    "The Guardian": {
        "url": "https://www.theguardian.com/world",
        "article_link_pattern": "/world/",
        "base_url": "https://www.theguardian.com"
    },
}

def get_article_links(site_name, site_config, timeout=10):
    """Scrapes article links with timeout and error handling"""
    print(f"Scraping {site_name}...")
    headers = {
        'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36',
        'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
        'Accept-Language': 'en-US,en;q=0.5'
    }
    
    try:
        response = requests.get(
            site_config["url"], 
            headers=headers,
            timeout=timeout,
            allow_redirects=True
        )
        response.raise_for_status()
        
        soup = BeautifulSoup(response.text, "html.parser")
        links = []
        
        for a in soup.find_all("a", href=True):
            href = a["href"]
            if site_config["article_link_pattern"] in href:
                full_url = href if href.startswith("http") else f"{site_config['base_url']}{href}"
                links.append(full_url)
        
        unique_links = list(set(links))
        print(f"Found {len(unique_links)} unique articles on {site_name}")
        return unique_links
        
    except Timeout:
        print(f"Timeout while scraping {site_name}, moving to next site")
        return []
    except RequestException as e:
        print(f"Network error while scraping {site_name}: {e}")
        return []
    except Exception as e:
        print(f"Unexpected error scraping {site_name}: {e}")
        return []
    


def scrape_article(url, site_name, timeout=10):
    try:
        headers = {
            'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5'
        }
        
        response = requests.get(url, headers=headers, timeout=timeout)
        response.raise_for_status()
        soup = BeautifulSoup(response.text, "html.parser")
        
        title_element = soup.find("h1")
        title = title_element.text.strip() if title_element else "No title"
        
        invalid_titles = {
            "Business", "Climate", "Sport", "Technology", "Entertainment",
            "NewsNews", "No title", 
            "Analysis", "Art & Design", "Movies", "Opinion", "Review",
            "Menu", "Navigation", "Search"
        }
        
        if (title in invalid_titles or 
            len(title) < 10 or 
            any(section in title for section in ["Section", "Category", "Page"])):
            print(f"Skipping article with invalid title: {title}")
            return None
        
        paragraphs = soup.find_all("p")
        content = " ".join([p.text.strip() for p in paragraphs])
        
        if len(content) < 300: 
            print(f"Skipping article with insufficient content: {title}")
            return None
            
        return {"url": url, "title": title, "content": content}
    except Exception as e:
        print(f"Error scraping article {url}: {e}")
        return None

def search_keywords(article):
    """Checks if article contains any target keywords."""
    if not article:
        return None
        
    content = article["content"].lower()
    title = article["title"].lower()
    
    matches = {kw for kw in KEYWORDS if kw.lower() in content or kw.lower() in title}
    
    if matches:
        print(f"Found keywords {matches} in article: {article['title']}")
        return list(matches)
    return None


    

def check_quote_exists(quote_text):
    """Check if quote already exists in Firebase"""
    docs = db.collection(COLLECTION).where("text", "==", quote_text).limit(1).get()
    return len(docs) > 0

def store_quotes(quotes, article_info, source):
    """Store new quotes in Firebase"""
    batch = db.batch()
    stored_count = 0

    for quote in quotes:
        if not check_quote_exists(quote):
            quote_ref = db.collection(COLLECTION).document()
            batch.set(quote_ref, {
                "text": quote,
                "article_url": article_info["url"],
                "article_title": article_info["title"],
                "source": source,
                "timestamp": firestore.SERVER_TIMESTAMP,
                "score": None,
                "processed": False
            })
            stored_count += 1

    if stored_count > 0:
        batch.commit()
        print(f"Stored {stored_count} new quotes from: {article_info['title']}")
    
    return stored_count

def main():
    quotes_processed = 0
    max_articles_per_site = 20
    
    for site_name, site_config in NEWS_SITES.items():
        try:
            print(f"\nProcessing {site_name}...")
            article_links = get_article_links(site_name, site_config)
            
            article_links = article_links[:max_articles_per_site]
            print(f"Processing {len(article_links)} articles from {site_name}")
            
            for link in article_links:
                try:
                    time.sleep(2) 
                    
                    article = scrape_article(link, site_name)
                    if article:
                        matched_terms = search_keywords(article)
                        if matched_terms:
                            quotes = quote_extractor.extract_quotes(article["content"])
                            if quotes:
                                article_info = {
                                    "url": article["url"],
                                    "title": article["title"]
                                }
                                quotes_processed += store_quotes(quotes, article_info, site_name)
                        else:
                            print(f"No relevant keywords found in: {article['title']}")
                            
                except Exception as e:
                    print(f"Error processing article {link}: {e}")
                    continue
                
            time.sleep(5)
            
        except Exception as e:
            print(f"Error processing site {site_name}: {e}")
            continue
    
    print(f"\nScript completed. Processed {quotes_processed} new quotes.")
    return quotes_processed

if __name__ == "__main__":
    main()
