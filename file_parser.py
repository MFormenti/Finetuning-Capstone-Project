import ast
import json
import re


def extract_docstring_sections(docstring):
    sections = {
        "summary": "",
        "parameters": "",
        "returns": "",
        "explanation": "",
        "examples": "",
        "notes": "",
        "related_functions": "",
        "references": "",
        "see_also": ""
    }

    current_section = "summary"
    for line in docstring.splitlines():
        line = line.strip()
        if not line:
            continue
        if all([char == "=" for char in line]):
            continue
        if re.match(r"Parameters", line):
            current_section = "parameters"
        elif re.match(r"Explanation", line):
            current_section = "explanation"
        elif re.match(r"Returns", line):
            current_section = "returns"
        elif re.match(r"Examples", line):
            current_section = "examples"
        elif re.match(r"Notes", line):
            current_section = "notes"
        elif re.match(r"References", line):
            current_section = "references"
        elif re.match(r"See Also", line):
            current_section = "see_also"
        else:
            pattern = r".*import\s+((?:\w+\s*,\s*)*\w+)"
            match = re.match(pattern, line)
            if match:
                # Extract the list of function names
                functions = match.group(1)
                # Split the function names by comma and strip any extra whitespace
                function_list = [func.strip() for func in functions.split(',')]
                sections['related_functions'] += ', '.join(function_list)
            sections[current_section] += line + " "

    return sections


def parse_function(node):
    function_info = {
        "function_name": node.name,
        "docstring": "",
        "sections": {}
    }

    if ast.get_docstring(node):
        docstring = ast.get_docstring(node)
        sections = extract_docstring_sections(docstring)
        function_info["docstring"] = sections["summary"].strip()
        function_info["sections"] = {k: v.strip() for k, v in sections.items() if v.strip()}

    return function_info


def parse_module(filepath):
    with open(filepath, "r") as source:
        tree = ast.parse(source.read())

    functions = []

    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef):
            function_info = parse_function(node)
            functions.append(function_info)

    return functions


def save_to_json(data, filename):
    with open(filename, 'w') as f:
        json.dump(data, f, indent=4)


# Example usage
filepath = "crypto.py"  # Replace with the actual path to the crypto module
parsed_data = parse_module(filepath)
save_to_json(parsed_data, "crypto_corpus.json")
