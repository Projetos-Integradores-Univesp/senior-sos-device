import requests

url = "http://127.0.0.1:8000/auth/refresh"
response = requests.get(url)

refresh_token = "colar refresh_token aqui..."

headers = {
    "Authorization": f"Bearer {refresh_token}",
    "Content-Type": "application/json",
}

response = requests.get(url, headers=headers)
print(response.json())
