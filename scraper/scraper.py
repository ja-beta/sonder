from firebase_init import db
from google.cloud import firestore
from config import QUOTES_COLLECTION, KEYWORDS
from quote_extractor import QuoteExtractor
import requests
from bs4 import BeautifulSoup
import time
from requests.exceptions import RequestException, Timeout
import argparse
from datetime import datetime, timedelta
import hashlib
import re

COLLECTION = QUOTES_COLLECTION

quote_extractor = QuoteExtractor()

NEWS_SITES = {
    "BBC": {
        "url": "https://www.bbc.com/",
        "article_link_pattern": "/news/",
        "base_url": "https://www.bbc.com",
        # BBC articles have several formats:
        # 1. /news/articles/c5ype8w6ynwo - main format
        # 2. /news/world-europe-12345678 - regional format
        # 3. /news/uk-12345678 - UK format
        # 4. /news/technology-12345678 - topical format
        "article_regex": r"/news/(articles/[a-z0-9]+|[a-z0-9\-]+)"
    },
    # "NPR": {
    #     "url": "https://www.npr.org/sections/world/",
    #     "article_link_pattern": "/sections/",
    #     "base_url": "https://www.npr.org"
    # },
    "AP News": {
        "url": "https://apnews.com/hub/world-news",
        "article_link_pattern": "/article/",
        "base_url": "https://apnews.com",
        # AP News articles look like /article/israel-hamas-war-gaza-casualties-2024-xxx
        "article_regex": r"/article/[a-z0-9\-]+"
    },
    "The Guardian": {
        "url": "https://www.theguardian.com/world",
        "article_link_pattern": "/world/",
        "base_url": "https://www.theguardian.com",
        # Guardian articles have year/month/day in the URL 
        # Example: /world/2023/may/01/article-title
        "article_regex": r"/world/\d{4}/[a-z]{3}/\d{2}/[a-z0-9\-]+"
    },
    "Al Jazeera": {
        "url": "https://www.aljazeera.com/news/",
        "article_link_pattern": "/news/",
        "base_url": "https://www.aljazeera.com",
        # Al Jazeera articles typically have year/month/day in them
        # Example: /news/2023/5/1/article-title
        "article_regex": r"/news/\d{4}/\d{1,2}/\d{1,2}/[a-z0-9\-]+"
    },
    # "Times of Israel": {
    #     "url": "https://www.timesofisrael.com/",
    #     "article_link_pattern": "/",
    #     "base_url": "https://www.timesofisrael.com"
    # },
    "The Independent": {
        "url": "https://www.independent.co.uk/news/world",
        "article_link_pattern": "/news/world/",
        "base_url": "https://www.independent.co.uk",
        # The Independent articles include some kind of article ID
        "article_regex": r"/news/world/[a-z0-9\-]+-[a-z0-9]+"
    },
    "France 24": {
        "url": "https://www.france24.com/en/",
        "article_link_pattern": "/en/",
        "base_url": "https://www.france24.com",
        # France24 articles typically have dates or numbers in them
        # Example: /en/middle-east/20230501-israel-strikes-gaza-after-rocket-fire
        "article_regex": r"/en/[a-z0-9\-]+/\d{8}-[a-z0-9\-]+"
    },
    "Deutsche Welle": {
        "url": "https://www.dw.com/en/middle-east/s-14207", 
        "article_link_pattern": "/en/",
        "base_url": "https://www.dw.com",
        # Deutsche Welle articles have a specific format with an article ID at the end
        # Example: /en/title-goes-here/a-12345678
        "article_regex": r"/en/[a-z0-9\-]+/a-\d+"
    },
    "Jerusalem Post": {
        "url": "https://www.jpost.com/",
        "article_link_pattern": "/",
        "base_url": "https://www.jpost.com",
        # Jerusalem Post URLs typically include a specific article number
        "article_regex": r"/[a-z\-]+/article-\d+"
    },
    "The New Humanitarian": {
        "url": "https://www.thenewhumanitarian.org/",
        "article_link_pattern": "/",
        "base_url": "https://www.thenewhumanitarian.org",
        # The New Humanitarian articles usually have year/month/day
        # Example: /news/2023/05/01/article-title
        "article_regex": r"/news/\d{4}/\d{2}/\d{2}/[a-z0-9\-]+"
    },
    "Foreign Policy": {
        "url": "https://foreignpolicy.com/",
        "article_link_pattern": "/",
        "base_url": "https://foreignpolicy.com",
        # Foreign Policy articles usually have year/month/day
        # Example: /2023/05/01/article-title
        "article_regex": r"/\d{4}/\d{2}/\d{2}/[a-z0-9\-]+"
    }
}

