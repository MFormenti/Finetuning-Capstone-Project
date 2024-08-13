import ollama

messages = [
    {
        'role': 'user',
        'content': 'Create a python requests instance',
    }
]
response = ollama.chat(model='codellama:7b', messages=messages)
print(response['message']['content'])
