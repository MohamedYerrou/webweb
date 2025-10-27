#!/usr/bin/php-cgi
<?php
header("Content-Type: text/html; charset=UTF-8");
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>PHP CGI | Hello</title>
    <style>
        body {
            font-family: "Segoe UI", Arial, sans-serif;
            background: radial-gradient(circle at top left, #0d1b2a, #1b263b, #0a192f);
            color: #e0e6ed;
            margin: 0;
            padding: 0;
        }

        .container {
            max-width: 700px;
            margin: 80px auto;
            background: #1b263b;
            border: 2px solid #3a506b;
            border-radius: 15px;
            box-shadow: 0 8px 25px rgba(0,0,0,0.5);
            padding: 2rem;
            animation: fadeIn 0.6s ease;
        }

        h1 {
            text-align: center;
            color: #66c0f4;
            font-size: 2rem;
            margin-bottom: 1rem;
        }

        p.intro {
            text-align: center;
            color: #b8c7d9;
            font-size: 1.1rem;
            margin-bottom: 1.5rem;
        }

        hr {
            border: none;
            border-top: 1px solid #3a506b;
            margin: 1.5rem 0;
        }

        h2 {
            color: #5dade2;
            border-left: 4px solid #66c0f4;
            padding-left: 10px;
            margin-bottom: 0.8rem;
        }

        ul {
            list-style-type: none;
            padding: 0;
        }

        li {
            background: #0f1e33;
            border: 1px solid #2e4a68;
            border-radius: 8px;
            padding: 10px;
            margin: 8px 0;
        }

        li b {
            color: #66c0f4;
        }

        footer {
            text-align: center;
            color: #7a8ca7;
            font-size: 0.9rem;
            margin-top: 2rem;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(15px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* subtle hover glow */
        li:hover {
            background: #132942;
            border-color: #66c0f4;
            transition: all 0.2s ease-in-out;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Hello from PHP CGI</h1>
        <p class="intro">This is a simple CGI script running inside our web server.</p>
        <hr>
        <h2>CGI Information</h2>
        <ul>
            <li><b>Server Software:</b> <?= $_SERVER["SERVER_SOFTWARE"] ?? "Unknown" ?></li>
            <li><b>Request Method:</b> <?= $_SERVER["REQUEST_METHOD"] ?? "Unknown" ?></li>
            <li><b>Server Protocol:</b> <?= $_SERVER["SERVER_PROTOCOL"] ?? "Unknown" ?></li>
        </ul>
        <footer>Webserv PHP CGI © 2025</footer>
    </div>
</body>
</html>
