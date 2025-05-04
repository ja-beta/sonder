from openai import OpenAI
from dotenv import load_dotenv
from config import QUOTES_COLLECTION, MODEL_ID
from firebase_init import db
from run_model import evaluate_quote
import os
import re
import time

load_dotenv()
client = OpenAI()

COLLECTION_NAME = QUOTES_COLLECTION

def clean_quote(quote):
    quote = re.sub(r'^[\'""]|[\'""]$', '', quote)
    quote = re.sub(r',$', '', quote)
    quote = quote.strip()
    return quote

def process_quotes(batch_size=100):
    """Process ALL unscored quotes in batches"""
    total_processed = 0
    
    while True:  
        try:
            quotes_ref = db.collection(QUOTES_COLLECTION)\
                .where("processed", "==", False)\
                .where("score", "==", None)\
                .limit(batch_size)
            
            quotes = quotes_ref.get()
            
            if not quotes:
                break
                
            batch = db.batch()
            batch_processed = 0
            
            for doc in quotes:
                try:
                    quote_data = doc.to_dict()
                    quote_text = clean_quote(quote_data['text'])
                    score = evaluate_quote(quote_text, MODEL_ID)
                    
                    if score is not None:
                        batch.update(doc.reference, {
                            "text": quote_text,
                            "score": score,
                            "processed": True
                        })
                        batch_processed += 1
                        print(f"\nProcessed quote: {quote_text}")
                        print(f"Score: {score}")
                    else:
                        print(f"Skipped quote (evaluate_quote returned None): {quote_text}")
                    
                    if batch_processed % 500 == 0:
                        batch.commit()
                        batch = db.batch()
                        time.sleep(1)  
                        
                except Exception as e:
                    print(f"Error processing quote: {quote_text if 'quote_text' in locals() else 'Unknown'}")
                    print(f"Error details: {e}")
                    continue
            
            if batch_processed % 500 != 0:
                batch.commit()
            
            total_processed += batch_processed
            print(f"\nBatch complete. Total processed so far: {total_processed}")
            
            time.sleep(2)
            
        except Exception as e:
            print(f"Error in batch processing: {e}")
            continue
    
    print(f"\nAll done! Total quotes processed: {total_processed}")
    return total_processed

def main():
    return process_quotes()

if __name__ == "__main__":
    main()