def get_article_links(site_name, site_config, timeout=10, max_retries=3):
    """Scrapes article links with timeout and error handling"""
    print(f"Scraping {site_name}...")
    
    # List of different user agents to try if we get blocked
    user_agents = [
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 Safari/537.36',
        'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15',
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/117.0',
        'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36',
        'Mozilla/5.0 (iPad; CPU OS 17_4 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4 Mobile/15E148 Safari/604.1'
    ]
    
    retry_count = 0
    last_error = None
    
    while retry_count < max_retries:
        # Get a different user agent each retry
        user_agent = user_agents[retry_count % len(user_agents)]
        
        headers = {
            'User-Agent': user_agent,
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Referer': 'https://www.google.com/',
            'Cache-Control': 'no-cache',
            'Pragma': 'no-cache'
        }
        
        try:
            if retry_count > 0:
                print(f"Retry {retry_count + 1}/{max_retries} for {site_name}...")
            
            response = requests.get(
                site_config["url"], 
                headers=headers,
                timeout=timeout,
                allow_redirects=True
            )
            response.raise_for_status()
            
            # If we get here, the request was successful
            soup = BeautifulSoup(response.text, "html.parser")
            
            # First get all links to count them
            all_links = []
            for a in soup.find_all("a", href=True):
                href = a["href"]
                if site_config["article_link_pattern"] in href:
                    full_url = href if href.startswith("http") else f"{site_config['base_url']}{href}"
                    all_links.append(full_url)
            
            print(f"Found {len(all_links)} total links with '{site_config['article_link_pattern']}' on {site_name}")
            
            # if len(all_links) > 0:
            #     sample_size = min(5, len(all_links))
            #     print(f"Sample links from {site_name}:")
            #     for i in range(sample_size):
            #         print(f"  - {all_links[i]}")
            
            links = []
            
            # Special handling for BBC news
            if site_name == "BBC" and "article_regex" in site_config:
                article_regex = re.compile(site_config["article_regex"])
                
                # Find all links
                for a in soup.find_all("a", href=True):
                    href = a["href"]
                    # Check if it matches BBC article pattern
                    if article_regex.search(href):
                        full_url = href if href.startswith("http") else f"{site_config['base_url']}{href}"
                        links.append(full_url)
                
                print(f"Found {len(links)} {site_name} article links matching regex")
            else:
                # Standard pattern matching for other sites
                for a in soup.find_all("a", href=True):
                    href = a["href"]
                    if site_config["article_link_pattern"] in href:
                        full_url = href if href.startswith("http") else f"{site_config['base_url']}{href}"
                        links.append(full_url)
            
            for article_container in soup.find_all(["div", "article", "li"], class_=lambda c: c and any(cls in str(c).lower() for cls in ["article", "story", "news", "post", "entry", "item"])):
                for a in article_container.find_all("a", href=True):
                    href = a["href"]
                    # For BBC, apply the regex check
                    if site_name == "BBC" and "article_regex" in site_config:
                        article_regex = re.compile(site_config["article_regex"])
                        if article_regex.search(href):
                            full_url = href if href.startswith("http") else f"{site_config['base_url']}{href}"
                            links.append(full_url)
                    else:
                        # Standard check for other sites
                        if site_config["article_link_pattern"] in href:
                            full_url = href if href.startswith("http") else f"{site_config['base_url']}{href}"
                            links.append(full_url)
            
            # Remove duplicates and non-articles
            unique_links = []
            seen = set()
            for link in links:
                normalized = normalize_url(link)
                
                # For BBC, double check the article pattern
                if site_name == "BBC" and "article_regex" in site_config:
                    article_regex = re.compile(site_config["article_regex"])
                    if not article_regex.search(link):
                        continue
                        
                if normalized not in seen and site_config["article_link_pattern"] in link:
                    unique_links.append(link)
                    seen.add(normalized)
            
            print(f"Found {len(unique_links)} unique articles on {site_name}")
            
            # If we didn't find any links, let's print part of the HTML for debugging
            if len(unique_links) == 0 and len(all_links) > 0:
                print(f"WARNING: Found {len(all_links)} links matching the pattern but none matched the regex.")
                if site_name == "BBC":
                    print("BBC article regex pattern may need to be updated.")
                    print(f"First 3 links that didn't match the regex pattern:")
                    for i in range(min(3, len(all_links))):
                        print(f"  - {all_links[i]}")
            
            return unique_links
            
        except RequestException as e:
            retry_count += 1
            last_error = e
            print(f"Request error on try {retry_count} for {site_name}: {e}")
            
            # If it's a 403 error, definitely retry with a different user agent
            if hasattr(e, 'response') and e.response is not None and e.response.status_code == 403:
                print(f"Received 403 Forbidden from {site_name}, will retry with different user agent")
            elif retry_count >= max_retries:
                print(f"Max retries ({max_retries}) reached for {site_name}, giving up")
                break
                
            # Wait longer between retries
            time.sleep(retry_count * 2)
            continue
            
        except Exception as e:
            retry_count += 1
            last_error = e
            print(f"Unexpected error on try {retry_count} for {site_name}: {e}")
            
            if retry_count >= max_retries:
                print(f"Max retries ({max_retries}) reached for {site_name}, giving up")
                break
                
            # Wait longer between retries
            time.sleep(retry_count * 2)
            continue
    
    # If we get here, all retries failed
    if isinstance(last_error, Timeout):
        print(f"Timeout while scraping {site_name}, moving to next site")
    elif isinstance(last_error, RequestException):
        print(f"Network error while scraping {site_name}: {last_error}")
    else:
        print(f"Unexpected error scraping {site_name}: {last_error}")
        
    return []

