from elasticsearch import Elasticsearch, helpers
import json

# Connect to Elasticsearch
es = Elasticsearch([{'host': 'localhost', 'port': 9200, 'scheme': 'http'}])

# Define the index name
index_name = "crypto_module"

# Delete the index if it already exists
if es.indices.exists(index=index_name):
    es.indices.delete(index=index_name)

# Define the mapping for the index
mapping = {
    "mappings": {
        "properties": {
            "function_name": {"type": "text", "analyzer": "english"},
            "docstring": {"type": "text", "analyzer": "english"},
            "sections": {
                "type": "object",
                "properties": {
                    "summary": {"type": "text", "analyzer": "english"},
                    "parameters": {"type": "text", "analyzer": "english"},
                    "returns": {"type": "text", "analyzer": "english"},
                    "explanation": {"type": "text", "analyzer": "english"},
                    "related_functions": {"type": "text", "analyzer": "english"},
                    "examples": {"type": "text", "analyzer": "english"},
                    "notes": {"type": "text", "analyzer": "english"},
                    "references": {"type": "text", "analyzer": "english"},
                    "see_also": {"type": "text", "analyzer": "english"}
                }
            }
        }
    }
}

# Create the index with the defined mapping
es.indices.create(index=index_name, body=mapping)

# Load the JSON corpus
with open('crypto_corpus.json', 'r') as f:
    corpus = json.load(f)

# Prepare the data for bulk indexing
actions = [
    {
        "_index": index_name,
        "_source": function
    }
    for function in corpus
]

# Bulk index the data
helpers.bulk(es, actions)

print(f"Successfully indexed {len(corpus)} documents.")
