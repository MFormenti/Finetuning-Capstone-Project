from elasticsearch import Elasticsearch, helpers
import json

es = Elasticsearch(
    hosts=["https://elastic.hpc-crc.duckdns.org"],  # Replace with your Elasticsearch host
    http_auth=("juan.grados", ""),
    verify_certs=False    # Do not verify SSL certificates
)

# Define the index name
index_name = "crypto_module"

# Delete the index if it already exists
if es.indices.exists(index=index_name):
    es.indices.delete(index=index_name)

# Define the mapping for the index
mapping = {
    "mappings": {
        "properties": {
            "function_name": {"type": "text", "analyzer": "whitespace"},
            "docstring": {"type": "text", "analyzer": "standard"},
            "sections": {
                "type": "object",
                "properties": {
                    "summary": {"type": "text", "analyzer": "standard"},
                    "parameters": {"type": "text", "analyzer": "standard"},
                    "returns": {"type": "text", "analyzer": "standard"},
                    "explanation": {"type": "text", "analyzer": "standard"},
                    "related_functions": {"type": "text", "analyzer": "whitespace"},
                    "examples": {"type": "text", "analyzer": "standard"},
                    "notes": {"type": "text", "analyzer": "standard"},
                    "references": {"type": "text", "analyzer": "whitespace"},
                    "see_also": {"type": "text", "analyzer": "standard"}
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
