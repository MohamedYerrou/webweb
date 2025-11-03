#!/usr/bin/env python3

import os

print("Content-Type: text/html")
print()

print("<!DOCTYPE html>")
print("<html><head><title>CGI Environment</title></head><body>")
print("<h1>CGI Response from Webserv</h1>")
print("<h2>Environment Variables</h2>")
print("<table border='1' cellpadding='4' cellspacing='0'>")
print("<tr><th>Variable</th><th>Value</th></tr>")

for key, value in sorted(os.environ.items()):
    print(f"<tr><td>{key}</td><td>{value}</td></tr>")

print("</table>")
print("</body></html>")
