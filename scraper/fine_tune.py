from openai import OpenAI
import json
import os
from dotenv import load_dotenv
import pandas as pd

load_dotenv()
client = OpenAI()

def convert_ss():
    df = pd.read_csv('training-examples-v0.csv')
    training_examples = []
    
    system_msg = """You evaluate quotes based on three criteria:

1. Emotional Weight (0-2):
- 0: Dry language/technical
- 1: Basic emotional content
- 2: Strong emotional impact

2. Interpretative Space (0-2):
- 0: Too specific to one real world event
- 1: Some ambiguity
- 2: Rich with possible meanings

3. Memorability (0-2):
- 0: Forgettable/generic
- 1: Somewhat memorable
- 2: Haunting or unique

Automatically give a score of 0 to the quote if it container Trump or names other world leaders, as well as country and city names.
Calculate score as (sum of scores)/6
Return only the final score as a number between 0 and 1."""

    for _, row in df.iterrows():
        score = str(round((row['emotional_weight'] + row['interpretative_space'] + row['memorability']) / 6, 2))
        
        example = {
            "messages": [
                {"role": "system", "content": system_msg},
                {"role": "user", "content": f"Evaluate this quote: '{row['quote']}'"},
                {"role": "assistant", "content": score}
            ]
        }
        
        training_examples.append(example)

    with open("training_data.jsonl", "w") as f:
        for example in training_examples:
            f.write(json.dumps(example) + "\n")

# convert_ss()


# file = client.files.create(
#     file=open("training_data.jsonl", "rb"),
#     purpose="fine-tune"
# )
# print(f"File ID: {file.id}")

# job = client.fine_tuning.jobs.create(
#     training_file=file.id,
#     model="gpt-3.5-turbo-0125"
# )
# print(f"Job ID: {job.id}")

def check_job_stat(job_id):
    status = client.fine_tuning.jobs.retrieve(job_id)
    print(f"Status: {status.status}")
    return status

job_id = "ftjob-xpXiEQrYMp8E7i070BHqxnyB"
job = client.fine_tuning.jobs.retrieve(job_id)
status = check_job_stat(job_id)