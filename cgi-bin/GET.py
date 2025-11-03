#!/usr/bin/env python3
import os
import sys
import urllib.parse

print("Content-Type: text/plain\n")

method = os.environ.get('REQUEST_METHOD', 'GET')

if method == 'GET':
    print("=== Handling GET Request ===")
    query_string = os.environ.get('QUERY_STRING', '')
    if query_string:
        params = urllib.parse.parse_qs(query_string)
        for key, values in params.items():
            print(f"GET {key}: {values[0]}")
    
elif method == 'POST':
    print("=== Handling POST Request ===")
    # Use your file-based approach
    post_file = os.environ.get('POST_DATA_FILE')
    if post_file and os.path.exists(post_file):
        with open(post_file, 'r') as f:
            post_data = f.read()
        print(f"POST data: {post_data}")
        # Clean up
        os.unlink(post_file)
    
else:
    print(f"Unknown method: {method}")