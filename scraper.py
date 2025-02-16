import requests
from bs4 import BeautifulSoup
import json
import firebase_admin
from firebase_admin import credentials, firestore
import time
from requests.exceptions import RequestException, Timeout
from quote_extractor import QuoteExtractor

cred = credentials.Certificate("creds.json")
firebase_admin.initialize_app(cred)
db = firestore.client()

quote_extractor = QuoteExtractor()

KEYWORDS = ["war", "fight", "fighting", "hostage", "hostages", "prisoner", "prisoners", "conflict", "battle", "survivor", "survivors"]

# Focus on reliable free news sources
NEWS_SITES = {
    "BBC": {
        "url": "https://www.bbc.com/news",
        "article_link_pattern": "/news/",
        "base_url": "https://www.bbc.com"
    },
    "Reuters": {
        "url": "https://www.reuters.com/world/",
        "article_link_pattern": "/world/",
        "base_url": "https://www.reuters.com"
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
    }
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

def check_article_exists(url):
    """Check if article already exists in Firebase"""
    try:
        docs = db.collection("news_articles").where("url", "==", url).limit(1).get()
        return len(docs) > 0
    except Exception as e:
        print(f"Error checking article existence: {e}")
        return False

def scrape_article(url, site_name, timeout=10):
    """Scrapes individual article with improved title extraction"""
    try:
        headers = {
            'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5'
        }
        
        response = requests.get(url, headers=headers, timeout=timeout)
        response.raise_for_status()
        soup = BeautifulSoup(response.text, "html.parser")
        
        # Special handling for BBC (only exception needed)
        if site_name == "BBC":
            title_element = soup.find("h1", {"id": "main-heading"})
        else:
            title_element = soup.find("h1")
            
        title = title_element.text.strip() if title_element else "No title"
        
        # Skip if title looks invalid
        if title in ["NewsNews", "No title", "Business", "Analysis", "Art & Design", "Movies", "Climate"]:
            print(f"Skipping article with invalid title: {title}")
            return None
        
        # Get content
        paragraphs = soup.find_all("p")
        content = " ".join([p.text.strip() for p in paragraphs])
        
        # Validate content
        if len(content) < 100:  # Skip if content is too short
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
        
    # Convert article content and title to lowercase for case-insensitive matching
    content = article["content"].lower()
    title = article["title"].lower()
    
    # Search in both title and content
    matches = []
    for kw in KEYWORDS:
        kw = kw.lower()
        if kw in content or kw in title:
            matches.append(kw)
            
    # Debug output to verify matches
    if matches:
        print(f"Found keywords {matches} in article: {article['title']}")
    
    return matches if matches else None

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

def main():
    articles_processed = 0
    
    for site_name, site_config in NEWS_SITES.items():
        try:
            print(f"\nProcessing {site_name}...")
            article_links = get_article_links(site_name, site_config)
            
            # Process all articles
            for link in article_links:
                try:
                    # Skip if article already exists
                    if check_article_exists(link):
                        print(f"Skipping existing article: {link}")
                        continue
                        
                    # Add delay between requests
                    time.sleep(2)
                    
                    article = scrape_article(link, site_name)
                    if article:
                        matched_terms = search_keywords(article)
                        if matched_terms:
                            if store_in_firebase(article, matched_terms, site_name):
                                articles_processed += 1
                        else:
                            print(f"Skipped: {article['title']} (No keyword matches)")
                except Exception as e:
                    print(f"Error processing article {link}: {e}")
                    continue
                
            # Add delay between sites
            time.sleep(5)
            
        except KeyboardInterrupt:
            print(f"\nStopping gracefully at {site_name}...")
            break
        except Exception as e:
            print(f"Error processing site {site_name}: {e}")
            print("Moving to next site...")
            continue
    
    print(f"\nScript completed. Processed {articles_processed} articles.")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nScript stopped by user")
    except Exception as e:
        print(f"Unexpected error: {e}")
