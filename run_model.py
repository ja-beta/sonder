from openai import OpenAI
from dotenv import load_dotenv
import os

load_dotenv()
client=OpenAI()

def evaluate_quote(quote, model_id):
    try:
        response = client.chat.completions.create(
            model=model_id,
            messages=[
                {"role": "system", "content": "You evaluate quotes based on emotional weight (0-2), interpretative space (0-2), and memorability (0-2). Calculate score as (sum of scores)/6. Return ONLY the final score as a number between 0 and 1, with no explanation."},
                {"role": "user", "content": f"Evaluate this quote: '{quote}'"}
            ]
        )
        score = float(response.choices[0].message.content)
        return score
    except Exception as e:
        print(f"Error: {e}")
        return None
    

def test_quotes():
    model_id = "ft:gpt-3.5-turbo-0125:personal::B2QGxish"

    test_quotes = [
        "I'm still alive, dad",
        "tangible and significant consequences",
        "This is everything I could say at this time",
        "I still think of her every day",
        "Only the best",
        "is an attack on both accountability and free speech.",
        "Elon can't do and won't do anything without our approval, and we'll give him the approval where appropriate. Where it's not appropriate we won't"

    ]

    for quote in test_quotes:
        score = evaluate_quote(quote, model_id)
        print(f"\nQuote: {quote}")
        print(f"Score: {score}")

if __name__ == "__main__":
    test_quotes()