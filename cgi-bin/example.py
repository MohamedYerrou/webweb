#!/usr/bin/env python3
print("Content-Type: text/html\r\n\r\n")

import os
from datetime import datetime

html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Python CGI | Hello</title>
    <style>
        body {{
            font-family: "Segoe UI", Arial, sans-serif;
            background: radial-gradient(circle at top left, #0d1b2a, #1b263b, #0a192f);
            color: #e0e6ed;
            margin: 0;
            padding: 0;
        }}

        .container {{
            max-width: 700px;
            margin: 80px auto;
            background: #1b263b;
            border: 2px solid #3a506b;
            border-radius: 15px;
            box-shadow: 0 8px 25px rgba(0,0,0,0.5);
            padding: 2rem;
            animation: fadeIn 0.6s ease;
        }}

        h1 {{
            text-align: center;
            color: #66c0f4;
            font-size: 2rem;
            margin-bottom: 1rem;
        }}

        p.intro {{
            text-align: center;
            color: #b8c7d9;
            font-size: 1.1rem;
            margin-bottom: 1.5rem;
        }}

        hr {{
            border: none;
            border-top: 1px solid #3a506b;
            margin: 1.5rem 0;
        }}

        h2 {{
            color: #5dade2;
            border-left: 4px solid #66c0f4;
            padding-left: 10px;
            margin-bottom: 0.8rem;
        }}

        ul {{
            list-style-type: none;
            padding: 0;
        }}

        li {{
            background: #0f1e33;
            border: 1px solid #2e4a68;
            border-radius: 8px;
            padding: 10px;
            margin: 8px 0;
            transition: all 0.2s ease-in-out;
        }}

        li b {{
            color: #66c0f4;
        }}

        li:hover {{
            background: #132942;
            border-color: #66c0f4;
        }}

        footer {{
            text-align: center;
            color: #7a8ca7;
            font-size: 0.9rem;
            margin-top: 2rem;
        }}

        @keyframes fadeIn {{
            from {{ opacity: 0; transform: translateY(15px); }}
            to {{ opacity: 1; transform: translateY(0); }}
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>Hello from Python CGI</h1>
        <p class="intro">This is a simple CGI script running inside your web server.</p>
        <hr>
        <h2>CGI Information</h2>
        <ul>
            <li><b>Server Software:</b> {os.getenv("SERVER_SOFTWARE", "Unknown")}</li>
            <li><b>Request Method:</b> {os.getenv("REQUEST_METHOD", "Unknown")}</li>
            <li><b>Server Protocol:</b> {os.getenv("SERVER_PROTOCOL", "Unknown")}</li>
            <li><b>Server Time:</b> {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}</li>
            <li><b>Gateway Interface:</b> {os.getenv("GATEWAY_INTERFACE", "Unknown")}</li>
            <li><b>Script Filename:</b> {os.getenv("SCRIPT_FILENAME", "Unknown")}</li>
            <li><b>Script Name:</b> {os.getenv("SCRIPT_NAME", "Unknown")}</li>
            <li><b>Server Name:</b> {os.getenv("SERVER_NAME", "Unknown")}</li>
            <li><b>Server Port:</b> {os.getenv("SERVER_PORT", "Unknown")}</li>
            <li><b>Remote Address:</b> {os.getenv("REMOTE_ADDR", "Unknown")}</li>
            <li><b>Content Length:</b> {os.getenv("CONTENT_LENGTH", "0")}</li>
        </ul>
        <footer>Webserv Python CGI © 2025</footer>
    </div>
</body>
</html>
"""

print(html)
