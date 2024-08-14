from elasticsearch import Elasticsearch

# Connect to Elasticsearch
es = Elasticsearch([{'host': 'localhost', 'port': 9200, 'scheme':'http'}])

# Define the index name
index_name = "crypto_module"

# Define a sample query
query = {
    "query": {
        "multi_match": {
            "query": "How to use RSA with SymPy",
            "fields": ["function_name^5", "sections.summary"]
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
#for hit in response['hits']['hits']:
#    print(f"Function Name: {hit['_source']['function_name']}")
#    print(f"Summary: {hit['_source']['sections'].get('summary', 'No summary available')}")
#    print(f'Score : {hit["_score"]}')
#    print(f'Source : {hit["_source"]}')
#    print("----")
