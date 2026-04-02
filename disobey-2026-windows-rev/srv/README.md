# Server

Small REST server that validates keys using the same logic as the runtime.

## Run locally

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
$env:FLAG = "FLAG{example}"
python app.py
```

### Request

```powershell
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8000/check -ContentType "application/json" -Body '{"key":"DIS01-23456-789AB-CDEF0"}'
```

## Docker

Build:

```powershell
docker build -t disobey-srv .
```

Run:

```powershell
docker run --rm -p 8000:8000 -e FLAG="FLAG{example}" disobey-srv
```