def scrape_article(url, site_name, timeout=10, max_retries=3):
    """Scrapes an article with timeout, retry logic, and error handling"""
    # List of different user agents to try if we get blocked
    user_agents = [
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 Safari/537.36',
        'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15',
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/117.0',
        'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36',
        'Mozilla/5.0 (iPad; CPU OS 17_4 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4 Mobile/15E148 Safari/604.1'
    ]
    
    retry_count = 0
    last_error = None
    
    while retry_count < max_retries:
        # Get a different user agent each retry
        user_agent = user_agents[retry_count % len(user_agents)]
        
        headers = {
            'User-Agent': user_agent,
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Referer': 'https://www.google.com/',
            'Cache-Control': 'no-cache',
            'Pragma': 'no-cache'
        }
        
        try:
            if retry_count > 0:
                print(f"Retry {retry_count}/{max_retries} for article: {url}")
            else:
                print(f"Scraping article: {url}")
            
            response = requests.get(url, headers=headers, timeout=timeout)
            response.raise_for_status()
            soup = BeautifulSoup(response.text, "html.parser")
            
            # Find title with site-specific handling
            title = "No title"
            
            # Special handling for BBC
            if site_name == "BBC":
                # First try to find H1 (article page)
                title_element = soup.find("h1")
                if title_element:
                    title = title_element.text.strip()
                else:
                    # If no H1, try to find main headline in H2 (sometimes BBC uses H2 for headlines)
                    headline_elements = soup.find_all("h2")
                    for h2 in headline_elements:
                        # Look for headlines with significant text length
                        if len(h2.text.strip()) > 20:
                            title = h2.text.strip()
                            break
                # If we still have "No title", try other common headline classes
                if title == "No title":
                    for headline_class in ["article-headline", "story-headline", "headline"]:
                        headline = soup.find(class_=headline_class)
                        if headline:
                            title = headline.text.strip()
                            break
            else:
                # Default behavior for other sites
                title_element = soup.find("h1")
                title = title_element.text.strip() if title_element else "No title"
            
            invalid_titles = {
                "Business", "Climate", "Sport", "Technology", "Entertainment",
                "Analysis", "Art & Design", "Movies", "Opinion", "Review",
                "Menu", "Navigation", "Search"
            }
            
            if (title in invalid_titles or 
                len(title) < 10 or 
                any(section in title for section in ["Section", "Category", "Page"])):
                print(f"Skipping article with invalid title: {title}")
                return None
            
            # Find content with fallbacks for different site structures
            content = ""
            
            # First try to find content in article body containers
            article_containers = soup.find_all(["article", "div", "section"], 
                class_=lambda c: c and any(cls in str(c).lower() for cls in 
                          ["article-body", "article-content", "story-body", "story-content", 
                           "main-content", "entry-content", "post-content"]))
            
            # If we found article containers, extract paragraphs from them
            if article_containers:
                for container in article_containers:
                    container_paragraphs = container.find_all("p")
                    if container_paragraphs:
                        content = " ".join([p.text.strip() for p in container_paragraphs])
                        break
            
            # If we couldn't get content from containers, try the standard method
            if not content or len(content) < 300:
                paragraphs = soup.find_all("p")
                content = " ".join([p.text.strip() for p in paragraphs])
            
            # Last resort: if still not enough content, get all text from the page
            if len(content) < 300:
                print(f"Not enough paragraph content, trying to extract all text")
                # Get all text from the body, excluding scripts and styles
                for script in soup(["script", "style"]):
                    script.extract()
                content = soup.get_text(separator=" ", strip=True)
            
            if len(content) < 300: 
                print(f"Skipping article with insufficient content: {title}")
                return None
            
            print(f"Successfully scraped article: {title} ({len(content)} chars)")    
            return {"url": url, "title": title, "content": content}
            
        except RequestException as e:
            retry_count += 1
            last_error = e
            print(f"Request error on article try {retry_count} for {url}: {e}")
            
            # If it's a 403 error, definitely retry with a different user agent
            if hasattr(e, 'response') and e.response is not None and e.response.status_code == 403:
                print(f"Received 403 Forbidden for article, will retry with different user agent")
            elif retry_count >= max_retries:
                print(f"Max retries ({max_retries}) reached for article, giving up")
                break
                
            # Wait longer between retries
            time.sleep(retry_count * 2)
            continue
            
        except Exception as e:
            retry_count += 1
            last_error = e
            print(f"Unexpected error on article try {retry_count}: {e}")
            
            if retry_count >= max_retries:
                print(f"Max retries ({max_retries}) reached for article, giving up")
                break
                
            # Wait longer between retries
            time.sleep(retry_count * 2)
            continue
    
    # If we get here, all retries failed
    print(f"Error scraping article {url}: {last_error}")
    return None

