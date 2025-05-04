import os
from dotenv import load_dotenv

load_dotenv()

QUOTES_COLLECTION = os.getenv("QUOTES_COLLECTION", "quotes_v7")

KEYWORDS = [
    "war", "conflict", "battle", "hostage", "hostages"
]

def update_collection(new_collection):
    global QUOTES_COLLECTION
    QUOTES_COLLECTION = new_collection
    return QUOTES_COLLECTION

MODEL_ID = "ft:gpt-3.5-turbo-0125:personal::B2QGxish"
