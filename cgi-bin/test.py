#!/usr/bin/env python3

# Output HTTP header
print("Content-Type: text/html")
print()  # Required blank line between headers and body

# Output HTML body
print("""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>CGI Test</title>
</head>
<body>
    <h1>Hello from Python CGI!</h1>
    <p>Your CGI setup is working correctly ✅</p>
</body>
</html>""")
