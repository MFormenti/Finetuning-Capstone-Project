from elasticsearch import Elasticsearch

# Connect to Elasticsearch
es = Elasticsearch(
    hosts=["https://elastic.hpc-crc.duckdns.org"],  # Replace with your Elasticsearch host
    http_auth=("juan.grados", "57fc9e91375496866fd9ef5aabc0bdcc4cc86892"),
    verify_certs=False    # Do not verify SSL certificates
)

# Define the index name
index_name = "crypto_module"

# Define a sample query
query = {
    "query": {
        "multi_match": {
            "query": "How to use Diffie Hellman with SymPy",
            "fields": ["function_name^5", "sections.summary", "docstring"]
        }
    }
}

# Execute the search
response = es.search(index=index_name, body=query)


context_list = []
for hit in response['hits']['hits']:
    if hit['_score'] > 2.0:
        context_list.append(hit['_source'])


# Print out the results
for hit in response['hits']['hits']:
    print(f"Function Name: {hit['_source']['function_name']}")
    print(f"Summary: {hit['_source']['sections'].get('summary', 'No summary available')}")
    print(f'Score : {hit["_score"]}')
    print(f'Source : {hit["_source"]}')
    print("----")
