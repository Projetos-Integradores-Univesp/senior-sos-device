import requests

url = "http://127.0.0.1:8000/auth/refresh"
response = requests.get(url)
