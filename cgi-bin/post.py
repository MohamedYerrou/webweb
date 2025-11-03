#!/usr/bin/env python3
import os
import sys
import datetime

print("Content-Type: text/plain\n")
print("=== CGI POST Handler Test ===")
print(f"Timestamp: {datetime.datetime.now()}")
print("")

# Read from the stored file via environment variable
post_file = os.environ.get('POST_DATA_FILE')
print(f"POST_DATA_FILE environment variable: {post_file}")

if post_file and os.path.exists(post_file):
    print("✅ POST data file found!")
    
    # Read the POST data from file
    with open(post_file, 'r') as f:
        post_data = f.read()
    
    print(f"📦 POST data content: '{post_data}'")
    print(f"📏 Data length: {len(post_data)} bytes")
    
    # Display all relevant environment variables
    print("\n=== Environment Variables ===")
    print(f"REQUEST_METHOD: {os.environ.get('REQUEST_METHOD', 'Not set')}")
    print(f"CONTENT_LENGTH: {os.environ.get('CONTENT_LENGTH', 'Not set')}")
    print(f"CONTENT_TYPE: {os.environ.get('CONTENT_TYPE', 'Not set')}")
    print(f"SCRIPT_NAME: {os.environ.get('SCRIPT_NAME', 'Not set')}")
    
    # Process the data (example business logic)
    print("\n=== Processing Data ===")
    if post_data:
        # Example: Save to permanent storage
        storage_file = "/tmp/cgi_processed_data.log"
        try:
            with open(storage_file, 'a') as f:
                timestamp = datetime.datetime.now().isoformat()
                f.write(f"[{timestamp}] {post_data}\n")
            print(f"✅ Data saved to permanent storage: {storage_file}")
        except Exception as e:
            print(f"❌ Error saving data: {e}")
        
        # Example: Parse form data if it's URL encoded
        if os.environ.get('CONTENT_TYPE', '').startswith('application/x-www-form-urlencoded'):
            print("🔍 Parsing form data:")
            pairs = post_data.split('&')
            for pair in pairs:
                if '=' in pair:
                    key, value = pair.split('=', 1)
                    print(f"   {key}: {value}")
    
    # Clean up the temporary file
    try:
        os.unlink(post_file)
        print(f"✅ Temporary file cleaned up: {post_file}")
    except Exception as e:
        print(f"⚠️  Warning: Could not delete temp file: {e}")
        
else:
    print("❌ No POST data file found or file doesn't exist")
    print("\n=== Available Environment ===")
    for key in ['REQUEST_METHOD', 'CONTENT_LENGTH', 'CONTENT_TYPE', 'POST_DATA_FILE']:
        print(f"{key}: {os.environ.get(key, 'Not set')}")
    
    # Fallback: try reading from stdin (in case you use Option 2)
    print("\n=== Attempting stdin fallback ===")
    try:
        content_length = int(os.environ.get('CONTENT_LENGTH', 0))
        if content_length > 0:
            post_data = sys.stdin.read(content_length)
            print(f"📦 POST data from stdin: '{post_data}'")
        else:
            print("No data in stdin either")
    except Exception as e:
        print(f"Stdin read failed: {e}")

print("\n=== Test Complete ===")