def search_keywords(article):
    """Checks if article contains any target keywords."""
    if not article:
        return None
        
    content = article["content"].lower()
    title = article["title"].lower()
    
    matches = {kw for kw in KEYWORDS if kw.lower() in title or kw.lower() in content}
    
    if matches:
        print(f"Found keywords {matches} in article: {article['title']}")
        return list(matches)
    return None

def normalize_quote(text):
    text = text.lower().strip()
    text = re.sub(r'\s+', ' ', text)
    #text = re.sub(r'[^\w\s]', '', text)
    return text

def quote_id(text):
    #using hash to avoid long document IDs
    return hashlib.sha256(text.encode('utf-8')).hexdigest()

def store_quotes(quotes, article_info, source):
    """Store new quotes in Firebase using normalized hash as document ID to prevent duplicates"""
    batch = db.batch()
    stored_count = 0

    for quote in quotes:
        norm_quote = normalize_quote(quote)
        if not norm_quote:
            continue
        doc_id = quote_id(norm_quote)
        doc_ref = db.collection(COLLECTION).document(doc_id)
        if doc_ref.get().exists:
            continue  # Already exists, skip
        batch.set(doc_ref, {
            "text": quote.strip(),
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

def normalize_url(url):
    """Normalize URL for consistent tracking"""
    url = url.rstrip('/')
    url = url.split('?')[0]
    url = url.replace('http://', '').replace('https://', '')
    url = url.replace('www.', '')
    return url

def has_been_processed(url, days_threshold=30):
    """Check if URL has been processed in the last X days"""
    normalized_url = normalize_url(url)
    url_hash = hashlib.md5(normalized_url.encode()).hexdigest()
    
    # print(f"Checking if URL has been processed: {url}")
    # print(f"  Normalized URL: {normalized_url}")
    # print(f"  URL hash: {url_hash}")
    
    doc_ref = db.collection("processed_urls").document(url_hash)
    doc = doc_ref.get()
    
    if doc.exists:
        # print(f"  URL was previously processed!")
        return True
    
    # print(f"  URL is new, not previously processed.")
    return False

def mark_as_processed(url, site_name):
    """Mark URL as processed in Firestore"""
    normalized_url = normalize_url(url)
    url_hash = hashlib.md5(normalized_url.encode()).hexdigest()
    
    # print(f"Marking URL as processed: {url}")
    # print(f"  Normalized URL: {normalized_url}")
    # print(f"  URL hash: {url_hash}")
    
    doc_ref = db.collection("processed_urls").document(url_hash)
    doc_ref.set({
        "url": url,
        "normalized_url": normalized_url,
        "site_name": site_name,
        "timestamp": firestore.SERVER_TIMESTAMP
    })
    # print(f"  Successfully marked URL as processed.")

def main():
    quotes_processed = 0
    max_articles_per_site = 1
    max_articles_to_check = 16  
    processed_urls = set()

    # Create the processed_urls collection if it doesn't exist
    try:
        if not db.collection("processed_urls").limit(1).get():
            print("Creating processed_urls collection")
            db.collection("processed_urls").document("init").set({"init": True})
            db.collection("processed_urls").document("init").delete()
    except Exception as e:
        print(f"Error initializing processed_urls collection: {e}")
    
    for site_name, site_config in NEWS_SITES.items():
        try:
            print(f"\nProcessing {site_name}...")
            article_links = get_article_links(site_name, site_config)
            
            if not article_links:
                print(f"No articles found for {site_name}, skipping site")
                continue
                
            print(f"Found {len(article_links)} total articles for {site_name}")
            print(f"Will check up to {len(article_links[:max_articles_to_check])} articles to find {max_articles_per_site} successful ones")
            
            # Track successful articles (those that resulted in quotes being added)
            successful_articles = 0
            # Track articles we've attempted to process
            attempted_articles = 0
            
            articles_to_check = article_links[:max_articles_to_check]
            for link in articles_to_check:
                # Stop if we've processed enough successful articles for this site
                if successful_articles >= max_articles_per_site:
                    print(f"Reached target of {max_articles_per_site} successful articles for {site_name}")
                    break
                
                # Count this as an attempt
                attempted_articles += 1
                
                # Check both in-memory and Firestore for already processed URLs
                if link in processed_urls:
                    print(f"Skipping URL already processed in this run")
                    continue
                
                if has_been_processed(link):
                    print(f"Skipping URL already processed in previous runs")
                    continue
                
                # If we get here, this is a new article
                print(f"Processing new article #{attempted_articles}")
                processed_urls.add(link)
                
                try:
                    time.sleep(2) 
                    
                    # Try to scrape the article
                    article = scrape_article(link, site_name)
                    
                    # Check if article is valid
                    if not article:
                        print(f"Article could not be scraped or has invalid title/content")
                        mark_as_processed(link, site_name)
                        continue  # Skip to next article
                    
                    # Check for keywords
                    matched_terms = search_keywords(article)
                    if not matched_terms:
                        print(f"No relevant keywords found in: {article['title']}")
                        mark_as_processed(link, site_name)
                        continue  # Skip to next article
                    
                    # Try to extract quotes
                    quotes = quote_extractor.extract_quotes(article["content"])
                    if not quotes:
                        print(f"No quotes found in: {article['title']}")
                        mark_as_processed(link, site_name)
                        continue  # Skip to next article
                    
                    # We have quotes! Try to store them
                    article_info = {
                        "url": article["url"],
                        "title": article["title"]
                    }
                    quotes_added = store_quotes(quotes, article_info, site_name)
                    
                    # Check if any quotes were actually added (might all be duplicates)
                    if quotes_added > 0:
                        successful_articles += 1
                        quotes_processed += quotes_added
                        print(f"Success! Found and stored {quotes_added} new quotes from article")
                    else:
                        print(f"No new quotes were added (all were duplicates)")
                    
                    # Mark as processed either way
                    mark_as_processed(link, site_name)
                    
                except Exception as e:
                    print(f"Error processing article: {e}")
                    # Don't mark as processed if there was an exception
                    # to allow retrying in the future
                    continue
            
            # Report results for this site
            print(f"Site {site_name} summary:")
            print(f"  - Attempted to process {attempted_articles} articles")
            print(f"  - Successfully added quotes from {successful_articles} articles")
            print(f"  - Added a total of {quotes_processed} new quotes")
            
            # If we didn't reach our target, explain why
            if successful_articles < max_articles_per_site:
                if attempted_articles >= len(articles_to_check):
                    print(f"  - Couldn't reach target of {max_articles_per_site} successful articles (exhausted all available articles)")
                else:
                    print(f"  - Stopped early due to exceptions")
            
            time.sleep(5)
            
        except Exception as e:
            print(f"Error processing site {site_name}: {e}")
            continue
    
    print(f"\nScript completed. Processed {quotes_processed} new quotes in total.")
    return quotes_processed

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Quote scraper utility')
    args = parser.parse_args()
    main()
